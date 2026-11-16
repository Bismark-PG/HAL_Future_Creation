#include "HALVehiclePresentationComponent.h"
#include "HALImpactBurstSettings.h"
#include "HALCombatTypes.h"
#include "Components/MaterialBillboardComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarImpactBurstLog(TEXT("hal.VehicleImpactBurstLog"), 0,
		TEXT("0=off, 1=authority published/consumed comic impact bursts; no physics or damage changes."));
	bool IsNewerBurstSequence(uint32 Value, uint32 Previous) { return Value && int32(Value - Previous) > 0; }
	void ReleasePulseSlot(const FHALImpactBurstPulse& Pulse, TArray<int32>& OutReleasedSlots)
	{ if (Pulse.Slot != INDEX_NONE) { OutReleasedSlots.AddUnique(Pulse.Slot); } }
}

float HALVehiclePresentation::BurstStrength(const FHALImpactBurstConfig& Config, float DesiredHorizontalDeltaSpeed)
{
	const float Span = Config.MaximumPushSpeed - Config.MinimumPushSpeed;
	if (!FMath::IsFinite(DesiredHorizontalDeltaSpeed) || !FMath::IsFinite(Span) || Span <= 0) { return 0; }
	return FMath::Clamp((DesiredHorizontalDeltaSpeed - Config.MinimumPushSpeed) / Span, 0.f, 1.f);
}

uint8 HALVehiclePresentation::QuantizeBurstStrength(float Strength)
{
	return FMath::IsFinite(Strength) ? uint8(FMath::RoundToInt(FMath::Clamp(Strength, 0.f, 1.f) * 255.f)) : 0;
}

float HALVehiclePresentation::BurstSeed(uint32 Sequence)
{
	// Derived from the authority identity, so every client draws the same star for the same hit.
	const uint32 Hash = Sequence * 2654435761u;
	return float((Hash >> 8) & 0xFFFF) / 65535.f;
}

FLinearColor HALVehiclePresentation::BurstEdgeColor(const FHALImpactBurstConfig& Config, EHALImpactTier Tier)
{
	switch (Tier)
	{
	case EHALImpactTier::Heavy: return Config.HeavyColor;
	case EHALImpactTier::Medium: return Config.MediumColor;
	default: return Config.LightColor;
	}
}

FVector HALVehiclePresentation::ResolveBurstPoint(const FHALImpactBurstConfig& Config, const FVector& RawPoint,
	const FVector& TargetLocation, const FVector& PushDirection)
{
	if (TargetLocation.ContainsNaN()) { return FVector::ZeroVector; }
	const FVector Direction = PushDirection.ContainsNaN() ? FVector::ZeroVector : PushDirection.GetSafeNormal();
	// A zero or far away contact point means the attack never reported one; use the target edge instead.
	const bool bUsable = !RawPoint.ContainsNaN() && !RawPoint.IsZero()
		&& FVector::DistSquared(RawPoint, TargetLocation) <= FMath::Square(Config.MaximumPointDistanceCm);
	const FVector Base = bUsable ? RawPoint : TargetLocation - Direction * Config.FallbackOffsetCm;
	// Off the contact surface toward the source, then lifted so the body never splits the sprite.
	return Base - Direction * Config.SurfaceOffsetCm + FVector(0, 0, Config.VerticalOffsetCm);
}

bool HALVehiclePresentation::AcceptBurst(const FHALImpactBurstConfig& Config, const FHALImpactBurstEvent& Event,
	float Age, FHALImpactBurstState& State, TArray<int32>& OutReleasedSlots)
{
	OutReleasedSlots.Reset();
	const FVector Point(Event.WorldPoint);
	if (!Config.bEnabled || !IsNewerBurstSequence(Event.Sequence, State.LastSeenSequence) || !FMath::IsFinite(Age) || Age < 0
		|| !HALImpact::IsValidTier(Event.Tier) || Event.Tier == EHALImpactTier::None || Point.ContainsNaN()) { return false; }
	// Retire identities even when their packet is late or the burst is already over.
	State.LastSeenSequence = Event.Sequence;
	const float Strength = float(Event.Strength) / 255.f;
	const float Lifetime = Config.LifetimeSeconds + Config.StrengthLifetimeSeconds * Strength;
	if (Lifetime <= 0 || Age > Config.MaximumEventAgeSeconds || Age >= Lifetime) { return false; }
	const int32 Cap = FMath::Max(Config.MaximumConcurrent, 1);
	while (State.Pulses.Num() >= Cap)
	{
		ReleasePulseSlot(State.Pulses[0], OutReleasedSlots);
		State.Pulses.RemoveAt(0);
	}
	FHALImpactBurstPulse& Pulse = State.Pulses.AddDefaulted_GetRef();
	Pulse.WorldPoint = Point;
	Pulse.Age = Age;
	Pulse.Lifetime = Lifetime;
	Pulse.Strength = Strength;
	Pulse.Seed = BurstSeed(Event.Sequence);
	Pulse.SizeCm = FMath::Lerp(Config.MinimumSizeCm, Config.MaximumSizeCm, Strength);
	Pulse.Spikes = FMath::Lerp(Config.MinimumSpikes, Config.MaximumSpikes, Strength);
	Pulse.EdgeColor = BurstEdgeColor(Config, Event.Tier);
	Pulse.Tier = Event.Tier;
	++State.StartedCount;
	return true;
}

void HALVehiclePresentation::AdvanceBursts(const FHALImpactBurstConfig& Config, float DeltaSeconds,
	FHALImpactBurstState& State, TArray<int32>& OutReleasedSlots)
{
	OutReleasedSlots.Reset();
	if (!Config.bEnabled || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0)
	{
		// Disabling drops live bursts but keeps the watermark, so late packets never replay.
		for (const FHALImpactBurstPulse& Pulse : State.Pulses) { ReleasePulseSlot(Pulse, OutReleasedSlots); }
		State.Pulses.Reset();
		return;
	}
	for (int32 Index = State.Pulses.Num() - 1; Index >= 0; --Index)
	{
		FHALImpactBurstPulse& Pulse = State.Pulses[Index];
		Pulse.Age += DeltaSeconds;
		if (Pulse.Lifetime <= 0 || Pulse.Age >= Pulse.Lifetime)
		{
			ReleasePulseSlot(Pulse, OutReleasedSlots);
			State.Pulses.RemoveAt(Index);
		}
	}
}

void HALVehiclePresentation::ClearBursts(FHALImpactBurstState& State, TArray<int32>& OutReleasedSlots)
{
	OutReleasedSlots.Reset();
	for (const FHALImpactBurstPulse& Pulse : State.Pulses) { ReleasePulseSlot(Pulse, OutReleasedSlots); }
	State.Pulses.Reset();
}

const FHALImpactBurstConfig* UHALVehiclePresentationComponent::GetBurstConfig() const
{
	return bBurstReady ? &GetDefault<UHALImpactBurstSettings>()->Burst : nullptr;
}

void UHALVehiclePresentationComponent::ResolveBurstMaterial()
{
	bBurstReady = false;
	BurstMaterial = nullptr;
	const FHALImpactBurstConfig& Settings = GetDefault<UHALImpactBurstSettings>()->Burst;
	FString Error;
	if (!Settings.bEnabled) { return; }
	if (!Settings.Validate(Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("HAL ImpactBurst: disabled, settings invalid: %s"), *Error);
		return;
	}
	// Authority only needs the configuration to publish; the sprite material is a client side cost.
	bBurstReady = true;
	if (GetNetMode() == NM_DedicatedServer || Settings.Material.IsNull()) { return; }
	// Resolved once outside the hit path. A missing material leaves the path live but silent.
	BurstMaterial = Settings.Material.LoadSynchronous();
	if (!BurstMaterial)
	{ UE_LOG(LogTemp, Warning, TEXT("HAL ImpactBurst: material %s could not be loaded."), *Settings.Material.ToString()); }
}

int32 UHALVehiclePresentationComponent::AcquireBurstSlot(const FHALImpactBurstConfig& BurstConfig)
{
	AActor* Owner = GetOwner();
	if (!IsValid(BurstMaterial) || !IsValid(Owner) || !Owner->GetRootComponent()) { return INDEX_NONE; }
	const int32 Cap = FMath::Clamp(BurstConfig.MaximumConcurrent, 1, 16);
	if (BurstSprites.Num() < Cap) { BurstSprites.SetNum(Cap); BurstMaterials.SetNum(Cap); }
	for (int32 Slot = 0; Slot < Cap; ++Slot)
	{
		bool bBusy = false;
		for (const FHALImpactBurstPulse& Pulse : BurstState.Pulses) { if (Pulse.Slot == Slot) { bBusy = true; break; } }
		if (bBusy) { continue; }
		if (!IsValid(BurstSprites[Slot]))
		{
			UMaterialBillboardComponent* Sprite = NewObject<UMaterialBillboardComponent>(Owner);
			if (!Sprite) { return INDEX_NONE; }
			Sprite->SetupAttachment(Owner->GetRootComponent());
			// World anchored: the burst marks where the contact happened, not where the car ends up.
			Sprite->SetUsingAbsoluteLocation(true);
			Sprite->SetUsingAbsoluteRotation(true);
			Sprite->SetUsingAbsoluteScale(true);
			Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Sprite->SetCastShadow(false);
			Sprite->bReceivesDecals = false;
			Sprite->SetVisibility(false);
			Sprite->RegisterComponent();
			Sprite->AddElement(BurstMaterial, nullptr, false, 1.f, 1.f, nullptr);
			BurstSprites[Slot] = Sprite;
			BurstMaterials[Slot] = Sprite->CreateDynamicMaterialInstance(0, BurstMaterial);
		}
		if (!IsValid(BurstMaterials[Slot])) { return INDEX_NONE; }
		return Slot;
	}
	return INDEX_NONE;
}

void UHALVehiclePresentationComponent::ReleaseBurstSlots(const TArray<int32>& Slots)
{
	for (const int32 Slot : Slots)
	{
		if (BurstSprites.IsValidIndex(Slot) && IsValid(BurstSprites[Slot])) { BurstSprites[Slot]->SetVisibility(false); }
	}
}

void UHALVehiclePresentationComponent::ApplyBurstSlot(const FHALImpactBurstConfig& BurstConfig, const FHALImpactBurstPulse& Pulse)
{
	if (!BurstSprites.IsValidIndex(Pulse.Slot) || !IsValid(BurstSprites[Pulse.Slot]) || !IsValid(BurstMaterials[Pulse.Slot])) { return; }
	UMaterialBillboardComponent* Sprite = BurstSprites[Pulse.Slot];
	UMaterialInstanceDynamic* Instance = BurstMaterials[Pulse.Slot];
	// BaseSize is the sprite half extent, so the configured size stays a real diameter in cm.
	if (Sprite->Elements.Num() > 0)
	{
		Sprite->Elements[0].BaseSizeX = Sprite->Elements[0].BaseSizeY = FMath::Max(Pulse.SizeCm, 1.f) * .5f;
		Sprite->MarkRenderStateDirty();
	}
	Sprite->SetWorldLocation(Pulse.WorldPoint);
	Instance->SetScalarParameterValue(BurstConfig.StrengthParameter, Pulse.Strength);
	Instance->SetScalarParameterValue(BurstConfig.SeedParameter, Pulse.Seed);
	Instance->SetScalarParameterValue(BurstConfig.SpikesParameter, Pulse.Spikes);
	Instance->SetScalarParameterValue(BurstConfig.AgeParameter, Pulse.Lifetime > 0 ? FMath::Clamp(Pulse.Age / Pulse.Lifetime, 0.f, 1.f) : 1.f);
	Instance->SetVectorParameterValue(BurstConfig.FillColorParameter, BurstConfig.FillColor);
	Instance->SetVectorParameterValue(BurstConfig.EdgeColorParameter, Pulse.EdgeColor);
	Sprite->SetVisibility(true);
}

void UHALVehiclePresentationComponent::PublishImpactBurst(const FHALCombatHitResult& Result, const FVector& WorldDirection,
	const FVector& WorldImpactPoint, uint32 Sequence)
{
	const FHALImpactBurstConfig* BurstConfig = GetBurstConfig();
	if (!BurstConfig || !Sequence || !IsValid(GetOwner())) { return; }
	FHALImpactBurstEvent Event;
	Event.Sequence = Sequence;
	Event.ServerTimeSeconds = float(GetLaunchServerTime());
	Event.Tier = Result.ImpactSummary.EffectiveTier;
	Event.Strength = HALVehiclePresentation::QuantizeBurstStrength(
		HALVehiclePresentation::BurstStrength(*BurstConfig, Result.ImpactSummary.DesiredHorizontalDeltaSpeed));
	Event.WorldPoint = HALVehiclePresentation::ResolveBurstPoint(*BurstConfig, WorldImpactPoint,
		GetOwner()->GetActorLocation(), WorldDirection);
	if (CVarImpactBurstLog.GetValueOnGameThread())
	{
		UE_LOG(LogTemp, Display, TEXT("HAL ImpactBurst: %s Publish Seq=%u Tier=%d Strength=%u Point=%s"),
			*GetNameSafe(GetOwner()), Event.Sequence, int32(Event.Tier), Event.Strength, *FVector(Event.WorldPoint).ToCompactString());
	}
	MulticastImpactBurst(Event);
}

void UHALVehiclePresentationComponent::MulticastImpactBurst_Implementation(FHALImpactBurstEvent Event)
{ ConsumeImpactBurst(Event); }

void UHALVehiclePresentationComponent::ConsumeImpactBurst(const FHALImpactBurstEvent& Event)
{
	check(IsInGameThread());
	const FHALImpactBurstConfig* BurstConfig = GetBurstConfig();
	// A component that cannot advance the burst must never start one, or the sprite would never retire.
	if (!BurstConfig || !GetWorld() || !IsComponentTickEnabled() || !FMath::IsFinite(Event.ServerTimeSeconds)) { return; }
	const double RawAge = GetLaunchServerTime() - Event.ServerTimeSeconds;
	if (!FMath::IsFinite(RawAge) || RawAge < -.25) { return; }
	const float Age = float(FMath::Max(0., RawAge));
	TArray<int32> Released;
	const bool bStarted = HALVehiclePresentation::AcceptBurst(*BurstConfig, Event, Age, BurstState, Released);
	// Evicted sprites are hidden before a new pulse can claim the same slot.
	ReleaseBurstSlots(Released);
	if (bStarted && !BurstState.Pulses.IsEmpty())
	{
		FHALImpactBurstPulse& Pulse = BurstState.Pulses.Last();
		Pulse.Slot = AcquireBurstSlot(*BurstConfig);
		ApplyBurstSlot(*BurstConfig, Pulse);
	}
	if (CVarImpactBurstLog.GetValueOnGameThread())
	{
		UE_LOG(LogTemp, Display, TEXT("HAL ImpactBurst: %s Consume Seq=%u Age=%.3f Started=%d Live=%d Starts=%u"),
			*GetNameSafe(GetOwner()), Event.Sequence, Age, bStarted ? 1 : 0, BurstState.Pulses.Num(), BurstState.StartedCount);
	}
}

void UHALVehiclePresentationComponent::UpdateImpactBursts(float DeltaTime)
{
	const FHALImpactBurstConfig* BurstConfig = GetBurstConfig();
	if (!BurstConfig) { if (!BurstState.Pulses.IsEmpty()) { ClearImpactBursts(); } return; }
	TArray<int32> Released;
	HALVehiclePresentation::AdvanceBursts(*BurstConfig, DeltaTime, BurstState, Released);
	ReleaseBurstSlots(Released);
	for (const FHALImpactBurstPulse& Pulse : BurstState.Pulses)
	{
		if (!BurstMaterials.IsValidIndex(Pulse.Slot) || !IsValid(BurstMaterials[Pulse.Slot])) { continue; }
		BurstMaterials[Pulse.Slot]->SetScalarParameterValue(BurstConfig->AgeParameter,
			Pulse.Lifetime > 0 ? FMath::Clamp(Pulse.Age / Pulse.Lifetime, 0.f, 1.f) : 1.f);
	}
}

void UHALVehiclePresentationComponent::ClearImpactBursts()
{
	TArray<int32> Released;
	HALVehiclePresentation::ClearBursts(BurstState, Released);
	ReleaseBurstSlots(Released);
}
