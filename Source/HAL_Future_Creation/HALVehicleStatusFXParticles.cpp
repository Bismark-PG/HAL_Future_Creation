#include "HALVehicleStatusFXComponent.h"
#include "HALVehicleStatusFXRenderer.h"
#include "HALVehicleStatusFXDefinition.h"
#include "HALVehicleStatusFXSettings.h"
#include "HALStatusFXParticleBinding.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "Misc/App.h"

void UHALVehicleStatusFXComponent::RenderParticles()
{
	const auto* Settings = GetDefault<UHALVehicleStatusFXSettings>();
	int32 Remaining = FMath::Clamp(Settings->MaximumParticlesPerVehicle, 0, 8192);
	int32 Layers = FMath::Clamp(Settings->MaximumParticleLayersPerVehicle, 0, 128);
	for (auto& P : Particles)
	{
		if (!IsValid(P.Component)) { continue; }
		const auto* R = Requests.FindByPredicate([&](const auto& Q) { return Q.State.Tag == P.Instance.Tag && Q.State.InstanceId == P.Instance.Id && Q.Target.Id == P.TargetId; });
		const auto* D = R ? R->Recipe.Get() : nullptr;
		if (!D || !D->bEnableRendering || HALStatusFXRender::UsesShockDriver(P.Instance.Tag))
		{ if (P.Component->IsActive()) { P.Component->DeactivateImmediate(); } P.RenderState = TEXT("ObservationOnly"); continue; }
		const auto* L = D->NiagaraLayers.FindByPredicate([&](const auto& Q) { return Q.LayerId == P.LayerId; });
		if (!L) { continue; }
		if (!P.bAttemptedConfiguration)
		{
			P.bAttemptedConfiguration = true;
			P.bConfigured = HALStatusFXParticle::Configure(P.Component, R->Target, *L, P.RenderState);
			if (!P.bConfigured && L->Binding == EHALStatusFXBinding::MeshSurface && L->SurfaceFallbackBoundsSystem)
			{
				P.FallbackReason = P.RenderState;
				FHALStatusFXNiagaraLayer Bounds = *L; Bounds.Binding = EHALStatusFXBinding::Bounds;
				P.Component->SetAsset(L->SurfaceFallbackBoundsSystem);
				P.bConfigured = HALStatusFXParticle::Configure(P.Component, R->Target, Bounds, P.RenderState);
				P.bBoundsFallback = P.bConfigured;
			}
		}
		if (!P.bConfigured) { continue; }
		const int32 Allowed = Layers > 0 ? FMath::Min(Remaining, L->MaximumParticles) : 0;
		if (Allowed <= 0) { P.RenderState = TEXT("BudgetBlocked"); if (P.Component->IsActive()) { P.Component->DeactivateImmediate(); } continue; }
		Remaining -= Allowed; --Layers;
		const FVector Extent = R->Target.LocalBounds.GetExtent();
		const FVector WorldExtent = Extent * R->Target.MeshToWorld.GetScale3D().GetAbs();
		const double Area = 8 * (WorldExtent.X * WorldExtent.Y + WorldExtent.X * WorldExtent.Z + WorldExtent.Y * WorldExtent.Z);
		const float Scale = L->bScaleRateBySize ? float(FMath::Clamp(Area / L->ReferenceAreaCm2, 0.25, 4.0)) : 1.f;
		const float Residue = HALStatusFXRender::ResidueWeight(*R);
		const float Weight = FMath::Max(R->State.Intensity, L->bEmitDuringResidue ? Residue : 0.f);
		P.Component->SetVariableVec3(TEXT("User.FX_BoundsCenter"), R->Target.LocalBounds.GetCenter());
		P.Component->SetVariableVec3(TEXT("User.FX_BoundsExtent"), Extent * (1 + L->PaddingFraction));
		P.Component->SetVariableFloat(TEXT("User.FX_Intensity"), R->State.Intensity);
		P.Component->SetVariableFloat(TEXT("User.FX_Residue"), Residue);
		P.Component->SetVariableFloat(TEXT("User.FX_Age"), float(R->State.ClockSeconds));
		P.Component->SetVariableFloat(TEXT("User.FX_SpawnRate"), FMath::Min(2000.f, L->SpawnRate * Scale) * Weight);
		P.Component->SetVariableInt(TEXT("User.FX_MaxParticles"), Allowed);
		if (!FApp::CanEverRender() || !GetWorld() || !GetWorld()->FXSystem.IsValid()) { P.RenderState = TEXT("PreparedHeadless"); continue; }
		if (!P.Component->GetAsset()->IsReadyToRun()) { P.RenderState = TEXT("WaitingSystemCompile"); continue; }
		if (!P.Component->IsRegistered()) { P.Component->RegisterComponent(); }
		if (Weight > KINDA_SMALL_NUMBER && !P.Component->IsActive()) { P.Component->Activate(false); }
		// Zero spawn rate preserves existing particles during the configured fade/residue window.
		P.RenderState = TEXT("Rendered");
	}
}
