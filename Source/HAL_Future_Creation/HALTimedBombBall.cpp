#include "HALTimedBombBall.h"
#include "ArcadeVehicleMovementComponent.h"
#include "BallControlComponent.h"
#include "HALAbilitySystemComponent.h"
#include "HALBombActor.h"
#include "HALGameFlowPermissions.h"
#include "HALGameplayTags.h"
#include "HALHookComponent.h"
#include "TestVehiclePawn.h"
#include "VehicleRecoveryComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "EventManager.h"
#include "EventsData.h"
#include "TimerManager.h"

namespace
{
	FVector BumperCenter(const AActor& Car, float Radius)
	{
		const auto* Body = CastChecked<UPrimitiveComponent>(Car.GetRootComponent());
		const FVector Forward = Car.GetActorForwardVector().GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
		FVector Front;
		const FVector Outside = Car.GetActorLocation() + Forward * (Body->Bounds.SphereRadius + Radius);
		if (Body->GetClosestPointOnCollision(Outside, Front) < 0.f) { Front = Car.GetActorLocation() + Forward * Body->Bounds.BoxExtent.GetAbsMax(); }
		return Front + Forward * (.78f * Radius) + FVector::UpVector * (.18f * Radius);
	}

	bool Separated(AActor& A, AActor& B)
	{
		auto* First = Cast<UPrimitiveComponent>(A.GetRootComponent());
		auto* Second = Cast<UPrimitiveComponent>(B.GetRootComponent());
		FVector NearFirst, NearSecond;
		return First && Second && !First->ComponentOverlapComponent(Second,Second->GetComponentLocation(),Second->GetComponentQuat(),
			FCollisionQueryParams(SCENE_QUERY_STAT(ClaspSeparation),false)) && First->GetClosestPointOnCollision(B.GetActorLocation(), NearFirst) >= 0.f
			&& Second->GetClosestPointOnCollision(NearFirst, NearSecond) > 20.f;
	}
}

bool UHALTimedBombBallDefinition::Validate(FString& Error) const
{
	if (!Super::Validate(Error)) { return false; }
	if (!FMath::IsWithinInclusive(FuseSeconds,.25f,120.f) || !FMath::IsWithinInclusive(UrgentSeconds,0.f,120.f)
		|| !FMath::IsWithinInclusive(GroundBlastRadius,1.f,5000.f) || !FMath::IsWithinInclusive(PatrolSpeed,0.f,4000.f)
		|| !FMath::IsWithinInclusive(PatrolTurnRate,0.f,180.f) || !FMath::IsWithinInclusive(MinimumRamSpeed,1.f,5000.f)
		|| !FMath::IsWithinInclusive(MinimumClosingSpeed,1.f,5000.f) || !FMath::IsWithinInclusive(ReturnLockSeconds,0.f,5.f)
		|| !FMath::IsWithinInclusive(ClaspTravelSeconds,.12f,1.5f) || !FMath::IsWithinInclusive(JawOpening,.05f,.8f)
		|| !FMath::IsWithinInclusive(DialSegments,4,24) || !JawMesh || !DialMesh || !PatrolWakeMaterial || Gameplay.Impact.Version != EHALImpactVersion::TieredV1)
	{ Error = TEXT("Invalid timed-ball fuse, patrol, transfer, dial meshes or impact settings."); return false; }
	return true;
}

AHALTimedBombBall::AHALTimedBombBall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Assembly = CreateDefaultSubobject<USceneComponent>(TEXT("ClaspAssembly"));
	Assembly->SetupAttachment(PhysicsRoot);
	Assembly->SetAbsolute(true, true, true);
	auto Part = [this](const TCHAR* Name)
	{
		auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Assembly);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		return Mesh;
	};
	Shell = Part(TEXT("GraphiteCore"));
	LeftJaw = Part(TEXT("LeftMagnet"));
	RightJaw = Part(TEXT("RightMagnet"));
	Dial = Part(TEXT("CountdownDial"));
	Wake = Part(TEXT("PatrolStrokes"));
	Wake->SetCastShadow(false);
}

UHALSpecialBallDefinition* AHALTimedBombBall::DefaultType() const
{
	return GetDefault<UHALMatchBallSettings>()->TimedBombBallType.LoadSynchronous();
}

void AHALTimedBombBall::BeginPlay()
{
	FreeResponses = PhysicsRoot->GetCollisionResponseToChannels();
	bFreeGravity = PhysicsRoot->IsGravityEnabled();
	Super::BeginPlay();
	if (!Type() || !IsConfigurationValid()) { return; }
	if (HasAuthority())
	{
		Heading = GetActorForwardVector().GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
		PhysicsRoot->OnComponentHit.AddUniqueDynamic(this, &ThisClass::OnContact);
		LastPatrolTime = ServerNow();
		GetWorldTimerManager().SetTimer(PatrolTimer, this, &ThisClass::Patrol, .04f, true);
		if (auto* Scene = GetWorld()->GetPhysicsScene())
		{
			Scene->GetSolver()->GetEventManager()->RegisterHandler<Chaos::FCollisionEventData>(
				Chaos::EEventType::Collision,this,&ThisClass::OnVehicleCollisions);
			bWatchingCollisions = true;
		}
	}
	if (GetNetMode() == NM_DedicatedServer) { SetActorTickEnabled(false); return; }
	// Keep the replicated root and its ordinary interpolation as the free-ball position source.
	VisualMesh->SetHiddenInGame(true);
	Shell->SetStaticMesh(VisualMesh->GetStaticMesh());
	Shell->SetMaterial(0, VisualMesh->GetMaterial(0));
	Shell->SetRelativeScale3D(FVector(.81,.81,.76));
	LeftJaw->SetStaticMesh(Type()->JawMesh);
	RightJaw->SetStaticMesh(Type()->JawMesh);
	RightJaw->SetRelativeRotation(FRotator(0,180,0));
	auto* PaintMID = LeftJaw->CreateDynamicMaterialInstance(0);
	if (PaintMID)
	{
		PaintMID->SetVectorParameterValue(TEXT("CurrentColor"),Type()->CurrentColor);
		PaintMID->SetVectorParameterValue(TEXT("PoleTipColor"),Type()->PoleTipColor);
		RightJaw->SetMaterial(0, PaintMID);
	}
	Dial->SetStaticMesh(Type()->DialMesh);
	Dial->SetRelativeScale3D(FVector(.65,.65,1));
	Dial->SetRelativeLocation(FVector(0,0,35));
	Wake->SetStaticMesh(Type()->DialMesh);
	Wake->SetMaterial(0,Type()->PatrolWakeMaterial);
	if (Type()->PatrolWakeMaterial)
	{
		WakeMID = Wake->CreateDynamicMaterialInstance(0);
		WakeMID->SetVectorParameterValue(TEXT("LampColor"),Type()->CoronaColor);
		WakeMID->SetScalarParameterValue(TEXT("FlowSpeed"),Type()->SurfaceFlowSpeed);
	}
	Wake->SetRelativeScale3D(FVector(1.55,1.15,1));
	Wake->SetRelativeLocation(FVector(-53,0,-38));
	DialMID = Dial->CreateDynamicMaterialInstance(0);
	if (DialMID)
	{
		DialMID->SetVectorParameterValue(TEXT("LampColor"),Type()->CoronaColor);
		DialMID->SetVectorParameterValue(TEXT("AlertColor"),Type()->CurrentColor);
		DialMID->SetScalarParameterValue(TEXT("Segments"),Type()->DialSegments);
		DialMID->SetScalarParameterValue(TEXT("Glow"),Type()->Glow);
	}
	Paint();
}

void AHALTimedBombBall::OnContact(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent* OtherBody, FVector, const FHitResult& Hit)
{
	if (!HasAuthority() || !CanUseBall() || GetBallState() != EBasicBallState::Free || !IsValid(Other)) { return; }
	if (auto* Car = Cast<ATestVehiclePawn>(Other))
	{
		if (OtherBody == Car->GetRootComponent()) { TryMount(*Car, nullptr); }
		return;
	}
	const FVector N = Hit.ImpactNormal.GetSafeNormal2D();
	if (!OtherBody || OtherBody->IsSimulatingPhysics() || FMath::Abs(Hit.ImpactNormal.Z) > .55f || (Heading | N) >= 0.) { return; }
	Heading = Heading.MirrorByVector(N).GetSafeNormal2D();
}

void AHALTimedBombBall::Patrol()
{
	const double Now = ServerNow();
	const float Step = FMath::Clamp(Now-LastPatrolTime,0.,.15);
	LastPatrolTime = Now;
	if (!CanUseBall()) { return; }
	if (FuseEnd >= 0. && Now >= FuseEnd) { Detonate(); return; }
	if (AActor* Car = GetControlledBy())
	{
		for (auto It = ContactLocks.CreateIterator(); It; ++It)
		{
			AActor* Peer = It.Key().Get();
			if (!Peer || (Now >= It.Value() && Separated(*Car,*Peer))) { It.RemoveCurrent(); }
		}
		return;
	}
	if (FuseEnd >= 0. || !PhysicsRoot->IsSimulatingPhysics()) { return; }
	Heading = Heading.RotateAngleAxis(Type()->PatrolTurnRate * Step * FMath::Sin(.85 * Now), FVector::UpVector);
	const FVector Velocity = PhysicsRoot->GetPhysicsLinearVelocity();
	const FVector Desired = Heading * Type()->PatrolSpeed;
	// Preserve gravity and real wall/floor contacts; only propel the horizontal patrol.
	PhysicsRoot->SetPhysicsLinearVelocity(FVector(Desired.X,Desired.Y,Velocity.Z));
}

bool AHALTimedBombBall::TryMount(ATestVehiclePawn& Target, AActor* Passer)
{
	if (!HasAuthority() || !CanUseBall() || IsActorBeingDestroyed()) { return false; }
	if (ResolveDueFuse()) { return false; }
	if (!Target.CanReceiveSkillEffects() || UHALAbilitySystemComponent::IsActorStunned(&Target)
		|| (Passer ? GetControlledBy() != Passer : GetBallState() != EBasicBallState::Free)
		|| &Target == Passer || !PassesPickupLocks(&Target)) { return false; }
	if (auto* Recovery = Target.FindComponentByClass<UVehicleRecoveryComponent>(); Recovery && Recovery->IsExecuting()) { return false; }
	auto* Receiver = Target.FindComponentByClass<UBallControlComponent>();
	auto* Previous = Passer ? Passer->FindComponentByClass<UBallControlComponent>() : nullptr;
	if (auto* Displaced = Receiver ? Cast<AHALTimedBombBall>(Receiver->GetHeldBall()) : nullptr)
	{
		if (Displaced->ResolveDueFuse() && (!Target.CanReceiveSkillEffects() || bConsumed)) { return false; }
	}
	const float Radius = PhysicsRoot->GetScaledSphereRadius();
	const FVector Start = Passer ? BumperCenter(*Passer,Radius) : GetActorLocation();
	if (!Receiver || (Passer && !Previous) || !Receiver->MountContactBomb(*this, Previous)) { return false; }
	const double Now = ServerNow();
	if (FuseEnd < 0.)
	{
		FuseEnd = Now + Type()->FuseSeconds;
		GetWorldTimerManager().SetTimer(FuseTimer,this,&ThisClass::Detonate,Type()->FuseSeconds,false);
	}
	ContactLocks.Reset();
	if (Passer)
	{
		ContactLocks.Add(Passer, Now + Type()->ReturnLockSeconds);
		Credit.InstigatorActor = Passer;
		const APawn* Pawn = CastChecked<APawn>(Passer);
		Credit.InstigatorPlayerState = Pawn->GetPlayerState();
		Credit.InstigatorPlayerId = Pawn->GetPlayerState() ? Pawn->GetPlayerState()->GetPlayerId() : INDEX_NONE;
	}
	Transfer.Started = Now;
	Transfer.From = Start;
	Transfer.FromCar = Passer;
	Transfer.To = &Target;
	CommitContactHolder(Target);
	PhysicsRoot->SetWorldLocation(BumperCenter(Target,Radius),false,nullptr,ETeleportType::TeleportPhysics);
	PhysicsRoot->SetPhysicsLinearVelocity(Target.GetVelocity());
	UE_LOG(LogTemp,Display,TEXT("TimedBall mount %s from=%s to=%s fuseEnd=%.3f"),*GetName(),*GetNameSafe(Passer),*Target.GetName(),FuseEnd);
	return true;
}

void AHALTimedBombBall::OnVehicleCollisions(const Chaos::FCollisionEventData& Events)
{
	auto* Driver = Cast<ATestVehiclePawn>(GetControlledBy());
	if (!Driver || bConsumed || Events.CollisionData.TimeCreated <= LastCollisionBatch) { return; }
	LastCollisionBatch = Events.CollisionData.TimeCreated;
	auto* Scene = GetWorld()->GetPhysicsScene();
	// Like the bull charge, use the contact's own solver step. By OnComponentHit the
	// two cars can already be travelling together and have almost zero closing speed.
	for (const auto& Contact : Events.CollisionData.AllCollisionsArray)
	{
		if (Contact.bProbe || !Contact.Proxy1 || !Contact.Proxy2) { continue; }
		auto* A = Scene->GetOwningComponent<UPrimitiveComponent>(Contact.Proxy1);
		auto* B = Scene->GetOwningComponent<UPrimitiveComponent>(Contact.Proxy2);
		const bool bDriverIsA = A == Driver->GetRootComponent();
		if (!bDriverIsA && B != Driver->GetRootComponent()) { continue; }
		auto* OtherBody = bDriverIsA ? B : A;
		auto* Target = OtherBody ? Cast<ATestVehiclePawn>(OtherBody->GetOwner()) : nullptr;
		if (!Target || Target == Driver || OtherBody != Target->GetRootComponent()) { continue; }
		const FVector BeforeA = Contact.Velocity1 - Contact.DeltaVelocity1;
		const FVector BeforeB = Contact.Velocity2 - Contact.DeltaVelocity2;
		if (OnRam(*Driver,*Target,bDriverIsA ? BeforeA : BeforeB,bDriverIsA ? BeforeB : BeforeA)) { return; }
	}
}

bool AHALTimedBombBall::OnRam(ATestVehiclePawn& Driver, ATestVehiclePawn& Target, const FVector& DriverBefore, const FVector& TargetBefore)
{
	if (ContactLocks.Contains(&Target) || bConsumed) { return false; }
	auto* Motion = Driver.FindComponentByClass<UArcadeVehicleMovementComponent>();
	if (!Motion || !Driver.CanReceiveSkillEffects() || UHALAbilitySystemComponent::IsActorStunned(&Driver)
		|| UHALHookComponent::BlocksActionsOn(&Driver) || Motion->IsBounceBackActive() || !Motion->GetStabilityObservation().bDrivingAllowed) { return false; }
	const auto Input = Motion->GetAcceptedDrivingInput();
	if (FMath::Max(Input.Throttle,Input.Brake) < .1f && !Input.bHandbrake) { return false; }
	const FVector Toward = (Target.GetActorLocation() - Driver.GetActorLocation()).GetSafeNormal2D();
	return (DriverBefore | Toward) >= Type()->MinimumRamSpeed && ((DriverBefore - TargetBefore) | Toward) >= Type()->MinimumClosingSpeed
		&& TryMount(Target, &Driver);
}

void AHALTimedBombBall::OnAuthorityBallStateChanged(EBasicBallState Previous)
{
	Super::OnAuthorityBallStateChanged(Previous);
	if (GetControlledBy())
	{
		PhysicsRoot->SetCollisionResponseToAllChannels(ECR_Ignore);
		PhysicsRoot->SetEnableGravity(false);
	}
	else if (Previous == EBasicBallState::Controlled)
	{
		PhysicsRoot->SetCollisionResponseToChannels(FreeResponses);
		PhysicsRoot->SetEnableGravity(bFreeGravity);
		Transfer.To = nullptr;
		Transfer.FromCar = nullptr;
		ContactLocks.Reset();
	}
}

bool AHALTimedBombBall::ResolveDueFuse()
{
	if (!HasAuthority() || FuseEnd < 0. || ServerNow() < FuseEnd) { return false; }
	Detonate();
	return true;
}

bool AHALTimedBombBall::ReleaseFromControl(AActor* ExpectedHolder, float Lock)
{
	if (HasAuthority() && ExpectedHolder && GetControlledBy() == ExpectedHolder)
	{
		if (bConsumed || ResolveDueFuse()) { return false; }
		PhysicsRoot->SetWorldLocation(BumperCenter(*ExpectedHolder,PhysicsRoot->GetScaledSphereRadius()),false,nullptr,ETeleportType::TeleportPhysics);
	}
	return Super::ReleaseFromControl(ExpectedHolder,Lock);
}

bool AHALTimedBombBall::HandleVehicleExit(const AActor* LeavingVehicle)
{
	if (LeavingVehicle && LeavingVehicle == GetControlledBy() && ResolveDueFuse()) { return false; }
	return Super::HandleVehicleExit(LeavingVehicle);
}

void AHALTimedBombBall::Detonate()
{
	if (!HasAuthority() || bConsumed || FuseEnd < 0.) { return; }
	bConsumed = true;
	AActor* Car = GetControlledBy();
	const FVector Center = IsValid(Car) ? BumperCenter(*Car,PhysicsRoot->GetScaledSphereRadius()) : GetActorLocation();
	Credit.SourceActor = this;
	Credit.SourceTag = HALGameplayTags::Combat_Source_Ball;
	Credit.FixedDamage = Type()->Gameplay.Damage.VehicleHitDamage;
	Credit.Impact = Type()->Gameplay.Impact;
	Credit.bCanHitInstigator = true;
	FHALCombatAction Action;
	// Publish the cue before damage can finish the match and retire the ball.
	PlayCue(Car ? EHALBallCue::MountedImpact : EHALBallCue::Impact,Center,
		Car ? PhysicsRoot->GetScaledSphereRadius() * Type()->CoronaSize : Type()->GroundBlastRadius);
	if (Action.Begin(*this,Credit))
	{
		const FHALBlastArea Blast{Center, Type()->GroundBlastRadius};
		if (IsValid(Car))
		{
			Blast.HitBody(*this, Action, *CastChecked<UPrimitiveComponent>(Car->GetRootComponent()), Center);
		}
		else { Blast.Apply(*this, Action); }
		Action.Close();
	}
	UE_LOG(LogTemp,Display,TEXT("TimedBall detonate %s holder=%s creditedPlayer=%d"),*GetName(),*GetNameSafe(Car),Credit.InstigatorPlayerId);
	Destroy();
}

void AHALTimedBombBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Paint();
}

void AHALTimedBombBall::Paint()
{
	if (Assembly->IsVisible() != HasStarted()) { Assembly->SetVisibility(HasStarted(),true); }
	if (!HasStarted() || !Type()) { return; }
	const double Now = ServerNow();
	const float Radius = PhysicsRoot->GetScaledSphereRadius();
	const bool Armed = FuseEnd >= 0.;
	const float Left = Armed ? FMath::Max(0., FuseEnd-Now) : Type()->FuseSeconds;
	const float Urgency = Armed && Type()->UrgentSeconds > 0.f ? 1.f-FMath::Clamp(Left/Type()->UrgentSeconds,0.f,1.f) : 0.f;
	const float Phase = Type()->FuseSeconds - Left + 1.5f*Type()->UrgentSeconds*Urgency*Urgency;
	const float Beat = FMath::Pow(.5f+.5f*FMath::Cos(UE_TWO_PI*Phase),4.f);
	AActor* Car = GetControlledBy();
	const float Travel = Car && Transfer.To == Car ? FMath::Clamp((Now-Transfer.Started)/Type()->ClaspTravelSeconds,0.,1.) : 1.f;
	const float Jaw = Travel < .23f ? FMath::SmoothStep(0.f,.23f,Travel) : 1.f-FMath::SmoothStep(.68f,1.f,Travel);
	FVector Center = VisualMesh->GetComponentLocation();
	float Yaw = GetVelocity().SizeSquared2D() > 100.f ? GetVelocity().Rotation().Yaw : Assembly->GetComponentRotation().Yaw;
	if (Car)
	{
		const float Transit = FMath::SmoothStep(.17f,.83f,Travel);
		const FVector Start = IsValid(Transfer.FromCar) && Transit < 1.f ? BumperCenter(*Transfer.FromCar,Radius) : FVector(Transfer.From);
		Center = FMath::Lerp(Start,BumperCenter(*Car,Radius),Transit);
		Center.Z += Radius * .38f * FMath::Sin(UE_PI*Transit);
		Yaw = Car->GetActorRotation().Yaw;
	}
	Assembly->SetWorldTransform(FTransform(FRotator(0,Yaw,0),Center,FVector(Radius/50.f)));
	const float Spread = 50.f * (Car ? Type()->JawOpening * Jaw : .045f + .022f*FMath::Sin(Now*5.));
	LeftJaw->SetRelativeLocation(FVector(0,Spread,0));
	RightJaw->SetRelativeLocation(FVector(0,-Spread,0));
	if (WakeMID)
	{
		const bool bSnap = Car && Travel > .68f && Travel < 1.f;
		Wake->SetVisibility(bSnap || (!Armed && !Car && GetVelocity().SizeSquared2D() > 100.));
		if (Wake->IsVisible())
		{
			const float Flash = bSnap ? FMath::Sin(UE_PI*(Travel-.68f)/.32f) : 0.f;
			WakeMID->SetScalarParameterValue(TEXT("Snap"),Flash);
			Wake->SetRelativeLocation(bSnap ? FVector(-38,0,18) : FVector(-53,0,-38));
			Wake->SetRelativeScale3D(bSnap ? FVector(.8,.8,1) : FVector(1.55,1.15,1));
		}
	}
	if (DialMID)
	{
		DialMID->SetScalarParameterValue(TEXT("Remaining"),FMath::Clamp(Left/Type()->FuseSeconds,0.f,1.f));
		DialMID->SetScalarParameterValue(TEXT("Urgency"),Urgency);
		const bool bExpiring = !Armed && RemainingFreeSeconds() < Type()->WarningSeconds;
		const float IdlePulse = bExpiring ? .45f+.4f*FMath::Sin(Now*9.) : .24f+.16f*FMath::Sin(Now*4.);
		DialMID->SetScalarParameterValue(TEXT("Beat"),Armed ? Beat : IdlePulse);
	}
}

void AHALTimedBombBall::EndPlay(EEndPlayReason::Type Reason)
{
	if (bWatchingCollisions && GetWorld() && GetWorld()->GetPhysicsScene())
	{ GetWorld()->GetPhysicsScene()->GetSolver()->GetEventManager()->UnregisterHandler(Chaos::EEventType::Collision,this); }
	GetWorldTimerManager().ClearTimer(FuseTimer);
	GetWorldTimerManager().ClearTimer(PatrolTimer);
	Super::EndPlay(Reason);
}

void AHALTimedBombBall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALTimedBombBall,FuseEnd);
	DOREPLIFETIME(AHALTimedBombBall,Transfer);
}
