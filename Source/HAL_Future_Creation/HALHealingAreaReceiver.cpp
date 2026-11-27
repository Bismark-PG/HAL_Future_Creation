#include "HALHealingAreaReceiver.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"
#include "HALHealthAttributeSet.h"
#include "HALExpandingHealingArea.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarHALHealingAreaLog(TEXT("hal.HealingAreaLog"), 0,
	TEXT("Log authority healing-area sources and selected periodic GE. 0=off,1=on."));

UHALHealingAreaReceiver::UHALHealingAreaReceiver() { PrimaryComponentTick.bCanEverTick = false; }

bool UHALHealingAreaReceiver::CanHeal() const
{
	return GetOwner()->HasAuthority() && !GetOwner()->IsActorBeingDestroyed() && ASC
		&& ASC->GetAvatarActor() == GetOwner() && ASC->IsHealthInitialized()
		&& ASC->GetSet<UHALHealthAttributeSet>() && ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute()) > 0;
}

void UHALHealingAreaReceiver::Bind(UHALAbilitySystemComponent* InASC)
{
	if (!GetOwner()->HasAuthority() || !InASC || InASC->GetAvatarActor() != GetOwner()
		|| !InASC->GetSet<UHALHealthAttributeSet>()) { return; }
	if (ASC == InASC) { Rebuild(); return; }
	Unbind(); ASC = InASC;
	HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute())
		.AddUObject(this, &ThisClass::HealthChanged);
	Rebuild();
}

void UHALHealingAreaReceiver::Unbind()
{
	if (ASC)
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute()).Remove(HealthChangedHandle);
		ASC->RemoveActiveGameplayEffect(EffectHandle);
	}
	ASC = nullptr; EffectHandle = {}; HealthChangedHandle.Reset();
}

bool UHALHealingAreaReceiver::RegisterArea(AActor* Source, const FHALHealingAreaRecipe& Recipe)
{
	if (!CanHeal() || !IsValid(Source) || Source->IsActorBeingDestroyed() || Source->GetWorld() != GetWorld() || !Recipe.IsValid()) { return false; }
	Sources.Add(Source, Recipe);
	Source->OnEndPlay.AddUniqueDynamic(this, &ThisClass::SourceEnded);
	// Also repairs a removed GE on the next area sample; equal sources never refresh its timer.
	Rebuild(); return true;
}

void UHALHealingAreaReceiver::UnregisterArea(AActor* Source)
{
	if (!GetOwner()->HasAuthority() || !Source || !Sources.Remove(Source)) { return; }
	Source->OnEndPlay.RemoveDynamic(this, &ThisClass::SourceEnded); Rebuild();
}

void UHALHealingAreaReceiver::Rebuild()
{
	const FHALHealingAreaRecipe* Best = nullptr;
	for (auto It = Sources.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Key()->IsActorBeingDestroyed()) { It.RemoveCurrent(); continue; }
		const auto& Recipe = It.Value();
		// Compare rates in double precision, then prefer the shorter period for equal rates.
		const double Rate = double(Recipe.HealPerTick) / Recipe.PeriodSeconds;
		const double BestRate = Best ? double(Best->HealPerTick) / Best->PeriodSeconds : -1.;
		if (!Best || Rate > BestRate || (Rate == BestRate && Recipe.PeriodSeconds < Best->PeriodSeconds)) { Best = &Recipe; }
	}
	if (!CanHeal() || !Best)
	{
		if (ASC) { ASC->RemoveActiveGameplayEffect(EffectHandle); }
		EffectHandle = {}; return;
	}
	if (ASC->GetActiveGameplayEffect(EffectHandle) && AppliedRecipe == *Best) { return; }
	ASC->RemoveActiveGameplayEffect(EffectHandle); EffectHandle = {};
	auto Context = ASC->MakeEffectContext(); Context.AddSourceObject(this);
	auto Spec = ASC->MakeOutgoingSpec(UHALHealingAreaEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid()) { return; }
	Spec.Data->Period = Best->PeriodSeconds;
	Spec.Data->SetSetByCallerMagnitude(HALGameplayTags::Data_Healing_AreaAmount, Best->HealPerTick);
	AppliedRecipe = *Best;
	EffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	if (!EffectHandle.IsValid()) { UE_LOG(LogTemp, Error, TEXT("Healing area GE rejected on %s."), *GetNameSafe(GetOwner())); }
	if (CVarHALHealingAreaLog.GetValueOnGameThread())
	{
		UE_LOG(LogTemp, Log, TEXT("Healing areas: Avatar=%s Sources=%d HPPerTick=%.2f Period=%.2fs Active=%d"),
			*GetNameSafe(GetOwner()), Sources.Num(), AppliedRecipe.HealPerTick, AppliedRecipe.PeriodSeconds, EffectHandle.IsValid());
	}
}

void UHALHealingAreaReceiver::HealthChanged(const FOnAttributeChangeData& Data)
{
	// Synchronous death cleanup prevents a queued periodic pulse from reviving the eliminated Avatar.
	if (Data.NewValue <= 0.f) { ClearForOwnerExit(); }
#if !UE_BUILD_SHIPPING
	// Attribute delegates carry the actual clamped HP delta and the GE that caused it.
	// Another skill's heal, base-value reset or MaxHP change must not look like an area pulse.
	if (!CanHeal() || Data.Attribute != UHALHealthAttributeSet::GetCurrentHPAttribute()
		|| Data.NewValue <= Data.OldValue || !Data.GEModData
		|| Data.GEModData->EffectSpec.GetContext().GetSourceObject() != this
		|| !Data.GEModData->EffectSpec.Def || !Data.GEModData->EffectSpec.Def->IsA<UHALHealingAreaEffect>()) { return; }
	AHALExpandingHealingArea* DebugArea = nullptr;
	for (const auto& Pair : Sources)
	{
		auto* Area = Cast<AHALExpandingHealingArea>(Pair.Key.Get());
		if (IsValid(Area) && !Area->IsActorBeingDestroyed() && Area->IsDebugDrawEnabled()
			&& Area->GetCurrentRadius() > 0 && (!DebugArea || Area->GetUniqueID() < DebugArea->GetUniqueID())) { DebugArea = Area; }
	}
	// One broadcaster even when several debug-enabled areas overlap.
	if (DebugArea)
	{
		DebugArea->NotifyDebugHealing(GetOwner(), Data.NewValue - Data.OldValue, Data.NewValue,
			ASC->GetNumericAttribute(UHALHealthAttributeSet::GetMaxHPAttribute()));
	}
#endif
}
void UHALHealingAreaReceiver::SourceEnded(AActor* Actor, EEndPlayReason::Type Reason) { UnregisterArea(Actor); }
void UHALHealingAreaReceiver::ClearForOwnerExit()
{
	for (const auto& Pair : Sources) { if (auto* Source = Pair.Key.Get()) { Source->OnEndPlay.RemoveDynamic(this, &ThisClass::SourceEnded); } }
	Sources.Reset(); Unbind();
}
void UHALHealingAreaReceiver::EndPlay(const EEndPlayReason::Type Reason) { ClearForOwnerExit(); Super::EndPlay(Reason); }
