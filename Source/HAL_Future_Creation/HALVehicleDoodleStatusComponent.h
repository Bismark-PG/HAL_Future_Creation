#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "HALVehicleDoodleStatusComponent.generated.h"

class UHALAbilitySystemComponent;
class UMaterialBillboardComponent;
class UMaterialInterface;
class UVehicleHealthComponent;

/** Comic status marks floating over a car: circling stars while stunned, sweat drops while nearly out of HP.
 * Presentation only. Reads replicated GAS state; never writes HP, tags, control permission or physics. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALVehicleDoodleStatusConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Doodle Status") bool bEnabled = true;
	/** Stars circle over any stunned car, whatever stunned it. None keeps them off. */
	UPROPERTY(EditAnywhere, Category="Stun Stars") TSoftObjectPtr<UMaterialInterface> StunMaterial;
	/** Sprite width as a share of the car body's width; the stars orbit inside it. Cars differ in size, the mark must not. */
	UPROPERTY(EditAnywhere, Category="Stun Stars", meta=(ClampMin="0.1", ClampMax="4")) float StunSizeWidthFraction = 1.25f;
	/** Height of the sprite centre above the top of the car body, as a share of the body's width. */
	UPROPERTY(EditAnywhere, Category="Stun Stars", meta=(ClampMin="0", ClampMax="2")) float StunLiftWidthFraction = .3f;
	/** Off by default (team review, 2026-11-18). The stars are not affected. */
	UPROPERTY(EditAnywhere, Category="Low Health") bool bLowHealth = false;
	/** Sweat drops beside a car whose HP share is at or below the threshold and still above zero. None keeps them off. */
	UPROPERTY(EditAnywhere, Category="Low Health") TSoftObjectPtr<UMaterialInterface> LowHealthMaterial;
	UPROPERTY(EditAnywhere, Category="Low Health", meta=(ClampMin="0.01", ClampMax="0.95")) float LowHealthFraction = .3f;
	UPROPERTY(EditAnywhere, Category="Low Health", meta=(ClampMin="0.1", ClampMax="4")) float LowHealthSizeWidthFraction = 1.f;
	UPROPERTY(EditAnywhere, Category="Low Health", meta=(ClampMin="0", ClampMax="2")) float LowHealthLiftWidthFraction = .22f;
	bool Validate(FString& Error) const;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Vehicle Doodle Status FX"))
class HAL_FUTURE_CREATION_API UHALVehicleDoodleStatusSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category="Doodle Status") FHALVehicleDoodleStatusConfig Status;
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
#if WITH_EDITOR
	bool CollectCookPackages(TArray<FName>& Packages, FString& Error) const;
#endif
};

namespace HALVehicleDoodleStatus
{
	/** Stars need a live car: an eliminated car shows nothing. */
	HAL_FUTURE_CREATION_API bool ShowStunStars(bool bStunned, float CurrentHP);
	/** Enabled, at or below the threshold, strictly above zero. Unknown or invalid HP never shows the mark. */
	HAL_FUTURE_CREATION_API bool ShowLowHealth(const FHALVehicleDoodleStatusConfig& Config, float CurrentHP, float MaxHP);
}

UCLASS(ClassGroup=(Vehicle))
class HAL_FUTURE_CREATION_API UHALVehicleDoodleStatusComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALVehicleDoodleStatusComponent();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	/** Event driven: a Stun tag count change and the health component's change notification. Health may be null. */
	void Bind(UHALAbilitySystemComponent* InAbilitySystem, UVehicleHealthComponent* InHealth);
	void Unbind();
	bool IsShowingStunStars() const { return bStars; }
	bool IsShowingLowHealth() const { return bLowHealth; }
private:
	void StunChanged(FGameplayTag Tag, int32 Count);
	UFUNCTION() void HealthChanged(float CurrentHP, float MaxHP);
	void Refresh();
	UMaterialBillboardComponent* EnsureSprite(TObjectPtr<UMaterialBillboardComponent>& Sprite, UMaterialInterface* Material, float SizeCm);
	/** Unrotated body width in cm; zero when the owner has no primitive root. */
	float GetBodyWidthCm() const;
	void PlaceSprites();
	TWeakObjectPtr<UHALAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UVehicleHealthComponent> Health;
	FDelegateHandle StunHandle;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> StunMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> LowHealthMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialBillboardComponent> StunSprite;
	UPROPERTY(Transient) TObjectPtr<UMaterialBillboardComponent> LowHealthSprite;
	bool bReady = false;
	bool bStars = false;
	bool bLowHealth = false;
};
