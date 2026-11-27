#include "HALHealingAreaGizmoVisualizer.h"
#include "HALExpandingHealingArea.h"
#include "HALHealingAreaGizmoComponent.h"
#include "Engine/World.h"
#include "PrimitiveDrawInterface.h"
#include "PrimitiveDrawingUtils.h"
#include "SceneView.h"

namespace
{
	class FHALHealingAreaGizmoVisualizer final : public FComponentVisualizer
	{
	public:
		void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override
		{
			const auto* Anchor = Cast<UHALHealingAreaGizmoComponent>(Component);
			const auto* Area = Anchor ? Cast<AHALExpandingHealingArea>(Anchor->GetOwner()) : nullptr;
			if (!Area || !View || !View->Family || !PDI || !Area->ShouldShowEditorGizmo() || !Area->GetWorld()
				|| Area->GetWorld()->IsGameWorld() || View->Family->EngineShowFlags.Game) { return; }
			const auto& Config = Area->GetAreaConfig();
			if (!FMath::IsFinite(Config.MaximumRadius) || Config.MaximumRadius <= 0
				|| !FMath::IsFinite(Config.HalfHeight) || Config.HalfHeight <= 0) { return; }
			// The real area is a world-XY cylinder; ignore every authored rotation/scale.
			const FVector Center = Area->GetActorLocation();
			const FLinearColor BoundsColor(.1f, .7f, .8f);
			DrawWireCylinder(PDI, Center, FVector::ForwardVector, FVector::RightVector, FVector::UpVector,
				BoundsColor, Config.MaximumRadius, Config.HalfHeight, 64, SDPG_World, 1.f);
			DrawCircle(PDI, Center, FVector::ForwardVector, FVector::RightVector, FLinearColor::Green,
				Config.MaximumRadius, 64, SDPG_World, 2.f);
			// Center-plane radius and full vertical extent make placement/height mismatches visible.
			PDI->DrawLine(Center - FVector::UpVector * Config.HalfHeight,
				Center + FVector::UpVector * Config.HalfHeight, BoundsColor, SDPG_World, 2.f);
			PDI->DrawLine(Center, Center + FVector::ForwardVector * Config.MaximumRadius, FLinearColor::Green, SDPG_World, 1.f);
		}
	};
}

TSharedRef<FComponentVisualizer> MakeHALHealingAreaGizmoVisualizer()
{
	return MakeShared<FHALHealingAreaGizmoVisualizer>();
}
