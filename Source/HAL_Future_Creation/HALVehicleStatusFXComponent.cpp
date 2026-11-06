#include "HALVehicleStatusFXComponent.h"
#include "HALVehiclePresentationComponent.h"
#include "HALVehicleStatusFXSubsystem.h"
#include "HALVehicleStatusFXDefinition.h"
#include "VehicleDefinition.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"

UHALVehicleStatusFXComponent::UHALVehicleStatusFXComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

double UHALVehicleStatusFXComponent::Now() const { return FMath::Max(0.0, FPlatformTime::Seconds() - Epoch); }

void UHALVehicleStatusFXComponent::Bind(UHALVehiclePresentationComponent* InProvider, UHALVehicleStatusFXSubsystem* InCache,
	const UVehicleDefinition* InVehicle, const UObject* InAvatarIdentity)
{
	check(IsInGameThread());
	if (bBound && Provider == InProvider && Cache == InCache && Vehicle == InVehicle && AvatarIdentity == InAvatarIdentity
		&& bHasAvatarIdentity == (InAvatarIdentity != nullptr) && (!bHasAvatarIdentity || AvatarIdentity.IsValid())) { return; }
	Unbind();
	if (!IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed() || GetNetMode() == NM_DedicatedServer
		|| !IsValid(InProvider) || InProvider->GetOwner() != GetOwner()) { return; }
	Provider = InProvider; Cache = InCache; Vehicle = InVehicle; AvatarIdentity = InAvatarIdentity;
	bHasAvatarIdentity = InAvatarIdentity != nullptr;
	bBound = true; Epoch = FPlatformTime::Seconds();
	AddTickPrerequisiteComponent(InProvider);
	const FGuid Binding = Runtime.GetBinding();
	TargetChanged = InProvider->OnStatusFXBindingsChanged().AddWeakLambda(this, [this, Binding]()
	{
		if (bBound && Binding == Runtime.GetBinding()) { Refresh(); }
	});
	if (InCache)
	{
		CatalogChanged = InCache->OnCatalogChanged().AddWeakLambda(this, [this, Binding](const FSoftObjectPath&, EHALStatusFXLoadState)
		{
			// Shared-cache completion can arrive after an Avatar/mesh transition. Never retain a captured mesh.
			if (bBound && Binding == Runtime.GetBinding()) { Refresh(); }
		});
		InCache->PrepareForVehicle(InVehicle);
	}
	Refresh();
}

void UHALVehicleStatusFXComponent::ReleaseResources()
{
	Requests.Reset();
	for (auto& Particle : Particles)
	{
		if (IsValid(Particle.Component)) { Particle.Component->DestroyComponent(); }
	}
	Particles.Reset();
	// Unassigned MIDs have no renderer/mesh references. Releasing the last owning reference permits GC.
	Surfaces.Reset();
	++MeshGeneration;
}

void UHALVehicleStatusFXComponent::Unbind()
{
	check(IsInGameThread());
	if (bBound) { LogStateChange(TEXT("Unbind")); }
	DetachObserver(); DebugSources.Reset();
	bBound = false;
	if (Provider.IsValid())
	{
		Provider->OnStatusFXBindingsChanged().Remove(TargetChanged);
		RemoveTickPrerequisiteComponent(Provider.Get());
	}
	if (Cache.IsValid()) { Cache->OnCatalogChanged().Remove(CatalogChanged); }
	TargetChanged.Reset(); CatalogChanged.Reset();
	ReleaseResources(); Runtime.ResetBinding(); Demands.Reset(); Recipes.Reset(); Targets = {};
	Provider.Reset(); Cache.Reset(); Vehicle.Reset(); AvatarIdentity.Reset();
	bHasAvatarIdentity = false;
	SetComponentTickEnabled(false);
}

FHALStatusFXSourceToken UHALVehicleStatusFXComponent::AcquireSource()
{
	check(IsInGameThread());
	if (bBound && bObservingASC && !HasValidObserver()) { Unbind(); }
	return bBound ? Runtime.AcquireSource() : FHALStatusFXSourceToken();
}

bool UHALVehicleStatusFXComponent::Submit(FHALStatusFXSourceToken Source, FGameplayTag Tag)
{
	check(IsInGameThread());
	if (bBound && bObservingASC && !HasValidObserver()) { Unbind(); return false; }
	const auto Root = FGameplayTag::RequestGameplayTag(TEXT("State.Visual"), false);
	if (!bBound || !Runtime.HasSource(Source) || !Tag.IsValid() || Tag == Root || !Tag.MatchesTag(Root)) { return false; }
	if (const auto* Existing = Demands.FindByPredicate([&](const auto& D) { return D.Source == Source; }))
	{
		if (Existing->Tag != Tag) { return false; }
	}
	else { Demands.Add({Source, Tag, Now(), false}); }
	Refresh(); return true;
}

bool UHALVehicleStatusFXComponent::RemoveSource(FHALStatusFXSourceToken Source)
{
	check(IsInGameThread());
	if (!bBound || !Runtime.RemoveSource(Source, Now())) { return false; }
	Demands.RemoveAll([&](const auto& D) { return D.Source == Source; });
	Refresh(); return true;
}

void UHALVehicleStatusFXComponent::Refresh()
{
	check(IsInGameThread());
	if (!bBound || bRefreshing || bReconcilingTags) { return; }
	if (!Provider.IsValid() || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()
		|| (bHasAvatarIdentity && !AvatarIdentity.IsValid()) || (bObservingASC && !HasValidObserver())) { Unbind(); return; }
	TGuardValue<bool> Guard(bRefreshing, true);
	// UE 5.8 single loose-tag partial removal can omit deferred delegates. Recheck before
	// preparing late resources and on the existing active spatial update, without an idle poll.
	ReconcileObservedTags();
	if (!bBound) { return; }
	const double Time = Now();
	Runtime.Advance(Time);
	if (!Provider->AreStatusFXBindingsCurrent(Targets)) { ReleaseResources(); }
	Provider->CaptureStatusFXTargets(Targets);
	for (auto& D : Demands)
	{
		if (D.bSubmitted || !Cache.IsValid()) { continue; }
		const auto* Recipe = Cache->FindEffect(D.Tag, Vehicle.Get());
		if (!Recipe) { continue; }
		const FHALStatusFXTiming Timing{Recipe->FadeInSeconds, Recipe->FadeOutSeconds, Recipe->ResidueSeconds};
		if (Runtime.Submit(D.Source, D.Tag, Timing, Time, FMath::Max(0.0, Time - D.ObservedAt)))
		{
			D.bSubmitted = true; Recipes.Add(D.Tag, const_cast<UHALVehicleStatusFXDefinition*>(Recipe));
		}
	}
	PrepareRequests(Time);
	// An active instance needs current geometry and silent visibility/asset-change detection, even at constant intensity.
	// A recipe-only wait is resumed by the cache event; idle vehicles never poll.
	SetComponentTickEnabled(Runtime.GetInstanceCount() > 0);
}

void UHALVehicleStatusFXComponent::PrepareRequests(double Time)
{
	Requests.Reset();
	TSet<UMaterialInstanceDynamic*> UsedSurfaces;
	TSet<UNiagaraComponent*> UsedParticles;
	TSet<FGameplayTag> LiveTags;
	for (const auto& State : Runtime.GetSnapshots())
	{
		LiveTags.Add(State.Tag);
		auto* Recipe = Recipes.FindRef(State.Tag).Get();
		const bool bAvailable = IsValid(Recipe) && !Targets.Targets.IsEmpty();
		Runtime.SetAvailable(State.GetHandle(), bAvailable, Time);
		if (!bAvailable) { continue; }
		for (const auto& Target : Targets.Targets)
		{
			FHALStatusFXRenderRequest Request;
			Request.MeshGeneration = MeshGeneration; Request.Target = Target; Request.Recipe = Recipe;
			if (Recipe->OverlayMaterial)
			{
				auto* Surface = Surfaces.FindByPredicate([&](const auto& S)
				{ return S.TargetId == Target.Id && IsValid(S.Material) && S.Material->Parent == Recipe->OverlayMaterial; });
				if (!Surface)
				{
					FHALStatusFXPreparedSurface New; New.TargetId = Target.Id;
					New.Material = UMaterialInstanceDynamic::Create(Recipe->OverlayMaterial, this);
					Surface = &Surfaces.Add_GetRef(MoveTemp(New));
				}
				Request.Surface = Surface->Material; UsedSurfaces.Add(Surface->Material);
			}
			for (const auto& Layer : Recipe->NiagaraLayers)
			{
				auto* Particle = Particles.FindByPredicate([&](const auto& P)
				{ return P.Instance.Tag == State.Tag && P.Instance.Id == State.InstanceId && P.TargetId == Target.Id && P.LayerId == Layer.LayerId; });
				if (!Particle)
				{
					FHALStatusFXPreparedParticle New; New.Instance = State.GetHandle(); New.TargetId = Target.Id; New.LayerId = Layer.LayerId;
					New.Component = NewObject<UNiagaraComponent>(GetOwner(), NAME_None, RF_Transient);
					New.Component->SetAutoActivate(false); New.Component->SetAutoDestroy(false);
					New.Component->SetAsset(Layer.Binding == EHALStatusFXBinding::Bounds ? Layer.BoundsSystem.Get()
						: Target.Kind == EHALVehicleFXMeshKind::Static ? Layer.StaticSurfaceSystem.Get() : Layer.SkeletalSurfaceSystem.Get());
					// Intentionally unregistered: no simulation, sampling, or duplicate shock particles before FX-4/5.
					Particle = &Particles.Add_GetRef(MoveTemp(New));
				}
				Request.Particles.Add({Layer.LayerId, Particle->Component.Get()}); UsedParticles.Add(Particle->Component);
			}
			Runtime.GetSnapshot(State.Tag, Request.State);
			Requests.Add(MoveTemp(Request));
		}
	}
	Surfaces.RemoveAll([&](const auto& S) { return !UsedSurfaces.Contains(S.Material); });
	Particles.RemoveAll([&](auto& P)
	{
		if (UsedParticles.Contains(P.Component)) { return false; }
		if (IsValid(P.Component)) { P.Component->DestroyComponent(); }
		return true;
	});
	for (auto It = Recipes.CreateIterator(); It; ++It) { if (!LiveTags.Contains(It.Key())) { It.RemoveCurrent(); } }
}

void UHALVehicleStatusFXComponent::CaptureRenderRequests(TArray<FHALStatusFXRenderRequest>& Out)
{
	Refresh(); Out = Requests;
}

bool UHALVehicleStatusFXComponent::IsRequestCurrent(const FHALStatusFXRenderRequest& Request)
{
	check(IsInGameThread());
	// Delayed work may arrive between ticks, after a tail expired or an Avatar was destroyed.
	Refresh();
	if (!bBound || (bHasAvatarIdentity && !AvatarIdentity.IsValid())
		|| Request.MeshGeneration != MeshGeneration || Request.State.Binding != Runtime.GetBinding()
		|| !Provider.IsValid() || !Provider->AreStatusFXBindingsCurrent(Targets)) { return false; }
	return Requests.ContainsByPredicate([&](const auto& Current)
	{
		return Current.State.Tag == Request.State.Tag && Current.State.InstanceId == Request.State.InstanceId
			&& Current.Target.Id == Request.Target.Id && Current.Target.Mesh == Request.Target.Mesh
			&& Current.Target.MeshAsset == Request.Target.MeshAsset && Current.Surface == Request.Surface;
	});
}

void UHALVehicleStatusFXComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction); Refresh();
}

void UHALVehicleStatusFXComponent::EndPlay(const EEndPlayReason::Type Reason) { Unbind(); Super::EndPlay(Reason); }
void UHALVehicleStatusFXComponent::OnUnregister() { Unbind(); Super::OnUnregister(); }
