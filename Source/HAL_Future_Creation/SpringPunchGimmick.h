// プロジェクト設定の Description ページで著作権表記を設定してください。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HALCombatTypes.h"
#include "SpringPunchGimmick.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UArrowComponent;
class UCurveFloat;
// パンチギミックの動作段階。
UENUM(BlueprintType)
enum class ESpringPunchState : uint8
{
	Idle,        // 初回の動作開始を待つ。
	Windup,      // 攻撃前の予告。
	Extending,   // 拳を前方へ伸ばす。
	Retracting,  // 拳を元の位置へ戻す。
	Cooldown     // 次の攻撃まで待つ。
};

/** One coherent server phase, sufficient for late join and relevancy re-entry. */
USTRUCT()
struct FSpringPunchPhase
{
	GENERATED_BODY()
	UPROPERTY() ESpringPunchState State = ESpringPunchState::Idle;
	UPROPERTY() double StartedAt = 0;
	UPROPERTY() float Duration = 1;
	UPROPERTY() float Distance = 0;
	UPROPERTY() FVector RetractedLocation = FVector::ZeroVector;
	UPROPERTY() uint32 PunchNumber = 0;
	/** Asset reference is captured per phase so authority and observers evaluate the same curve. */
	UPROPERTY() TObjectPtr<UCurveFloat> ExtensionCurve = nullptr;
	UPROPERTY() TObjectPtr<UCurveFloat> RetractionCurve = nullptr;
};

UCLASS()
class HAL_FUTURE_CREATION_API ASpringPunchGimmick : public AActor
{
	GENERATED_BODY()
	
public:	
	// このアクターのプロパティに初期値を設定する。
	ASpringPunchGimmick();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;

protected:
	/** Test baseline, damage per accepted target per punch. Zero still allows impact. */
	UPROPERTY(EditDefaultsOnly, Category = "Spring Punch|Combat", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	float BaseDamage = 25.0f;

	/** Rule response uses the shared tier table, resistance and immunity. */
	UPROPERTY(EditDefaultsOnly, Category = "Spring Punch|Combat")
	EHALImpactTier ImpactTier = EHALImpactTier::Medium;
	/** Optional horizontal response. Standard keeps the existing saved punch behavior. */
	UPROPERTY(EditDefaultsOnly, Category = "Spring Punch|Combat")
	EHALImpactPushPreset ImpactPushPreset = EHALImpactPushPreset::Standard;

	// ゲーム開始時、またはスポーン後にプレイを開始するときに呼び出される。
	virtual void BeginPlay() override;
	// 終了時にタイマーと表示更新を停止する。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 現在の動作段階。エディタからは変更できない。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spring Punch|State")
	ESpringPunchState CurrentState = ESpringPunchState::Idle;

	// ゲーム開始から最初の予告までの待機時間。
	UPROPERTY(EditDefaultsOnly, Category = "Spring Punch|Timing",
		meta = (ClampMin = "0.0", Units = "s"))
	float InitialDelay = 1.0f;

	// 拳を伸ばす前の予告時間。
	UPROPERTY(EditDefaultsOnly, Category = "Spring Punch|Timing",
		meta = (ClampMin = "0.01", Units = "s"))
	float WindupDuration = 0.8f;

	// 拳を最大距離まで伸ばす時間。
	UPROPERTY(EditDefaultsOnly, Category = "Spring Punch|Timing",
		meta = (ClampMin = "0.01", Units = "s"))
	float ExtendDuration = 0.25f;

	// 拳を元の位置へ戻す時間。
	UPROPERTY(EditDefaultsOnly, Category = "Spring Punch|Timing",
		meta = (ClampMin = "0.01", Units = "s"))
	float RetractDuration = 0.7f;

	// 拳が戻ってから次の予告までの待機時間。
	UPROPERTY(EditDefaultsOnly, Category = "Spring Punch|Timing",
		meta = (ClampMin = "0.01", Units = "s"))
	float CooldownDuration = 2.0f;

	// 初期位置からローカル X 軸の正方向へ伸ばす距離。
	UPROPERTY(EditDefaultsOnly, Category = "Spring Punch|Movement",
		meta = (ClampMin = "0.0", Units = "cm"))
	float PunchDistance = 400.0f;

	/** Normalized time (0..1) to extension progress (0..1). Slope controls speed; null keeps linear motion.
	 * Use an increasing curve from (0,0) to (1,1). Outputs are clamped and endpoints are exact. */
	UPROPERTY(EditDefaultsOnly, Category = "Spring Punch|Movement")
	TObjectPtr<UCurveFloat> ExtensionCurve;

	/** Normalized time to return progress: 0 = fully extended, 1 = back at the origin.
	 * Null/empty keeps linear return. Outputs are clamped and endpoints are exact. */
	UPROPERTY(EditDefaultsOnly, Category = "Spring Punch|Movement")
	TObjectPtr<UCurveFloat> RetractionCurve;

	// 攻撃範囲とヒットした対象をデバッグ表示する。
	UPROPERTY(EditDefaultsOnly, Category = "Spring Punch|Debug")
	bool bDebugPunchHits = false;

	/** Show accepted HP damage, target and resolved impact for 6 seconds on the local authority screen. Development only. */
	UPROPERTY(EditDefaultsOnly, Category = "Spring Punch|Debug")
	bool bDebugDamageOnScreen = false;

	// ギミック全体の位置と向きの基準。
	UPROPERTY(VisibleAnywhere, Category = "Spring Punch|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// 動かない土台のモデル。
	UPROPERTY(VisibleAnywhere, Category = "Spring Punch|Components")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	// このノードを動かすと、拳と攻撃範囲が一緒に移動する。
	UPROPERTY(VisibleAnywhere, Category = "Spring Punch|Components")
	TObjectPtr<USceneComponent> PunchPivot;

	// 拳の見た目を表すモデル。
	UPROPERTY(VisibleAnywhere, Category = "Spring Punch|Components")
	TObjectPtr<UStaticMeshComponent> FistMesh;

	// 拳の攻撃範囲を表すボックス。
	UPROPERTY(VisibleAnywhere, Category = "Spring Punch|Components")
	TObjectPtr<UBoxComponent> HitBox;

	// エディタ上でパンチの方向を示す。
	UPROPERTY(VisibleAnywhere, Category = "Spring Punch|Components")
	TObjectPtr<UArrowComponent> DirectionArrow;

	

public:	
	// Tick が有効な間、毎フレーム呼び出される。
	virtual void Tick(float DeltaTime) override;

private:
	friend class FHALSpringPunchLiveTest;
	friend class FHALSpringPunchCombatTest;
	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	FSpringPunchPhase Phase;
	UFUNCTION() void OnRep_Phase();
	double GetPhaseTime() const;
	float EvaluateExtensionProgress(float Alpha) const;
	FHALCombatAction PunchAction;
	bool BeginPunchAttack();
	FHALCombatHitResult ResolvePunchHit(const FHitResult& Hit);
	// 現在のパンチで処理済みのアクター。同じ対象への多重ヒットを防ぐ。
	TSet<TWeakObjectPtr<AActor>> HitActorsThisPunch;

	// 前回の攻撃判定位置。ワールド座標で保存する。
	FVector PreviousHitBoxWorldLocation = FVector::ZeroVector;

	// サーバー側で次の動作段階へ切り替える。
	void EnterState(ESpringPunchState NewState);

	// 現在の動作段階を終了し、次の段階へ進む。
	void AdvanceState();

	// 現在の動作段階と経過時間から拳の表示位置を更新する。
	void UpdatePunchVisual();

	// 前回位置から現在位置まで攻撃ボックスを掃引し、対象を検出する。
	void ScanPunchHits();

	// 拳を戻す位置。BeginPlay で初期の相対位置を保存する。
	FVector RetractedRelativeLocation = FVector::ZeroVector;

	// 現在の動作段階が始まったゲーム内時刻。単位は秒。
	double StateStartTime = 0.0;

	// 状態切り替え用タイマーの管理ハンドル。
	FTimerHandle StateTimerHandle;

};
