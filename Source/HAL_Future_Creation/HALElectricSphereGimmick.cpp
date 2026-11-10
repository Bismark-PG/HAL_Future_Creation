#include "HALElectricSphereGimmick.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

bool FHALElectricSphereConfig::Validate() const
{
	return FMath::IsFinite(Radius) && Radius >= 1 && FMath::IsFinite(BeginSeconds) && BeginSeconds >= 0
		&& FMath::IsFinite(LoopSeconds) && LoopSeconds >= .01f && FMath::IsFinite(OverSeconds) && OverSeconds >= 0;
}
EHALElectricSphereStage FHALElectricSphereConfig::StageAt(double Age) const
{
	return Age < BeginSeconds ? EHALElectricSphereStage::Begin
		: Age < double(BeginSeconds)+LoopSeconds ? EHALElectricSphereStage::Loop : EHALElectricSphereStage::Over;
}
AHALElectricSphereGimmick::AHALElectricSphereGimmick()
{
	SphereField=CreateDefaultSubobject<USphereComponent>(TEXT("SphereField"));
	SphereField->SetupAttachment(SceneRoot);
	SphereField->SetCollisionObjectType(ECC_WorldDynamic);
	SphereField->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereField->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Overlap);
	SphereField->SetGenerateOverlapEvents(false);
	UpdateGeometry();
}
void AHALElectricSphereGimmick::UpdateGeometry()
{
	// Retain inherited native identities for serialization, but never expose wall collision/visuals.
	for (UPrimitiveComponent* C : TArray<UPrimitiveComponent*>{PillarA.Get(),PillarB.Get(),ElectricField.Get(),PillarVisualA.Get(),PillarVisualB.Get(),WallVisual.Get()})
	{ C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetVisibility(false); C->SetHiddenInGame(true); }
	if (SphereField)
	{
		SphereField->SetSphereRadius(SphereConfig.Validate() ? SphereConfig.Radius : 1);
		SphereField->SetCollisionEnabled(IsContactActive() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}
UPrimitiveComponent* AHALElectricSphereGimmick::GetContactSurface() const { return SphereField; }
bool AHALElectricSphereGimmick::IsContactActive() const
{
	if (StartedAt<0 || !SphereConfig.Validate() || Stage!=EHALElectricSphereStage::Loop || !GetWorld()) { return false; }
	const auto* GS=GetWorld()->GetGameState();
	const double Now=HasAuthority() || !GS ? GetWorld()->GetTimeSeconds() : GS->GetServerWorldTimeSeconds();
	return SphereConfig.StageAt(Now-StartedAt)==EHALElectricSphereStage::Loop;
}
FVector AHALElectricSphereGimmick::GetPushDirection(UPrimitiveComponent*,const FVector& Current,const FVector& Previous,const FVector& Velocity) const
{
	if ((Current-Previous).SizeSquared2D()>.01) { return -(Current-Previous).GetSafeNormal2D(); }
	if (Velocity.SizeSquared2D()>.01) { return -Velocity.GetSafeNormal2D(); }
	const FVector Radial=(Current-SphereField->GetComponentLocation()).GetSafeNormal2D();
	return Radial.IsNearlyZero() ? FVector::ForwardVector : Radial;
}
bool AHALElectricSphereGimmick::ConfigureSphere(const FHALElectricSphereConfig& InConfig)
{
	if (!HasAuthority() || HasActorBegunPlay() || !InConfig.Validate()) { return false; }
	SphereConfig=InConfig; UpdateGeometry(); return true;
}
void AHALElectricSphereGimmick::BeginPlay()
{
	Super::BeginPlay();
	bLifecycleStarted=true;
	if (!SphereConfig.Validate())
	{
		UE_LOG(LogTemp,Error,TEXT("Electric sphere %s: invalid radius/timing; disabled."),*GetName());
		if (HasAuthority()) { Destroy(); } return;
	}
	if (HasAuthority()) { StartedAt=GetWorld()->GetTimeSeconds(); ForceNetUpdate(); }
	RefreshLifecycle();
	if (!IsActorBeingDestroyed()) { GetWorldTimerManager().SetTimer(LifecycleTimer,this,&ThisClass::RefreshLifecycle,.01f,true); }
}
void AHALElectricSphereGimmick::RefreshLifecycle()
{
	if (StartedAt<0 || !SphereConfig.Validate() || !bLifecycleStarted) { return; }
	const auto* GS=GetWorld()->GetGameState();
	const double Now=HasAuthority() || !GS ? GetWorld()->GetTimeSeconds() : GS->GetServerWorldTimeSeconds();
	const double Age=FMath::Max(0.,Now-StartedAt);
	const auto Next=SphereConfig.StageAt(Age);
	if (!bPresented || Stage!=Next)
	{
		Stage=Next; bPresented=true; UpdateGeometry();
		const double Offset=Stage==EHALElectricSphereStage::Begin ? 0 : Stage==EHALElectricSphereStage::Loop ? SphereConfig.BeginSeconds : double(SphereConfig.BeginSeconds)+SphereConfig.LoopSeconds;
		const float Duration=Stage==EHALElectricSphereStage::Begin ? SphereConfig.BeginSeconds : Stage==EHALElectricSphereStage::Loop ? SphereConfig.LoopSeconds : SphereConfig.OverSeconds;
		if (GetNetMode()!=NM_DedicatedServer) { OnStageChanged(Stage,float(Age-Offset),Duration,SphereField->GetScaledSphereRadius()); }
	}
	if (bDrawDebug && GetNetMode()!=NM_DedicatedServer
		&& Age<double(SphereConfig.BeginSeconds)+SphereConfig.LoopSeconds+SphereConfig.OverSeconds)
	{
		DrawDebugSphere(GetWorld(),SphereField->GetComponentLocation(),SphereField->GetScaledSphereRadius(),24,
			Stage==EHALElectricSphereStage::Loop ? FColor::Blue : FColor::Red,false,0.f,0,2.f);
	}
	if (HasAuthority() && Age>=double(SphereConfig.BeginSeconds)+SphereConfig.LoopSeconds+SphereConfig.OverSeconds) { Destroy(); }
}
void AHALElectricSphereGimmick::OnRep_Lifecycle() { UpdateGeometry(); RefreshLifecycle(); }
void AHALElectricSphereGimmick::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetWorld()) { GetWorldTimerManager().ClearTimer(LifecycleTimer); }
	Super::EndPlay(Reason);
}
void AHALElectricSphereGimmick::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALElectricSphereGimmick,SphereConfig);
	DOREPLIFETIME(AHALElectricSphereGimmick,StartedAt);
}
