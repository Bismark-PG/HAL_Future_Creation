#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "HALIncomingBallWidget.generated.h"

class ABasicBallActor;
struct FHALBallComicFXConfig;

/** One screen edge arrow, in viewport fractions so the widget never depends on DPI scale. */
struct FHALIncomingBallArrow
{
	TWeakObjectPtr<const ABasicBallActor> Ball;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Direction = FVector2D(1, 0);
	FLinearColor Color = FLinearColor::White;
	float SecondsToArrival = 0;
	/** Seconds the entry condition has held; the arrow is drawn once this passes the confirm time. */
	float ConfirmedSeconds = 0;
	float ShownSeconds = 0;
	bool bShown = false;
	bool bMissileLock = false;
	float MissileSize = 0.f;
	float PulsePhase = 0.f;
};

namespace HALIncomingBall
{
	/** Horizontal closest approach of a launched ball to a car. True when it will pass within the miss distance
	 * inside the time window. Pure presentation heuristic: it never feeds back into any hit or damage decision. */
	HAL_FUTURE_CREATION_API bool IsIncoming(const FHALBallComicFXConfig& Config, const FVector& BallLocation, const FVector& BallVelocity,
		const FVector& VehicleLocation, float& OutSecondsToArrival);
	/** Clamps a direction from the viewport centre onto the inset frame; all values in viewport fractions. */
	HAL_FUTURE_CREATION_API FVector2D EdgePoint(const FVector2D& Direction, float InsetFraction);
}

/**
 * Local-only doodle warning: an arrow on the screen edge, in the launcher's colour, pointing at a launched ball
 * that is off screen and heading for this player's car. Reads replicated ball state and the local camera
 * projection, writes nothing but itself.
 */
UCLASS()
class HAL_FUTURE_CREATION_API UHALIncomingBallWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	const TArray<FHALIncomingBallArrow>& GetArrows() const { return Arrows; }
private:
	TArray<FHALIncomingBallArrow> Arrows;
};

/** One non-interactive overlay per local player; cleared on world teardown. */
UCLASS()
class UHALIncomingBallHUDSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	static void EnsureForWorld(UWorld* World);
	virtual void Deinitialize() override;
private:
	void CleanupWorld(UWorld* World, bool, bool);
	UPROPERTY(Transient) TObjectPtr<UHALIncomingBallWidget> Widget;
	FDelegateHandle Cleanup;
};
