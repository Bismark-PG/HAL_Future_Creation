#include "HALMovementAreaReceiver.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"
#include "HALMovementAttributeSet.h"
#include "HALMovementAreaEffects.h"
#include "HALMovementEffectBridge.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarHALMovementAreaLog(TEXT("hal.MovementAreaLog"), 0,
	TEXT("Selected area source count, target multipliers and recovery. 0=off,1=on."));

UHALMovementAreaReceiver::UHALMovementAreaReceiver() { PrimaryComponentTick.bCanEverTick = false; }
void UHALMovementAreaReceiver::Bind(UHALAbilitySystemComponent* InASC)
{
	if (!GetOwner()->HasAuthority() || !InASC || InASC->GetAvatarActor() != GetOwner() || !InASC->GetSet<UHALMovementAttributeSet>()) { return; }
	if (ASC == InASC) { return; }
	Unbind(); ASC = InASC; Rebuild();
}
void UHALMovementAreaReceiver::Unbind()
{
	if (ASC)
	{
		// Handles belong to this receiver only; never remove another region, boost or replacement Avatar's GE.
		ASC->RemoveActiveGameplayEffect(SlowHandle); ASC->RemoveActiveGameplayEffect(SlipperyHandle);
	}
	ASC = nullptr; SlowHandle = {}; SlipperyHandle = {}; AppliedScales = FVector::OneVector;
	if (auto* Bridge = GetOwner()->FindComponentByClass<UHALMovementEffectBridge>()) { Bridge->SetAreaSway({}); Bridge->SetDragDrive({}); }
	Selection = {};
	bAppliedDrag = bAppliedSlippery = false;
}
bool UHALMovementAreaReceiver::RegisterArea(AActor* Source, const FHALMovementAreaConfig& Config)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Source) || Source->GetWorld() != GetWorld() || !Config.IsValid()) { return false; }
	if (const auto* Existing = Sources.Find(Source); Existing && *Existing == Config) { return true; }
	Sources.Add(Source, Config);
	Source->OnEndPlay.AddUniqueDynamic(this, &ThisClass::SourceEnded);
	Rebuild(); return true;
}
void UHALMovementAreaReceiver::UnregisterArea(AActor* Source)
{
	if (!GetOwner()->HasAuthority() || !Source || Sources.Remove(Source) == 0) { return; }
	Source->OnEndPlay.RemoveDynamic(this, &ThisClass::SourceEnded);
	Rebuild();
}
void UHALMovementAreaReceiver::SourceEnded(AActor* Actor, EEndPlayReason::Type Reason) { UnregisterArea(Actor); }
void UHALMovementAreaReceiver::Rebuild()
{
	FHALMovementAreaSelection Next;
	uint32 SelectedDriveId = MAX_uint32;
	for (auto It = Sources.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) { It.RemoveCurrent(); }
		else
		{
			const auto& Config = It.Value(); Next.Include(Config);
			const auto& Field = Config.DragDrive;
			const uint32 Id = It.Key()->GetUniqueID();
			if (Config.Kind == EHALMovementAreaKind::Drag && Config.LateralGripMultiplier < 1 && Field.Source.IsValid()
				&& (Field.Strength > Next.DragDrive.Strength || (Field.Strength == Next.DragDrive.Strength && Id < SelectedDriveId)))
			{ Next.DragDrive = Field; SelectedDriveId = Id; }
		}
	}
	FVector Recovery = FVector::ZeroVector;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		// Leaving a winning source retains its recovery; equal-strength ordering is deterministic.
		if (Selection.Scales[Index] != Next.Scales[Index])
		{ Recovery[Index] = FMath::Max(Selection.RecoverySeconds[Index], Next.RecoverySeconds[Index]); }
	}
	if (auto* Bridge = GetOwner()->FindComponentByClass<UHALMovementEffectBridge>())
	{
		// Removing Drag restores the remaining environment immediately, even if its source normally fades.
		if (Selection.bHasDrag && !Next.bHasDrag) { Recovery.Z = 0; }
		Bridge->SetAreaRecoverySeconds(Recovery);
		Bridge->SetAreaSway(Next.Sway);
		Bridge->SetDragDrive(Next.DragDrive);
	}
	Selection = Next;
	UpdateEffects();
	if (CVarHALMovementAreaLog.GetValueOnGameThread())
	{
		UE_LOG(LogTemp, Log, TEXT("Movement areas: Vehicle=%s Sources=%d TargetAccel=%.3f TargetSpeed=%.3f TargetGrip=%.3f Sway=%.2f Recovery=%.3f/%.3f/%.3f Bound=%d"),
			*GetNameSafe(GetOwner()), Sources.Num(), Selection.Scales.X, Selection.Scales.Y, Selection.Scales.Z,
			Selection.Sway.Amplitude, Recovery.X, Recovery.Y, Recovery.Z, ASC != nullptr);
	}
}
void UHALMovementAreaReceiver::UpdateEffects()
{
	if (!ASC || ASC->GetAvatarActor() != GetOwner()) { return; }
	const FVector Target = Selection.Scales;
	const bool bSlowChanged = Target.X != AppliedScales.X || Target.Y != AppliedScales.Y;
	const bool bGripChanged = Target.Z != AppliedScales.Z || Selection.bHasDrag != bAppliedDrag || Selection.bHasSlippery != bAppliedSlippery;
	if (bSlowChanged)
	{
		ASC->RemoveActiveGameplayEffect(SlowHandle); SlowHandle = {};
		if (Target.X < 1 || Target.Y < 1)
		{
			auto Context = ASC->MakeEffectContext(); Context.AddSourceObject(this);
			auto Spec = ASC->MakeOutgoingSpec(UHALSlowAreaEffect::StaticClass(), 1, Context);
			Spec.Data->SetSetByCallerMagnitude(HALGameplayTags::Data_Movement_AreaAcceleration, float(Target.X - 1));
			Spec.Data->SetSetByCallerMagnitude(HALGameplayTags::Data_Movement_AreaSpeed, float(Target.Y - 1));
			SlowHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
			if (!SlowHandle.IsValid()) { UE_LOG(LogTemp, Error, TEXT("Movement area slow GE rejected on %s."), *GetNameSafe(GetOwner())); }
		}
	}
	if (bGripChanged)
	{
		ASC->RemoveActiveGameplayEffect(SlipperyHandle); SlipperyHandle = {};
		if (Target.Z < 1)
		{
			auto Context = ASC->MakeEffectContext(); Context.AddSourceObject(this);
			const TSubclassOf<UGameplayEffect> GripEffect = Selection.bHasSlippery
				? UHALSlipperyAreaEffect::StaticClass() : UHALDragAreaEffect::StaticClass();
			auto Spec = ASC->MakeOutgoingSpec(GripEffect, 1, Context);
			if (Selection.bHasDrag && Selection.bHasSlippery) { Spec.Data->DynamicGrantedTags.AddTag(HALGameplayTags::State_Movement_Drag); }
			Spec.Data->SetSetByCallerMagnitude(HALGameplayTags::Data_Movement_AreaGrip, float(Target.Z - 1));
			SlipperyHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
			if (!SlipperyHandle.IsValid()) { UE_LOG(LogTemp, Error, TEXT("Movement area grip GE rejected on %s."), *GetNameSafe(GetOwner())); }
		}
	}
	AppliedScales = Target;
	bAppliedDrag = Selection.bHasDrag; bAppliedSlippery = Selection.bHasSlippery;
	if (bSlowChanged || bGripChanged) { ASC->GetOwner()->ForceNetUpdate(); }
}
void UHALMovementAreaReceiver::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearForOwnerExit(); Super::EndPlay(Reason);
}
void UHALMovementAreaReceiver::ClearForOwnerExit()
{
	for (const auto& Pair : Sources) { if (auto* Source = Pair.Key.Get()) { Source->OnEndPlay.RemoveDynamic(this, &ThisClass::SourceEnded); } }
	Sources.Reset(); Selection = {}; Unbind();
}
