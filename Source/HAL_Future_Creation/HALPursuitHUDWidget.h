#pragma once
#include "Blueprint/UserWidget.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "HALPolicePursuitTypes.h"
#include "HALPursuitHUDWidget.generated.h"
/** Designer root replaces NativePaint; Blueprint receives read-only projected target data. */
UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API UHALPursuitHUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure,Category="Pursuit|UI") TArray<FHALPursuitView> GetPursuitViews() const;
	UFUNCTION(BlueprintImplementableEvent,Category="Pursuit|UI") void ReceivePursuitViews(const TArray<FHALPursuitView>& Views);
	static TArray<FHALPursuitView> GatherViews(UWorld* World);
	static void PaintStars(const FGeometry& G,FSlateWindowElementList& Elements,int32 Layer,FVector2D Center,float Fraction,const FHALPursuitFX& Style);
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& C,FSlateWindowElementList& E,int32 L,const FWidgetStyle& S,bool B) const override;
private:
	void Refresh();
	bool bNative=true;
	FTimerHandle Timer;
};
/** One non-interactive pursuit overlay per local player; explicitly cleared on world teardown. */
UCLASS()
class UHALPursuitHUDSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	static void EnsureForWorld(UWorld* World,TSubclassOf<UHALPursuitHUDWidget> Class);
	virtual void Deinitialize() override;
private:
	void CleanupWorld(UWorld* World,bool,bool);
	UPROPERTY(Transient) TObjectPtr<UHALPursuitHUDWidget> Widget;
	FDelegateHandle Cleanup;
};
