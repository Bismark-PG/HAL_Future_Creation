#include "HALPursuitTargetComponent.h"
#include "HALPursuitHUDWidget.h"
#include "HALVehiclePresentationComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "TestVehiclePawn.h"

void UHALPursuitTargetComponent::StopChains()
{
	if(Chains) { Chains->SetVisibility(false); if(Chains->GetInstanceCount()) { Chains->ClearInstances(); } }
	SetComponentTickEnabled(false);
}
void UHALPursuitTargetComponent::UpdateMarker()
{
	if(!FApp::CanEverRender()||GetNetMode()==NM_DedicatedServer||!GetOwner()) { return; }
	auto C=State.FX.Chains;
	if(const auto* V=Cast<ATestVehiclePawn>(GetOwner()))
	{ for(const auto& Fit:C.VehicleFits) if(Fit.VehicleDefinition.Get()==V->GetVehicleDefinition()) { C.CenterOffset=Fit.CenterOffset; C.BoundsScale=Fit.BoundsScale; break; } }
	const double Now=PresentationTime();
	const auto E=HALArrestChains::Envelope(C,State.bArrested,Now,State.ArrestStart,State.ReleaseStart);
	if(E.bFinished||!C.LinkMesh||!C.Material) { StopChains(); return; }
	if(State.bArrested) { UHALPursuitHUDSubsystem::EnsureForWorld(GetWorld(),State.FX.WidgetClass); }
	FBox Bounds(ForceInit);
	if(auto* P=GetOwner()->FindComponentByClass<UHALVehiclePresentationComponent>())
	{
		FHALVehicleFXTargetSnapshot Targets; P->CaptureStatusFXTargets(Targets);
		for(const auto& T:Targets.Targets) if(T.Id==C.PartId) { Bounds=T.VehicleBounds; break; }
	}
	else if(C.PartId==TEXT("Body"))
	{
		// Narrow fallback for native render fixtures and non-rigged test targets.
		if(const auto* Root=Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent())) { Bounds=Root->CalcBounds(FTransform::Identity).GetBox(); }
	}
	TArray<FTransform> Pose;
	const double Flow=HALArrestChains::FlowDistance(C,Now,State.ArrestStart,State.ReleaseStart);
	if(!HALArrestChains::BuildPose(Bounds,C,C.LinkMesh->GetBounds().BoxExtent*2,E.Looseness,Pose,Flow))
	{ if(Chains) { Chains->SetVisibility(false); } return; } // A late visual binding may become ready next frame.
	if(!Chains)
	{
		Chains=NewObject<UInstancedStaticMeshComponent>(GetOwner(),NAME_None,RF_Transient);
		Chains->SetupAttachment(GetOwner()->GetRootComponent()); Chains->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Chains->SetGenerateOverlapEvents(false); Chains->SetCastShadow(false); Chains->SetCanEverAffectNavigation(false);
		Chains->SetBoundsScale(1.1f); Chains->RegisterComponent(); // Conservative render-only allowance for wire WPO.
	}
	if(Chains->GetStaticMesh()!=C.LinkMesh) { Chains->ClearInstances(); Chains->SetStaticMesh(C.LinkMesh); }
	if(!Material||Material->Parent!=C.Material) { Material=UMaterialInstanceDynamic::Create(C.Material,this); Chains->SetMaterial(0,Material); }
	Chains->SetWorldTransform(GetOwner()->GetActorTransform());
	if(Chains->GetInstanceCount()!=Pose.Num()) { Chains->ClearInstances(); Chains->AddInstances(Pose,false,false); }
	else { Chains->BatchUpdateInstancesTransforms(0,Pose,false,true,true); }
	Chains->SetVisibility(E.Visibility>0);
	Material->SetVectorParameterValue(TEXT("DarkColor"),C.DarkColor); Material->SetVectorParameterValue(TEXT("MiddleColor"),C.MiddleColor);
	Material->SetVectorParameterValue(TEXT("LightColor"),C.LightColor); Material->SetVectorParameterValue(TEXT("LightDirection"),FLinearColor(C.LightDirection.GetSafeNormal()));
	Material->SetScalarParameterValue(TEXT("ShadowThreshold"),C.ShadowThreshold); Material->SetScalarParameterValue(TEXT("HighlightThreshold"),C.HighlightThreshold);
	Material->SetScalarParameterValue(TEXT("ToneSoftness"),C.ToneSoftness); Material->SetScalarParameterValue(TEXT("GlintStrength"),C.GlintStrength);
	Material->SetScalarParameterValue(TEXT("Visibility"),E.Visibility);
	Material->SetScalarParameterValue(TEXT("WireRadius"),C.WireRadius);
}
