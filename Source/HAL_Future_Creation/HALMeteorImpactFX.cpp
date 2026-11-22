#include "HALMeteorBall.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void AHALMeteorStrike::PrepareImpactFX()
{
	const auto& Type = *State.Type;
	if (Type.ImpactGroundMaterial)
	{
		ImpactGroundMID = UMaterialInstanceDynamic::Create(Type.ImpactGroundMaterial, this);
		ImpactGroundMID->SetVectorParameterValue(TEXT("RimColor"), Type.CoronaColor);
		ImpactGroundMID->SetVectorParameterValue(TEXT("Accent"), Type.ShockAccentColor);
		ImpactGroundMID->SetScalarParameterValue(TEXT("RingWidth"), Type.ShockRingWidth / Type.DamageRadius);
		ImpactGroundMID->SetScalarParameterValue(TEXT("RingIntensity"), Type.ShockRingIntensity);
	}
	if (!Type.ImpactCloudMesh || !Type.ImpactCloudMaterial) { return; }
	for (UStaticMeshComponent* Cloud : ImpactClouds)
	{
		Cloud->SetStaticMesh(Type.ImpactCloudMesh);
		Cloud->SetTranslucentSortPriority(2);
		auto* MID = UMaterialInstanceDynamic::Create(Type.ImpactCloudMaterial, this);
		MID->SetVectorParameterValue(TEXT("Tint"), Type.CloudTint);
		MID->SetVectorParameterValue(TEXT("CoolColor"), Type.CooledSmokeColor);
		MID->SetScalarParameterValue(TEXT("Curl"), Type.CloudCurl);
		Cloud->SetMaterial(0, MID);
		CloudMIDs.Add(MID);
	}
}

void AHALMeteorStrike::DrawImpactClouds(float Progress)
{
	const auto& Type = *State.Type;
	const FVector Ground(State.Ground);
	// Three outward-facing crests open around the car. Local +X follows the painted tongue into its rolling lip.
	static constexpr float Heading[] = {24.f, 149.f, 263.f};
	static constexpr float Size[] = {1.f, .94f, .97f};
	static constexpr float Width[] = {1.74f, -1.64f, 1.80f};
	static constexpr float Delay[] = {0.f, .045f, .022f};
	for (int32 Index = 0; Index < CloudMIDs.Num(); ++Index)
	{
		const float Phase = FMath::Clamp((Progress - Delay[Index]) / (1.f - Delay[Index]), 0.f, 1.f);
		auto* Cloud = ImpactClouds[Index].Get();
		Cloud->SetVisibility(Phase > 0.f && Phase < 1.f);
		if (!Cloud->IsVisible()) { continue; }

		const float Burst = 1.f - FMath::Pow(1.f - FMath::Min(Phase / .52f, 1.f), 2.8f);
		const float Lift = FMath::SmoothStep(.035f, .60f, Phase);
		const float Release = FMath::SmoothStep(.60f, 1.f, Phase);
		const float Radius = Type.DamageRadius;
		const FRotator Facing(0., Heading[Index] + 12.f * Burst - 7.f * Release, 0.);
		const float Footprint = Radius * Type.CloudCoverage * Size[Index] * (.11f + .89f * Burst) / 100.f;
		// The painted tongues start partway across the sheet. Overlap their roots at the impact point.
		const FVector Origin = Ground - Facing.Vector() * (20.f * Footprint)
			+ FVector::UpVector * (4.f + Radius * Type.CloudLift * .33f * Release);
		const float Height = Radius * Type.CloudLift * (.045f + .955f * Lift) * (1.f - .27f * Release) / 40.f;
		const FVector Scale(1.25f * Footprint, Width[Index] * Footprint, FMath::Max(.0001f, Height));
		Cloud->SetWorldTransform(FTransform(Facing, Origin, Scale));
		CloudMIDs[Index]->SetScalarParameterValue(TEXT("Phase"), Phase);
	}
}
