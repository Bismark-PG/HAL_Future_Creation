#include "HALSmokeScreen.h"
#include "HALSmokeScreenAbility.h"
#include "HALSmokeScreenView.h"
#include "HALSlipperyTrail.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameFlowPermissions.h"
#include "HALGameplayTags.h"
#include "TestVehiclePawn.h"
#include "BasicBallActor.h"
#include "VehicleHealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

namespace
{
	TAutoConsoleVariable<int32> LogSmoke(TEXT("hal.SmokeLog"),0,TEXT("Car08 casts, pause/resume and authority vision source counts. 0=off."));
	TAutoConsoleVariable<int32> DrawSmoke(TEXT("hal.SmokeDebug"),0,TEXT("0=off; 1=draw footprint/outlet; 2=also bypass visual coverage for outlet calibration. Gameplay unchanged."));
	FCollisionObjectQueryParams SmokeObjects()
	{ FCollisionObjectQueryParams O; O.AddObjectTypesToQuery(ECC_WorldStatic); O.AddObjectTypesToQuery(ECC_WorldDynamic); return O; }
	bool SmokeSolid(const FHitResult& Hit)
	{
		const auto* C=Hit.GetComponent();
		return C&&C->IsPhysicsCollisionEnabled()&&!Cast<ATestVehiclePawn>(Hit.GetActor())&&!Cast<ABasicBallActor>(Hit.GetActor())
			&&(C->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block||C->GetCollisionResponseToChannel(ECC_PhysicsBody)==ECR_Block);
	}
	bool LiveVehicle(const ATestVehiclePawn* V)
	{
		const auto* H=IsValid(V)?V->FindComponentByClass<UVehicleHealthComponent>():nullptr;
		return H&&H->IsHealthReady()&&H->GetCurrentHP()>0&&V->CanReceiveSkillEffects();
	}
}
AHALSmokeScreen::AHALSmokeScreen()
{
	bReplicates=true; bAlwaysRelevant=true; SetReplicateMovement(false); SetNetUpdateFrequency(12);
	PrimaryActorTick.bCanEverTick=false;
}
bool AHALSmokeScreen::ClearLine(const UWorld& W,const AActor* Ignore,const FVector& From,const FVector& To)
{
	if(From.Equals(To,.1)) { return true; }
	TArray<FHitResult> Hits;
	W.LineTraceMultiByObjectType(Hits,From,To,SmokeObjects(),FCollisionQueryParams(SCENE_QUERY_STAT(SmokeWall),false,Ignore));
	for(const auto& H:Hits) { if(SmokeSolid(H)&&(H.bStartPenetrating||H.Time<.999f)) { return false; } }
	return true;
}
bool AHALSmokeScreen::FindOutletGround(const ATestVehiclePawn& V,const FHALSmokeScreenRules& R,FVector& Out)
{
	const auto* ASC=V.GetAbilitySystemComponent();
	if(!AHALSlipperyTrail::CanLay(V)||(ASC&&ASC->HasMatchingGameplayTag(HALGameplayTags::State_Stun))) { return false; }
	const FVector Outlet=V.GetActorTransform().TransformPosition(R.OutletOffset);
	TArray<FHitResult> Hits;
	V.GetWorld()->LineTraceMultiByObjectType(Hits,Outlet+FVector(0,0,25),Outlet-FVector(0,0,R.Height+600),
		SmokeObjects(),FCollisionQueryParams(SCENE_QUERY_STAT(SmokeOutletGround),false,&V));
	for(const auto& H:Hits) if(SmokeSolid(H))
	{
		if(H.bStartPenetrating||H.ImpactNormal.Z<.55f||H.GetComponent()->IsSimulatingPhysics()) { return false; }
		Out=H.ImpactPoint; return true;
	}
	return false;
}
bool AHALSmokeScreen::Start(const UHALSmokeScreenConfig& C,const FVector& Ground)
{
	FString Error;
	if(!HasAuthority()||StartTime>=0||!Cast<ATestVehiclePawn>(GetOwner())||!C.Validate(Error)||Ground.ContainsNaN()) { return false; }
	Rules=C.Rules; FX=C.FX; StartTime=HALSmoke::ServerTime(GetWorld()); bEmitting=true;
	AddPuff(Ground,0); OnRep_Smoke();
	UE_CLOG(LogSmoke.GetValueOnGameThread()!=0,LogTemp,Log,TEXT("Smoke started Owner=%s Emit=%.2f Puff=%.2f"),*GetNameSafe(GetOwner()),Rules.EmissionSeconds,Rules.PuffSeconds);
	return true;
}
void AHALSmokeScreen::OnRep_Smoke()
{
	if(StartTime<0||!Rules.IsValid()||GetWorld()->bIsTearingDown) { return; }
	if(!GetWorldTimerManager().IsTimerActive(UpdateTimer))
	{
		FTimerManagerTimerParameters P; P.bLoop=true; P.bMaxOncePerFrame=true;
		GetWorldTimerManager().SetTimer(UpdateTimer,this,&ThisClass::Update,Rules.UpdateSeconds,P);
	}
	if(FApp::CanEverRender()) { UHALSmokeViewSubsystem::EnsureForWorld(GetWorld()); UpdateJet(); }
}
void AHALSmokeScreen::ClipPuff(FHALSmokePuff& P)
{
	P.WallDistances.SetNum(Rules.WallRays);
	const FVector From=FVector(P.Ground)+FVector(0,0,FMath::Min(100.f,Rules.Height*.3f));
	for(int32 I=0;I<Rules.WallRays;++I)
	{
		const float Angle=2*PI*I/Rules.WallRays;
		const FVector To=From+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Rules.MaximumRadius;
		TArray<FHitResult> Hits;
		GetWorld()->LineTraceMultiByObjectType(Hits,From,To,SmokeObjects(),FCollisionQueryParams(SCENE_QUERY_STAT(SmokeClip),false,GetOwner()));
		float Distance=Rules.MaximumRadius;
		for(const auto& H:Hits) { if(SmokeSolid(H)) { Distance=FMath::Min(Distance,H.bStartPenetrating?0.f:FMath::Max(0.f,float(H.Distance)-2)); } }
		P.WallDistances[I]=uint16(FMath::FloorToInt(Distance));
	}
}
void AHALSmokeScreen::AddPuff(const FVector& Ground,float Age)
{
	if(Puffs.Num()>=Rules.MaximumPuffs) { return; }
	auto& P=Puffs.AddDefaulted_GetRef(); P.Ground=Ground; P.BirthAge=Age; ClipPuff(P); ForceNetUpdate();
}
float AHALSmokeScreen::Influence(const AActor& Target,bool bScreen,double Now,const FVector* Previous) const
{
	if(StartTime<0||IsActorBeingDestroyed()||(bScreen&&GetOwner()==&Target)||!IsValid(GetOwner())||GetOwner()->IsActorBeingDestroyed()) { return 0; }
	const FVector Current=HALSmoke::TargetPosition(Target);
	const bool Sweep=Previous&&FVector::DistSquared(*Previous,Current)<FMath::Square(2000.);
	float Strongest=0;
	for(const auto& P:Puffs)
	{
		const float Age=float(Now-StartTime)-P.BirthAge;
		FVector Position=Current;
		if(Sweep)
		{
			const FVector2D Step(Current-*Previous),Offset(FVector(P.Ground)-*Previous);
			const double T=Step.IsNearlyZero()?1.:FMath::Clamp(FVector2D::DotProduct(Step,Offset)/Step.SizeSquared(),0.,1.);
			const FVector Closest=FMath::Lerp(*Previous,Current,T);
			if(P.Strength(Rules,Age,Closest)>P.Strength(Rules,Age,Current)) { Position=Closest; }
		}
		const float S=P.Strength(Rules,Age,Position); if(S<=Strongest) { continue; }
		const FVector From=FVector(P.Ground)+FVector(0,0,FMath::Min(100.f,Rules.Height*.3f));
		// Keep an endpoint above the source floor, but retain vertical obstruction by another floor/ceiling.
		const FVector To(Position.X,Position.Y,FMath::Max(Position.Z,double(P.Ground.Z)+5));
		if(ClearLine(*GetWorld(),GetOwner(),From,To)) { Strongest=S; }
	}
	return Strongest;
}
void AHALSmokeScreen::UpdateReceivers()
{
	TSet<TWeakObjectPtr<UAbilitySystemComponent>> Inside;
	TMap<TWeakObjectPtr<ATestVehiclePawn>,FVector> Positions;
	for(TActorIterator<ATestVehiclePawn> It(GetWorld());It;++It)
	{
		auto* V=*It;
		if(V==GetOwner()||!LiveVehicle(V)||!HALGameFlow::Permissions(GetWorld(),V).bWorldInteractions) { continue; }
		auto* ASC=V->GetAbilitySystemComponent();
		if(!ASC||ASC->GetAvatarActor()!=V) { continue; }
		const FVector* Previous=LastPositions.Find(V); Positions.Add(V,HALSmoke::TargetPosition(*V));
		if(Influence(*V,true,HALSmoke::ServerTime(GetWorld()),Previous)<=.02f) { continue; }
		const auto* Existing=Receivers.Find(ASC);
		if(!Existing||!ASC->GetActiveGameplayEffect(*Existing))
		{
			auto Context=ASC->MakeEffectContext(); Context.AddSourceObject(this);
			const auto Spec=ASC->MakeOutgoingSpec(UHALSmokeObscuredEffect::StaticClass(),1,Context);
			if(!Spec.IsValid()) { continue; }
			const auto Handle=ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			if(!Handle.IsValid()) { continue; } Receivers.Add(ASC,Handle);
		}
		Inside.Add(ASC);
	}
	for(auto It=Receivers.CreateIterator();It;++It)
	{
		if(!Inside.Contains(It.Key())) { if(auto* ASC=It.Key().Get()) { ASC->RemoveActiveGameplayEffect(It.Value()); } It.RemoveCurrent(); }
	}
	LastPositions=MoveTemp(Positions);
}
void AHALSmokeScreen::ClearReceivers()
{
	for(const auto& Pair:Receivers) { if(auto* ASC=Pair.Key.Get()) { ASC->RemoveActiveGameplayEffect(Pair.Value); } }
	Receivers.Reset(); LastPositions.Reset();
}
void AHALSmokeScreen::Update()
{
	const double Now=HALSmoke::ServerTime(GetWorld()); const float Age=float(Now-StartTime);
	if(HasAuthority())
	{
		const auto* V=Cast<ATestVehiclePawn>(GetOwner());
		if(!LiveVehicle(V)||!HALGameFlow::Permissions(GetWorld(),V).bAbilities)
		{ Destroy(); return; }
		FVector Ground; const bool Emitting=Age<Rules.EmissionSeconds&&FindOutletGround(*V,Rules,Ground);
		if(Emitting!=bEmitting)
		{
			bEmitting=Emitting; ForceNetUpdate();
			UE_CLOG(LogSmoke.GetValueOnGameThread()!=0,LogTemp,Log,TEXT("Smoke Owner=%s Emitting=%d Age=%.2f"),*GetNameSafe(V),bEmitting,Age);
		}
		if(bEmitting&&(Puffs.IsEmpty()||FVector::DistSquared2D(Puffs.Last().Ground,Ground)>=FMath::Square(Rules.SampleSpacing)
			||Age-Puffs.Last().BirthAge>=Rules.StationaryInterval)) { AddPuff(Ground,Age); }
		const int32 Removed=Puffs.RemoveAll([&](const auto& P){return Age-P.BirthAge>=Rules.PuffSeconds;});
		if(Removed) { ForceNetUpdate(); }
		if (!bEmissionWindowEnded && Age >= Rules.EmissionSeconds)
		{
			bEmissionWindowEnded = true;
			OnEmissionEnded.Broadcast(this);
		}
		if(Age>=Rules.EmissionSeconds&&Puffs.IsEmpty()) { Destroy(); return; }
		if(Now-LastWallRefresh>=Rules.WallRefreshSeconds)
		{
			LastWallRefresh=Now;
			for(auto& P:Puffs) { const auto Before=P.WallDistances; ClipPuff(P); if(Before!=P.WallDistances) { ForceNetUpdate(); } }
		}
		const int32 Before=Receivers.Num(); UpdateReceivers();
		UE_CLOG(LogSmoke.GetValueOnGameThread()!=0&&Before!=Receivers.Num(),LogTemp,Log,TEXT("Smoke Owner=%s VisionSources=%d Puffs=%d"),*GetNameSafe(V),Receivers.Num(),Puffs.Num());
	}
	if(FApp::CanEverRender()) { UHALSmokeViewSubsystem::EnsureForWorld(GetWorld()); UpdateJet(); }
#if ENABLE_DRAW_DEBUG
	if(DrawSmoke.GetValueOnGameThread()!=0)
	{
		for(const auto& P:Puffs) for(int32 I=0;I<Rules.WallRays;++I)
		{
			const auto Vertex=[&](int32 J) { const float A=2*PI*J/Rules.WallRays; const float R=FMath::Min(Rules.Radius(Age-P.BirthAge),P.Boundary(A)); return FVector(P.Ground)+FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,5); };
			DrawDebugLine(GetWorld(),Vertex(I),Vertex(I+1),FColor::Silver,false,Rules.UpdateSeconds,0,2);
		}
		if(GetOwner()) { const auto Outlet=GetOwner()->GetActorTransform().TransformPosition(Rules.OutletOffset); DrawDebugDirectionalArrow(GetWorld(),Outlet,Outlet+FVector(0,0,100),20,FColor::Orange,false,Rules.UpdateSeconds,0,3); }
	}
#endif
}
void AHALSmokeScreen::UpdateJet()
{
	if(!IsValid(GetOwner())||!FX.JetSystem||!FApp::CanEverRender()) { return; }
	if(!Jet)
	{
		Jet=UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),FX.JetSystem,GetOwner()->GetActorLocation(),FRotator::ZeroRotator,FVector(1),false,false,ENCPoolMethod::None,false);
		if(!Jet) { return; } Jet->SetVariableFloat(TEXT("User.SpawnRate"),0); Jet->Activate(true);
	}
	const auto Transform=GetOwner()->GetActorTransform(); Jet->SetWorldLocation(Transform.TransformPosition(Rules.OutletOffset));
	const bool Current=bEmitting&&HALSmoke::ServerTime(GetWorld())<StartTime+Rules.EmissionSeconds;
	Jet->SetVariableFloat(TEXT("User.SpawnRate"),Current?FX.JetRate:0);
	Jet->SetVariableLinearColor(TEXT("User.SmokeColor"),FX.LightColor);
	const FVector Velocity=Transform.GetUnitAxis(EAxis::Z)*FX.UpwardSpeed-Transform.GetUnitAxis(EAxis::X)*FX.BackwardSpeed;
	Jet->SetVariableVec3(TEXT("User.VelocityMin"),Velocity-FVector(45,45,15));
	Jet->SetVariableVec3(TEXT("User.VelocityMax"),Velocity+FVector(45,45,80));
}
void AHALSmokeScreen::Cleanup()
{
	GetWorldTimerManager().ClearTimer(UpdateTimer); if(HasAuthority()) { ClearReceivers(); }
	UHALSmokeViewSubsystem::FadeForLocalPlayers(*this);
	if(Jet)
	{
		if(GetWorld()->bIsTearingDown) { Jet->DestroyComponent(); }
		else { Jet->SetVariableFloat(TEXT("User.SpawnRate"),0); Jet->SetAutoDestroy(true); Jet->Deactivate(); }
		Jet=nullptr;
	}

}
void AHALSmokeScreen::Destroyed()
{
	// Aborted spawning / pre-BeginPlay destruction does not invoke EndPlay. Source GEs must still be released.
	Cleanup(); Super::Destroyed();
}
void AHALSmokeScreen::EndPlay(const EEndPlayReason::Type Reason)
{
	Cleanup();
	Super::EndPlay(Reason);
}
void AHALSmokeScreen::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AHALSmokeScreen,Rules); DOREPLIFETIME(AHALSmokeScreen,FX);
	DOREPLIFETIME(AHALSmokeScreen,StartTime); DOREPLIFETIME(AHALSmokeScreen,Puffs); DOREPLIFETIME(AHALSmokeScreen,bEmitting);
}
