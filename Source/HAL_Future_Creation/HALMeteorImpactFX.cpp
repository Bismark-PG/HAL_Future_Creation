#include "HALMeteorBall.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
struct FMeteorPuff
{
	float HeadingDegrees;
	float Travel, Radius; // fractions of the cloud footprint
	float Height;         // relative to the height set on the ball type
	float Birth, End;     // birth in impact time, end in total cloud lifetime
};

// Three fire heads followed by smaller, offset puffs that detach into cooling smoke.
constexpr FMeteorPuff Puffs[] = {
	{ 24.f, .42f, .50f, 1.06f,  .00f, .91f},
	{151.f, .46f, .47f,  .89f,  .045f, .98f},
	{267.f, .40f, .51f,  .98f,  .022f, .94f},
	{ 58.f, .57f, .25f,  .68f,  .11f, .87f},
	{118.f, .52f, .29f,  .80f,  .085f, 1.f},
	{299.f, .59f, .24f,  .62f,  .16f, .93f}
};
}

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
	bUsesVolumeClouds = Type.ImpactVolumeMesh && Type.ImpactVolumeMesh->GetMaterial(0);
	auto* Mesh = bUsesVolumeClouds ? Type.ImpactVolumeMesh.Get() : Type.ImpactCloudMesh.Get();
	auto* Material = bUsesVolumeClouds ? Mesh->GetMaterial(0) : Type.ImpactCloudMaterial.Get();
	if (!Mesh || !Material) { return; }
	const int32 Count = bUsesVolumeClouds ? UE_ARRAY_COUNT(Puffs) : 3;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		auto* Cloud = ImpactClouds[Index].Get();
		Cloud->SetStaticMesh(Mesh);
		Cloud->SetTranslucentSortPriority(2);
		auto* MID = UMaterialInstanceDynamic::Create(Material, this);
		MID->SetVectorParameterValue(TEXT("Tint"), Type.CloudTint);
		MID->SetVectorParameterValue(TEXT("CoolColor"), Type.CooledSmokeColor);
		if (bUsesVolumeClouds)
		{
			Cloud->SetBoundsScale(1.12f);
			const FVector Outward = FRotator(0.f, Puffs[Index].HeadingDegrees, 0.f).Vector();
			MID->SetVectorParameterValue(TEXT("CoreDirection"), FLinearColor(-Outward.X, -Outward.Y, -.45f));
			MID->SetScalarParameterValue(TEXT("Seed"), Index * 2.39996f);
			MID->SetScalarParameterValue(TEXT("LobeSwelling"), FMath::Min(1.f, Type.CloudLift / .3f));
		}
		else { MID->SetScalarParameterValue(TEXT("Curl"), Type.CloudCurl); }
		Cloud->SetMaterial(0, MID);
		CloudMIDs.Add(MID);
	}
}

void AHALMeteorStrike::DrawVolumeClouds(float SecondsAfterImpact)
{
	const auto& Type = *State.Type;
	const float Reach = Type.DamageRadius * Type.CloudCoverage;
	const float Lift = Type.DamageRadius * Type.CloudLift;
	const FVector Extent = Type.ImpactVolumeMesh->GetBounds().BoxExtent;
	const float MeshRadius = FMath::Max3(1., Extent.X, Extent.Y);
	const float MeshHeight = FMath::Max(1., Extent.Z);
	for (int32 Index = 0; Index < CloudMIDs.Num(); ++Index)
	{
		const auto& Puff = Puffs[Index];
		const float Birth = Puff.Birth * Type.ImpactSeconds;
		const float Age = SecondsAfterImpact - Birth;
		const float Life = FMath::Clamp(Age / (Puff.End * Type.GetCloudDuration() - Birth), 0.f, 1.f);
		auto* Cloud = ImpactClouds[Index].Get();
		Cloud->SetVisibility(Life > 0.f && Life < 1.f);
		if (!Cloud->IsVisible()) { continue; }

		const float GrowthTime = FMath::Clamp(Age / (Type.CloudExpansionFraction * Type.ImpactSeconds), 0.f, 1.f);
		const float Growth = GrowthTime * (2.f - GrowthTime);
		const float Diffusion = FMath::SmoothStep(.26f, 1.f, Life);
		const float HalfWidth = Reach * Puff.Radius * (.04f + .92f * Growth + .14f * Diffusion);
		const float HalfHeight = Lift * Puff.Height * (.018f + .45f * Growth * Growth + .05f * Diffusion);
		const FRotator Heading(0.f, Puff.HeadingDegrees, 0.f);
		FVector Center = FVector(State.Ground) + Heading.Vector() * Reach * Puff.Travel * (.80f * Growth + .20f * Life);
		Center.Z += 4.f + HalfHeight + Lift * (.07f * Life + .57f * Diffusion);
		const float Roundness = FMath::Min(1.f, HalfHeight / FMath::Max(1.f, .36f * HalfWidth));
		const float Roll = Type.CloudCurl * Roundness * (16.f * Growth + 4.f * Diffusion);
		const FRotator Rotation(Roll, Puff.HeadingDegrees + 47.f * Index, 0.f);
		const FVector Scale(HalfWidth / MeshRadius, .93f * HalfWidth / MeshRadius, FMath::Max(.0001f, HalfHeight / MeshHeight));
		Cloud->SetWorldTransform(FTransform(Rotation, Center, Scale));

		const float Heat = 1.f - FMath::SmoothStep(.30f, 1.02f, Age / Type.ImpactSeconds);
		CloudMIDs[Index]->SetScalarParameterValue(TEXT("Heat"), Heat);
		CloudMIDs[Index]->SetScalarParameterValue(TEXT("Phase"), Life);
	}
}

void AHALMeteorStrike::DrawCrestSheets(float Progress)
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
