#include "HALVehicleStatusFXComponent.h"
#include "AbilitySystemComponent.h"
#include "HALVehicleStatusFXSubsystem.h"
#include "HALVehicleStatusFXDefinition.h"
#include "NiagaraComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHALStatusFXRuntime, Log, All);

namespace
{
#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> StatusFXLog(TEXT("hal.StatusFXLog"), 0, TEXT("Log local status-FX source/binding changes; no per-frame logging."));
#endif
	bool IsVisualTag(FGameplayTag Tag)
	{
		const auto Root = FGameplayTag::RequestGameplayTag(TEXT("State.Visual"), false);
		return Tag.IsValid() && Tag != Root && Tag.MatchesTag(Root);
	}
	const TCHAR* PhaseName(EHALStatusFXPhase Phase)
	{
		switch (Phase)
		{
		case EHALStatusFXPhase::Waiting: return TEXT("Waiting");
		case EHALStatusFXPhase::Active: return TEXT("Active");
		case EHALStatusFXPhase::Fading: return TEXT("Fading");
		case EHALStatusFXPhase::Residue: return TEXT("Residue");
		}
		return TEXT("Unknown");
	}
}

bool UHALVehicleStatusFXComponent::HasValidObserver() const
{
	return ObservedASC.IsValid() && ObservedASC->GetAvatarActor() == GetOwner() && AvatarIdentity == ObservedASC.Get();
}

void UHALVehicleStatusFXComponent::DetachObserver()
{
	if (ObservedASC.IsValid())
	{
		ObservedASC->RegisterGameplayTagEvent(FGameplayTag::RequestGameplayTag(TEXT("State.Visual"), false),
			EGameplayTagEventType::AnyCountChange).Remove(TagsChanged);
	}
	TagsChanged.Reset(); ObservedASC.Reset(); ObservedSources.Reset(); bObservingASC = false;
}

bool UHALVehicleStatusFXComponent::ObserveAbilitySystem(UAbilitySystemComponent* InASC)
{
	check(IsInGameThread());
	if (!bBound || !IsValid(InASC) || InASC->GetAvatarActor() != GetOwner() || AvatarIdentity != InASC) { return false; }
	if (bObservingASC && (ObservedASC != InASC || !TagsChanged.IsValid())) { return false; } // Replace through Bind, which revokes the old generation.
	if (!bObservingASC)
	{
		ObservedASC = InASC; bObservingASC = true;
		const FGuid Binding = Runtime.GetBinding();
		const TWeakObjectPtr<UAbilitySystemComponent> Expected(InASC);
		// Observe the root's ANY count change: a parent can become explicitly owned while an existing
		// child keeps its aggregate count positive, so NewOrRemoved/generic events would miss it.
		TagsChanged = InASC->RegisterGameplayTagEvent(FGameplayTag::RequestGameplayTag(TEXT("State.Visual"), false),
			EGameplayTagEventType::AnyCountChange).AddWeakLambda(this,
			[this, Binding, Expected](const FGameplayTag, int32)
			{
				if (!bBound || Binding != Runtime.GetBinding() || ObservedASC != Expected) { return; }
				if (!HasValidObserver()) { Unbind(); return; }
				ReconcileObservedTags();
			});
	}
	// Subscribe before snapshotting; state can already exist when Avatar/PlayerState replication finishes.
	ReconcileObservedTags(); return true;
}

void UHALVehicleStatusFXComponent::ReconcileObservedTags()
{
	check(IsInGameThread());
	if (!bBound || !bObservingASC || bReconcilingTags) { return; }
	if (!HasValidObserver()) { Unbind(); return; }
	bool bChanged = false;
	{
	TGuardValue<bool> Guard(bReconcilingTags, true);
	FGameplayTagContainer Owned; ObservedASC->GetOwnedGameplayTags(Owned);
	TArray<FGameplayTag> Removed;
	for (const auto& Pair : ObservedSources) { if (!Owned.HasTagExact(Pair.Key)) { Removed.Add(Pair.Key); } }
	for (const auto Tag : Removed)
	{
		const auto Source = ObservedSources.FindChecked(Tag); ObservedSources.Remove(Tag);
		RemoveSource(Source); bChanged = true;
	}
	for (const auto Tag : Owned)
	{
		// Only explicitly owned tags are actual sources; an implied parent is not another state.
		if (!IsVisualTag(Tag) || ObservedSources.Contains(Tag)) { continue; }
		const auto Source = AcquireSource();
		ObservedSources.Add(Tag, Source);
		if (!Submit(Source, Tag)) { ObservedSources.Remove(Tag); RemoveSource(Source); }
		else { bChanged = true; }
	}
	}
	// Reconcile the entire snapshot before any recipe/resource refresh. Otherwise an intermediate
	// removal could instantiate another already-ended pending tag and incorrectly give it a tail.
	if (bChanged) { Refresh(); LogStateChange(TEXT("ASC tags")); }
}

#if !UE_BUILD_SHIPPING
bool UHALVehicleStatusFXComponent::SetDebugSource(FName Name, FGameplayTag Tag, bool bActive)
{
	check(IsInGameThread());
	Refresh();
	if (!bBound || Name.IsNone() || Name.ToString().Len() > 64 || !IsVisualTag(Tag)) { return false; }
	if (const auto* Existing = DebugSources.Find(Name))
	{
		const auto* Demand = Demands.FindByPredicate([&](const auto& D) { return D.Source == *Existing; });
		if (!Demand || Demand->Tag != Tag) { return false; }
		if (bActive) { return Submit(*Existing, Tag); }
		const auto Source = *Existing; DebugSources.Remove(Name);
		const bool Result = RemoveSource(Source); LogStateChange(TEXT("Debug remove")); return Result;
	}
	if (!bActive) { return true; } // Repeated off is idempotent.
	if (DebugSources.Num() >= 16) { return false; }
	const auto Source = AcquireSource(); DebugSources.Add(Name, Source);
	if (!Submit(Source, Tag)) { DebugSources.Remove(Name); RemoveSource(Source); return false; }
	LogStateChange(TEXT("Debug add")); return true;
}

void UHALVehicleStatusFXComponent::ClearDebugSources()
{
	check(IsInGameThread());
	const auto Previous = MoveTemp(DebugSources); DebugSources.Reset();
	for (const auto& Pair : Previous) { RemoveSource(Pair.Value); }
	if (!Previous.IsEmpty()) { LogStateChange(TEXT("Debug clear")); }
}
#endif

FHALStatusFXDiagnostics UHALVehicleStatusFXComponent::CaptureDiagnostics()
{
	check(IsInGameThread());
	Refresh();
	FHALStatusFXDiagnostics D;
	D.Binding = Runtime.GetBinding(); D.MeshGeneration = MeshGeneration; D.bBound = bBound;
	D.bObservingASC = bObservingASC && HasValidObserver(); D.bTickEnabled = IsComponentTickEnabled();
	D.Leases = Runtime.GetSourceLeaseCount(); D.ActiveSources = Runtime.GetActiveSourceCount();
	D.ASCSources = ObservedSources.Num(); D.DebugSources = DebugSources.Num();
	D.Targets = Targets.Targets.Num(); D.Materials = Surfaces.Num(); D.Particles = Particles.Num();
	D.Instances = Runtime.GetSnapshots();
	for (const auto& P : Particles)
	{
		if (!IsValid(P.Component)) { continue; }
		D.RegisteredParticles += P.Component->IsRegistered() ? 1 : 0;
		D.ActiveParticles += P.Component->IsActive() ? 1 : 0;
	}
	const double Time = Now();
	for (const auto& Demand : Demands)
	{
		FHALStatusFXSourceDiagnostic S; S.Tag = Demand.Tag; S.Kind = TEXT("External");
		S.AgeSeconds = FMath::Max(0.0, Time - Demand.ObservedAt); S.bSubmitted = Demand.bSubmitted;
		if (const auto* Source = ObservedSources.Find(Demand.Tag); Source && *Source == Demand.Source)
		{ S.Kind = TEXT("ASC"); S.Name = Demand.Tag.GetTagName(); }
		for (const auto& Pair : DebugSources) { if (Pair.Value == Demand.Source) { S.Kind = TEXT("Debug"); S.Name = Pair.Key; break; } }
		D.PendingRecipes += Demand.bSubmitted ? 0 : 1; D.Sources.Add(MoveTemp(S));
	}
	return D;
}

void UHALVehicleStatusFXComponent::LogStateChange(const TCHAR* Reason)
{
#if !UE_BUILD_SHIPPING
	if (StatusFXLog.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogHALStatusFXRuntime, Display, TEXT("StatusFX Change=%s Vehicle=%s Binding=%s ASC=%d Debug=%d Leases=%d Instances=%d MID=%d Niagara=%d"),
			Reason, *GetNameSafe(GetOwner()), *Runtime.GetBinding().ToString(), ObservedSources.Num(), DebugSources.Num(),
			Runtime.GetSourceLeaseCount(), Runtime.GetInstanceCount(), Surfaces.Num(), Particles.Num());
	}
#endif
}

void UHALVehicleStatusFXComponent::LogDiagnostics()
{
	const auto D = CaptureDiagnostics();
	const auto* Pawn = Cast<APawn>(GetOwner()); const auto* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
	FString Error;
	const auto Catalog = UHALVehicleStatusFXSubsystem::SelectCatalog(Vehicle.Get());
	const auto Load = Cache.IsValid() ? Cache->GetCatalogState(Catalog, Error) : EHALStatusFXLoadState::Unconfigured;
	UE_LOG(LogHALStatusFXRuntime, Display, TEXT("StatusFX Vehicle=%s PlayerId=%d LocalRole=%d Local=%d Bound=%d Observer=%d ASC=%s Avatar=%s Binding=%s MeshGen=%llu Tick=%d Catalog=%s Load=%s Error=%s"),
		*GetNameSafe(GetOwner()), PS ? PS->GetPlayerId() : INDEX_NONE, GetOwner() ? int32(GetOwner()->GetLocalRole()) : 0,
		Pawn && Pawn->IsLocallyControlled(), D.bBound, D.bObservingASC, *GetNameSafe(ObservedASC.Get()),
		*GetNameSafe(ObservedASC.IsValid() ? ObservedASC->GetAvatarActor() : nullptr), *D.Binding.ToString(), D.MeshGeneration,
		D.bTickEnabled, *Catalog.ToString(), *StaticEnum<EHALStatusFXLoadState>()->GetNameStringByValue(int64(Load)), *Error);
	UE_LOG(LogHALStatusFXRuntime, Display, TEXT("StatusFX Counts Leases=%d SubmittedSources=%d ASC=%d Debug=%d PendingRecipes=%d Instances=%d Targets=%d MID=%d Niagara=%d Registered=%d Active=%d Rendering=PreparedDormant"),
		D.Leases, D.ActiveSources, D.ASCSources, D.DebugSources, D.PendingRecipes, D.Instances.Num(), D.Targets,
		D.Materials, D.Particles, D.RegisteredParticles, D.ActiveParticles);
	for (const auto& S : D.Sources)
	{
		UE_LOG(LogHALStatusFXRuntime, Display, TEXT("StatusFX Source Tag=%s Kind=%s Name=%s Age=%.3f RecipeSubmitted=%d"),
			*S.Tag.ToString(), *S.Kind.ToString(), *S.Name.ToString(), S.AgeSeconds, S.bSubmitted);
	}
	for (const auto& I : D.Instances)
	{
		UE_LOG(LogHALStatusFXRuntime, Display, TEXT("StatusFX Instance Tag=%s Id=%llu Phase=%s Sources=%d Clock=%.3f Intensity=%.3f Available=%d Recipe=%s"),
			*I.Tag.ToString(), I.InstanceId, PhaseName(I.Phase), I.SourceCount, I.ClockSeconds, I.Intensity, I.bAvailable,
			*GetPathNameSafe(Recipes.FindRef(I.Tag)));
	}
}
