#pragma once
#include "Blueprint/UserWidget.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "HALSmokeScreenView.generated.h"

class AHALSmokeScreen;
class UMaterialInstanceDynamic;

/** Below normal HUD, above the complete scene (including damage post-processing and translucent FX).
 * World-projected, wall-clipped cloud silhouettes plus a local-only animated screen veil.
 * This layer never takes focus, changes the camera or modifies gameplay. */
UCLASS(NotBlueprintable)
class HAL_FUTURE_CREATION_API UHALSmokeScreenView : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& G,float Delta) override;
	virtual int32 NativePaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& C,FSlateWindowElementList& E,int32 L,const FWidgetStyle& S,bool B) const override;
	float GetObscuration() const { return Obscuration; }
	/** Custom world-space target widgets can use the same policy as the native labels. */
	UFUNCTION(BlueprintPure,Category="Smoke|Presentation") static bool ShouldHideWorldTarget(const AActor* Target);
private:
	UPROPERTY(Transient) TMap<TObjectPtr<AHALSmokeScreen>,TObjectPtr<UMaterialInstanceDynamic>> CloudMaterials;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ScreenMaterial;
	float Obscuration=0, FadeIn=.2f, FadeOut=.5f, FullStrength=.8f;
	TWeakObjectPtr<APawn> LastPawn;
	FVector PreviousPosition=FVector::ZeroVector;
	bool bHasPosition=false;
};
UCLASS()
class HAL_FUTURE_CREATION_API UHALSmokeViewSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	static void EnsureForWorld(UWorld* World);
	virtual void Deinitialize() override;
private:
	void CleanupWorld(UWorld* World,bool,bool);
	UPROPERTY(Transient) TObjectPtr<UHALSmokeScreenView> View;
	FDelegateHandle Cleanup;
};
