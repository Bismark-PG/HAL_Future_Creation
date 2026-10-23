#include "SpringPunchGimmick.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "VehicleHealthComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "DrawDebugHelpers.h"
#include "CombatResolver.h"
#include "HALGameplayTags.h"
#include "VehicleKnockbackSettings.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Curves/CurveFloat.h"

namespace
{
	float EvaluatePunchCurve(const UCurveFloat* Curve, float Alpha)
	{
		if (Alpha <= 0) { return 0; }
		if (Alpha >= 1) { return 1; }
		if (!Curve || Curve->FloatCurve.GetNumKeys() == 0) { return Alpha; }
		const float Progress = Curve->GetFloatValue(Alpha);
		return FMath::IsFinite(Progress) ? FMath::Clamp(Progress, 0.f, 1.f) : Alpha;
	}
}

ASpringPunchGimmick::ASpringPunchGimmick()
{
	// Tick は使用可能にするが、初期状態では停止しておく。
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetNetUpdateFrequency(10);

	// ギミック全体の基準となるルートを作成する。
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// 土台をルートに取り付ける。
	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(SceneRoot);
	BaseMesh->SetCollisionProfileName(TEXT("BlockAll"));
	BaseMesh->SetSimulatePhysics(false);

	// 拳と攻撃範囲をまとめて動かすためのノード。
	PunchPivot = CreateDefaultSubobject<USceneComponent>(TEXT("PunchPivot"));
	PunchPivot->SetupAttachment(SceneRoot);

	// 拳のモデルは見た目だけを担当する。
	FistMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FistMesh"));
	FistMesh->SetupAttachment(PunchPivot);
	FistMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FistMesh->SetGenerateOverlapEvents(false);
	FistMesh->SetSimulatePhysics(false);

	// 後で攻撃判定に使用するボックス。現段階では衝突を無効にする。
	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(PunchPivot);
	HitBox->SetBoxExtent(FVector(50.0f, 50.0f, 50.0f));
	HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitBox->SetGenerateOverlapEvents(false);

	// ローカル X 軸の正方向をパンチの方向として表示する。
	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(
		TEXT("DirectionArrow"));
	DirectionArrow->SetupAttachment(SceneRoot);
	DirectionArrow->SetHiddenInGame(true);
}

void ASpringPunchGimmick::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Existing Blueprint defaults may still contain the old nonreplicated flag.
	if (GetNetMode() != NM_Client && HasAuthority() && !GetIsReplicated()) { SetReplicates(true); }
}

void ASpringPunchGimmick::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASpringPunchGimmick, Phase);
}

double ASpringPunchGimmick::GetPhaseTime() const
{
	const AGameStateBase* State = GetWorld()->GetGameState();
	return GetNetMode() == NM_Client && State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void ASpringPunchGimmick::OnRep_Phase()
{
	CurrentState = Phase.State;
	StateStartTime = Phase.StartedAt;
	RetractedRelativeLocation = Phase.RetractedLocation;
	SetActorTickEnabled(CurrentState == ESpringPunchState::Extending || CurrentState == ESpringPunchState::Retracting);
	UpdatePunchVisual();
}

void ASpringPunchGimmick::BeginPlay()
{
	Super::BeginPlay();

	// Blueprint で設定した拳の初期相対位置を保存する。
	if (GetNetMode() != NM_Client) { RetractedRelativeLocation = PunchPivot->GetRelativeLocation(); }

	// 初期状態では表示更新を停止する。
	SetActorTickEnabled(false);

	// サーバー側で最初の待機状態を開始する。
	if (GetNetMode() != NM_Client && HasAuthority())
	{
		EnterState(ESpringPunchState::Idle);
	}
	else { OnRep_Phase(); }
}

void ASpringPunchGimmick::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PunchAction.Close();
	HitActorsThisPunch.Reset();
	// 終了後に状態遷移が呼び出されないようにする。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StateTimerHandle);
	}

	SetActorTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}

void ASpringPunchGimmick::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 伸縮中の拳の表示位置を更新する。
	UpdatePunchVisual();
	ScanPunchHits();
}

void ASpringPunchGimmick::EnterState(ESpringPunchState NewState)
{
	// 状態の切り替えはサーバー側だけで行う。
	if (GetNetMode() == NM_Client || !HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 前の状態で設定したタイマーを解除する。
	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.ClearTimer(StateTimerHandle);
	PunchAction.Close();

	// 新しい状態と、その開始時刻を記録する。
	CurrentState = NewState;
	StateStartTime = World->GetTimeSeconds();
	if (NewState == ESpringPunchState::Extending) { ++Phase.PunchNumber; }

	// 新しいパンチの開始時に、ヒット記録と判定開始位置を初期化する。
	if (CurrentState == ESpringPunchState::Extending)
	{
		HitActorsThisPunch.Reset();

		// 拳を初期位置にそろえてから、攻撃ボックスの位置を記録する。
		PunchPivot->SetRelativeLocation(RetractedRelativeLocation);
		PreviousHitBoxWorldLocation = HitBox->GetComponentLocation();
		BeginPunchAttack();
	}

	float Duration = 0.0f;

	switch (CurrentState)
	{
	case ESpringPunchState::Idle:
		Duration = FMath::Max(0.0f, InitialDelay);
		break;

	case ESpringPunchState::Windup:
		Duration = FMath::Max(0.01f, WindupDuration);
		break;

	case ESpringPunchState::Extending:
		Duration = FMath::Max(0.01f, ExtendDuration);
		break;

	case ESpringPunchState::Retracting:
		Duration = FMath::Max(0.01f, RetractDuration);
		break;

	case ESpringPunchState::Cooldown:
		Duration = FMath::Max(0.01f, CooldownDuration);
		break;

	default:
		SetActorTickEnabled(false);
		return;
	}
	Phase.State = NewState;
	Phase.StartedAt = StateStartTime;
	Phase.Duration = Duration;
	Phase.Distance = FMath::Max(0.0f, PunchDistance);
	Phase.RetractedLocation = RetractedRelativeLocation;
	Phase.ExtensionCurve = NewState == ESpringPunchState::Extending ? ExtensionCurve.Get() : nullptr;
	Phase.RetractionCurve = NewState == ESpringPunchState::Retracting ? RetractionCurve.Get() : nullptr;
	UpdatePunchVisual();
	ForceNetUpdate();

	// 拳が伸縮している間だけ、表示更新用の Tick を有効にする。
	const bool bMoving =
		CurrentState == ESpringPunchState::Extending ||
		CurrentState == ESpringPunchState::Retracting;

	SetActorTickEnabled(bMoving);

	// 待機時間がゼロの場合は、次のゲームフレームで状態を進める。
	if (Duration <= 0.0f)
	{
		StateTimerHandle = TimerManager.SetTimerForNextTick(
			this,
			&ASpringPunchGimmick::AdvanceState);
	}
	else
	{
		// 指定時間後に一度だけ、次の状態へ進める。
		TimerManager.SetTimer(
			StateTimerHandle,
			this,
			&ASpringPunchGimmick::AdvanceState,
			Duration,
			false);
	}
}

void ASpringPunchGimmick::AdvanceState()
{
	// 状態遷移はサーバー側だけで実行する。
	if (GetNetMode() == NM_Client || !HasAuthority())
	{
		return;
	}

	switch (CurrentState)
	{
	case ESpringPunchState::Idle:
		EnterState(ESpringPunchState::Windup);
		break;

	case ESpringPunchState::Windup:
		EnterState(ESpringPunchState::Extending);
		break;

	case ESpringPunchState::Extending:
		// 描画フレームの間隔に関係なく、伸び切った位置を確定する。
		PunchPivot->SetRelativeLocation(
			RetractedRelativeLocation +
			FVector(Phase.Distance, 0.0f, 0.0f));

		// 戻る状態へ移る前に、最後の移動区間も判定する。
		ScanPunchHits();
		EnterState(ESpringPunchState::Retracting);
		break;

	case ESpringPunchState::Retracting:
		// Tick を止める前に、拳を正確な初期位置へ戻す。
		PunchPivot->SetRelativeLocation(RetractedRelativeLocation);

		EnterState(ESpringPunchState::Cooldown);
		break;

	case ESpringPunchState::Cooldown:
		EnterState(ESpringPunchState::Windup);
		break;

	default:
		break;
	}
}

void ASpringPunchGimmick::UpdatePunchVisual()
{
	UWorld* World = GetWorld();
	if (!World || !PunchPivot)
	{
		return;
	}

	// 現在の状態が始まってからの経過時間を求める。
	const double ElapsedTime = FMath::Max(
		0.0,
		GetPhaseTime() - Phase.StartedAt);

	// 初期位置からローカル X 軸方向へ伸ばした位置。
	const FVector ExtendedRelativeLocation =
		RetractedRelativeLocation +
		FVector(Phase.Distance, 0.0f, 0.0f);

	switch (CurrentState)
	{
	case ESpringPunchState::Extending:
	{
		// 伸びる動作の進行度を 0～1 に制限する。
		const float Alpha = FMath::Clamp(
			static_cast<float>(
				ElapsedTime / FMath::Max(0.01f, Phase.Duration)),
			0.0f,
			1.0f);

		PunchPivot->SetRelativeLocation(
			FMath::Lerp(
				RetractedRelativeLocation,
				ExtendedRelativeLocation,
				EvaluateExtensionProgress(Alpha)));
		break;
	}

	case ESpringPunchState::Retracting:
	{
		// 戻る動作の進行度を 0～1 に制限する。
		const float Alpha = FMath::Clamp(
			static_cast<float>(
				ElapsedTime / FMath::Max(0.01f, Phase.Duration)),
			0.0f,
			1.0f);

		PunchPivot->SetRelativeLocation(
			FMath::Lerp(
				ExtendedRelativeLocation,
				RetractedRelativeLocation,
				EvaluatePunchCurve(Phase.RetractionCurve, Alpha)));
		break;
	}

	default:
		// 待機・予告・クールダウン中は初期位置を維持する。
		PunchPivot->SetRelativeLocation(RetractedRelativeLocation);
		break;
	}
}

float ASpringPunchGimmick::EvaluateExtensionProgress(float Alpha) const
{
	return EvaluatePunchCurve(Phase.ExtensionCurve, Alpha);
}

void ASpringPunchGimmick::ScanPunchHits()
{
	// 非複製アクターでもクライアント側の攻撃判定は実行しない。
	if (GetNetMode() == NM_Client || !HasAuthority() ||
		CurrentState != ESpringPunchState::Extending || !PunchAction.IsActive())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !HitBox)
	{
		return;
	}

	const FVector Start = PreviousHitBoxWorldLocation;
	const FVector End = HitBox->GetComponentLocation();
	const FQuat Rotation = HitBox->GetComponentQuat();
	const FVector HalfExtent = HitBox->GetScaledBoxExtent();

	// 次回の掃引開始位置を更新する。
	PreviousHitBoxWorldLocation = End;
	if (HalfExtent.GetMin() <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 現在の車両が使用する PhysicsBody を検索対象にする。
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpringPunchScan), false, this);
	QueryParams.bFindInitialOverlaps = true;

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(
		Hits, Start, End, Rotation, ObjectParams,
		FCollisionShape::MakeBox(HalfExtent), QueryParams);

	bool bFoundNewTarget = false;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || Target == this ||
			!Target->FindComponentByClass<UVehicleHealthComponent>())
		{
			continue;
		}

		const TWeakObjectPtr<AActor> TargetKey(Target);
		if (HitActorsThisPunch.Contains(TargetKey))
		{
			continue;
		}

		const FHALCombatHitResult Result = ResolvePunchHit(Hit);
		if (Result.bHitAccepted)
		{
			HitActorsThisPunch.Add(TargetKey);
			bFoundNewTarget = true;
		}
		if (bDebugPunchHits)
		{
			UE_LOG(LogTemp, Log, TEXT("SpringPunch [%s] Attack=%s Target=%s Accepted=%d Reject=%d Damage=%.2f Impact=%d Tier=%d Drop=%d"),
				*GetNameSafe(this), *PunchAction.GetSnapshot().AttackId.ToString(), *GetNameSafe(Target),
				Result.bHitAccepted, int32(Result.RejectReason), Result.ActualHPRemoved,
				int32(Result.ImpactOutcome), int32(Result.ImpactSummary.EffectiveTier), Result.ImpactSummary.bBallDropped);
		}
	}

	if (bDebugPunchHits)
	{
		const FColor Color = bFoundNewTarget ? FColor::Green : FColor::Yellow;
		// 始点・終点のボックスと移動経路を表示する。
		DrawDebugBox(World, Start, HalfExtent, Rotation, Color, false, 0.15f);
		DrawDebugBox(World, End, HalfExtent, Rotation, Color, false, 0.15f);
		DrawDebugLine(World, Start, End, Color, false, 0.15f, 0, 2.0f);
	}
}

bool ASpringPunchGimmick::BeginPunchAttack()
{
	PunchAction.Close();
	FHALCollisionImpactRules Rules;
	if (GetNetMode() == NM_Client || !HasAuthority() || CurrentState != ESpringPunchState::Extending
		|| !FMath::IsFinite(BaseDamage) || BaseDamage < 0 || BaseDamage > 10000
		|| uint8(ImpactTier) > uint8(EHALImpactTier::Heavy)
		|| !GetDefault<UVehicleKnockbackSettings>()->GetCollisionImpactRules(Rules)
		|| Rules.Version != EHALImpactVersion::TieredV1)
	{
		if (bDebugPunchHits) { UE_LOG(LogTemp, Warning, TEXT("SpringPunch [%s]: attack not ready; check damage, tier and TieredV1 combat rules."), *GetName()); }
		return false;
	}
	FHALAttackSourceSnapshot Source;
	Source.SourceActor = this;
	Source.SourceTag = HALGameplayTags::Combat_Source_SpringPunch;
	Source.DamageMode = EHALDamageMode::SourceFixedDamage;
	Source.FixedDamage = BaseDamage;
	Source.Impact.Version = EHALImpactVersion::TieredV1;
	Source.Impact.Tier = ImpactTier;
	return PunchAction.Begin(*this, Source);
}

FHALCombatHitResult ASpringPunchGimmick::ResolvePunchHit(const FHitResult& Hit)
{
	if (GetNetMode() == NM_Client || !HasAuthority() || CurrentState != ESpringPunchState::Extending)
	{
		FHALCombatHitResult Result;
		Result.RejectReason = EHALHitRejectReason::ClosedAction;
		return Result;
	}
	FHALCombatHitRequest Request;
	Request.AttackId = PunchAction.GetSnapshot().AttackId;
	Request.PulseSequence = PunchAction.GetPulseSequence();
	Request.bApplyDamage = true;
	Request.bApplyImpact = true;
	Request.Impact.Mode = EHALImpactMode::ExplicitTier;
	Request.Impact.Tier = PunchAction.GetSnapshot().Impact.Tier;
	Request.Hit.SourceActor = this;
	Request.Hit.TargetActor = Hit.GetActor();
	Request.Hit.TargetPhysicsBody = Hit.GetActor()
		? Cast<UPrimitiveComponent>(Hit.GetActor()->GetRootComponent()) : nullptr;
	Request.Hit.ImpactPoint = Hit.ImpactPoint;
	// The punch travels along its parent's local +X, independent of the struck face.
	Request.Hit.ImpactNormal = -SceneRoot->GetForwardVector();
	const UVehicleHealthComponent* Health = Hit.GetActor()
		? Hit.GetActor()->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
	const float HPBefore = Health ? Health->GetCurrentHP() : 0.0f;
	const FHALCombatHitResult Result = FCombatResolver::ResolveCombatHit(PunchAction, Request);
#if !UE_BUILD_SHIPPING
	if (bDebugDamageOnScreen && Result.bHitAccepted && Result.ActualHPRemoved > 0 && GEngine
		&& GetNetMode() != NM_DedicatedServer)
	{
		const FString Tier = StaticEnum<EHALImpactTier>()->GetNameStringByValue(int64(Result.ImpactSummary.EffectiveTier));
		const FString Message = FString::Printf(TEXT("[SpringPunch] %s -> %s | HP %.1f -> %.1f (-%.1f) | Impact %s (%d), Applied=%s"),
			*GetActorNameOrLabel(), *Hit.GetActor()->GetActorNameOrLabel(), HPBefore, HPBefore - Result.ActualHPRemoved,
			Result.ActualHPRemoved, *Tier, int32(Result.ImpactSummary.EffectiveTier), Result.bImpactApplied ? TEXT("Yes") : TEXT("No"));
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 6.0f, FColor::Yellow, Message);
	}
#endif
	return Result;
}
