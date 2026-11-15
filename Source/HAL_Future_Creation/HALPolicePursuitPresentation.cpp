#include "HALPolicePursuit.h"
#include "HALPursuitTargetComponent.h"
#include "HALPursuitHUDWidget.h"
#include "HALVehiclePresentationComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
namespace
{
	UStaticMeshComponent* Visual(AActor& Owner,UStaticMesh* Mesh,UMaterialInterface* Material)
	{
		auto* C=NewObject<UStaticMeshComponent>(&Owner); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false);
		C->SetStaticMesh(Mesh); if(Material) { C->SetMaterial(0,Material); } C->SetupAttachment(Owner.GetRootComponent()); C->RegisterComponent(); return C;
	}
}
void AHALPolicePursuit::UpdatePresentation()
{
	if(!FApp::CanEverRender()||GetNetMode()==NM_DedicatedServer||!GetOwner()||!State.FX.IsValid()) { return; }
	const auto& FX=State.FX; const float Age=float(ServerTime()-State.Start);
	if(Age>=State.Rules.Duration) { StopPresentation(); return; }
	UHALPursuitHUDSubsystem::EnsureForWorld(GetWorld(),FX.WidgetClass);
	if(!Range&&FX.RangeMesh&&FX.RangeMaterial)
	{
		Range=Visual(*this,FX.RangeMesh,FX.RangeMaterial); auto* M=UMaterialInstanceDynamic::Create(FX.RangeMaterial,this); Range->SetMaterial(0,M); Materials.Add(M);
		for(int32 I=0;I<2;++I)
		{
			auto* B=Visual(*this,FX.BeaconMesh,FX.SignalMaterial); Beacons.Add(B);
			auto* BM=FX.SignalMaterial?UMaterialInstanceDynamic::Create(FX.SignalMaterial,this):nullptr; Materials.Add(BM); if(BM) { B->SetMaterial(0,BM); }
			auto* L=NewObject<UPointLightComponent>(this); L->SetupAttachment(GetRootComponent()); L->SetCastShadows(false); L->SetIntensityUnits(ELightUnits::Lumens); L->RegisterComponent(); Lights.Add(L);
		}
		if(FX.SirenSound)
		{
			Audio=NewObject<UAudioComponent>(this); Audio->bAutoActivate=false; Audio->SetupAttachment(GetRootComponent()); Audio->SetSound(FX.SirenSound);
			Audio->bOverrideAttenuation=true; Audio->AttenuationOverrides.bAttenuate=true; Audio->AttenuationOverrides.bSpatialize=true;
			Audio->AttenuationOverrides.AttenuationShapeExtents=FVector(FX.SirenInnerRadius); Audio->AttenuationOverrides.FalloffDistance=FX.SirenFalloff;
			Audio->SetVolumeMultiplier(FX.SirenVolume); Audio->RegisterComponent(); Audio->Play();
		}
	}
	if(!Range||Materials.Num()!=3) { return; }
	const FVector Up=GetOwner()->GetActorUpVector(); FVector Center,Extent; GetOwner()->GetActorBounds(true,Center,Extent);
	FVector Roof=Center+Up*(FVector::DotProduct(Extent,Up.GetAbs())+FX.RoofLift); FVector Right=GetOwner()->GetActorRightVector();
	bool RoofReady=FX.bAutomaticRoof;
	if(auto* P=GetOwner()->FindComponentByClass<UHALVehiclePresentationComponent>())
	{
		FHALVehicleFXTargetSnapshot Targets; P->CaptureStatusFXTargets(Targets); FHALResolvedFXMount Mount;
		if(HALVehicleFX::ResolveMount(Targets,FX.RoofMount,Mount))
		{
			RoofReady=true; Roof=Mount.World.GetLocation();
			if(FX.bAutomaticRoof) { Roof=Mount.Mesh->Bounds.Origin+Up*(FVector::DotProduct(Mount.Mesh->Bounds.BoxExtent,Up.GetAbs())+FX.RoofLift); }
			else { Right=Mount.World.GetRotation().GetRightVector(); }
		}
	}
	for(int32 I=0;I<2;++I)
	{
		const float Wave=.5f+.5f*FMath::Cos(Age*UE_PI/FX.FlashHalfPeriod+I*UE_PI),Power=FMath::SmoothStep(.2f,.8f,Wave);
		const auto Color=I==0?FX.Red:FX.Blue; const FVector Position=Roof+Right*((I==0?-1:1)*FX.LightSpacing*.5f);
		Beacons[I]->SetVisibility(RoofReady); Beacons[I]->SetWorldLocation(Position); Beacons[I]->SetWorldScale3D(FVector(FX.BeaconRadius/50));
		Lights[I]->SetWorldLocation(Position); Lights[I]->SetLightColor(Color); Lights[I]->SetAttenuationRadius(FX.LightRadius); Lights[I]->SetIntensity(RoofReady?FX.LightIntensity*Power:0);
		if(Materials[I+1]) { Materials[I+1]->SetVectorParameterValue(TEXT("Color"),Color); Materials[I+1]->SetScalarParameterValue(TEXT("Glow"),FX.SignalGlow*(.05f+.95f*Power)); }
	}
	if(Audio) { Audio->SetWorldLocation(Roof); }
	FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Hit; const FVector At=GetOwner()->GetActorLocation(); FCollisionQueryParams Q(SCENE_QUERY_STAT(PursuitGroundFX),false,GetOwner());
	TArray<FHitResult> Hits; GetWorld()->LineTraceMultiByObjectType(Hits,At+FVector(0,0,100),At-FVector(0,0,1800),Objects,Q);
	bool Ground=false;
	for(const auto& H:Hits) if(const auto* C=H.GetComponent();C&&C->IsPhysicsCollisionEnabled()&&H.ImpactNormal.Z>.55f)
	{ Hit=H; Ground=true; break; }
	Range->SetVisibility(Ground); if(Ground) { Range->SetWorldLocation(Hit.ImpactPoint+Hit.ImpactNormal*FX.GroundLift); Range->SetWorldRotation(FQuat::FindBetweenNormals(FVector::UpVector,Hit.ImpactNormal)); }
	Range->SetWorldScale3D(FVector(State.Rules.Radius/100,State.Rules.Radius/100,1));
	if(auto* M=Materials[0].Get())
	{
		M->SetVectorParameterValue(TEXT("Color"),FX.Red); M->SetScalarParameterValue(TEXT("Opacity"),FX.RangeOpacity);
		M->SetScalarParameterValue(TEXT("RingOpacity"),FX.RangeRingOpacity); M->SetScalarParameterValue(TEXT("Time"),Age);
		M->SetScalarParameterValue(TEXT("ScanSpeed"),FX.ScanSpeed); M->SetScalarParameterValue(TEXT("ScanWidth"),FX.ScanWidth);
		M->SetScalarParameterValue(TEXT("ScanOpacity"),FX.ScanOpacity); M->SetScalarParameterValue(TEXT("RingWidth"),FX.RingWidth);
	}
}
void AHALPolicePursuit::StopPresentation()
{
	if(Audio) { Audio->Stop(); }
	if(Range) { Range->SetVisibility(false); }
	for(const auto& B:Beacons) { if(B) { B->SetVisibility(false); } }
	for(const auto& L:Lights) { if(L) { L->SetIntensity(0); } }
}
