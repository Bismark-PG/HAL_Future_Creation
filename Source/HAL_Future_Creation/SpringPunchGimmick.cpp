#include "SpringPunchGimmick.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ASpringPunchGimmick::ASpringPunchGimmick()
{
	// Tick は使用可能にするが、初期状態では停止しておく。
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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

void ASpringPunchGimmick::BeginPlay()
{
	Super::BeginPlay();

	// Blueprint で設定した拳の初期相対位置を保存する。
	RetractedRelativeLocation = PunchPivot->GetRelativeLocation();

	// 初期状態では表示更新を停止する。
	SetActorTickEnabled(false);

	// サーバー側で最初の待機状態を開始する。
	if (HasAuthority())
	{
		EnterState(ESpringPunchState::Idle);
	}
}

void ASpringPunchGimmick::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
}

void ASpringPunchGimmick::EnterState(ESpringPunchState NewState)
{
	// 状態の切り替えはサーバー側だけで行う。
	if (!HasAuthority())
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

	// 新しい状態と、その開始時刻を記録する。
	CurrentState = NewState;
	StateStartTime = World->GetTimeSeconds();

	// 新しいパンチの開始時に、ヒット記録と判定開始位置を初期化する。
	if (CurrentState == ESpringPunchState::Extending)
	{
		HitActorsThisPunch.Reset();

		// 拳を初期位置にそろえてから、攻撃ボックスの位置を記録する。
		PunchPivot->SetRelativeLocation(RetractedRelativeLocation);
		PreviousHitBoxWorldLocation = HitBox->GetComponentLocation();
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
	if (!HasAuthority())
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
			FVector(FMath::Max(0.0f, PunchDistance), 0.0f, 0.0f));

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
		World->GetTimeSeconds() - StateStartTime);

	// 初期位置からローカル X 軸方向へ伸ばした位置。
	const FVector ExtendedRelativeLocation =
		RetractedRelativeLocation +
		FVector(FMath::Max(0.0f, PunchDistance), 0.0f, 0.0f);

	switch (CurrentState)
	{
	case ESpringPunchState::Extending:
	{
		// 伸びる動作の進行度を 0～1 に制限する。
		const float Alpha = FMath::Clamp(
			static_cast<float>(
				ElapsedTime / FMath::Max(0.01f, ExtendDuration)),
			0.0f,
			1.0f);

		PunchPivot->SetRelativeLocation(
			FMath::Lerp(
				RetractedRelativeLocation,
				ExtendedRelativeLocation,
				Alpha));
		break;
	}

	case ESpringPunchState::Retracting:
	{
		// 戻る動作の進行度を 0～1 に制限する。
		const float Alpha = FMath::Clamp(
			static_cast<float>(
				ElapsedTime / FMath::Max(0.01f, RetractDuration)),
			0.0f,
			1.0f);

		PunchPivot->SetRelativeLocation(
			FMath::Lerp(
				ExtendedRelativeLocation,
				RetractedRelativeLocation,
				Alpha));
		break;
	}

	default:
		// 待機・予告・クールダウン中は初期位置を維持する。
		PunchPivot->SetRelativeLocation(RetractedRelativeLocation);
		break;
	}
}
