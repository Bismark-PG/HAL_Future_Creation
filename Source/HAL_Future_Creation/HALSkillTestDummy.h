#pragma once

#include "CoreMinimal.h"
#include "TestVehiclePawn.h"
#include "Engine/DataAsset.h"
#include "Blueprint/UserWidget.h"
#include "HALSkillTestDummy.generated.h"

class UHALHealthAttributeSet;
class UHALCombatAttributeSet;
class UHALImpactAttributeSet;
class UHALMovementAttributeSet;
class UWidgetComponent;
class UHALSkillTestDummyWidget;

/** Test-only authoring wrapper. Vehicle rules remain in an ordinary, separately owned Definition. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALSkillTestDummyConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Vehicle") TObjectPtr<UVehicleDefinition> VehicleDefinition;
	UPROPERTY(EditAnywhere, Category="Display") FText Label = NSLOCTEXT("HALSkillTest", "Target", "TARGET");
	UPROPERTY(EditAnywhere, Category="Display") bool bShowHealth = true;
	/** None uses native label/HP. A Designer layout replaces it. */
	UPROPERTY(EditAnywhere, Category="Display") TSubclassOf<UHALSkillTestDummyWidget> WidgetClass;
	bool Validate(FString& Error) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** Formal vehicle receivers without a player, controller, room seat, or active ability grants. */
UCLASS()
class HAL_FUTURE_CREATION_API AHALSkillTestDummy : public ATestVehiclePawn
{
	GENERATED_BODY()
public:
	AHALSkillTestDummy();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void PostNetInit() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual bool CanReceiveSkillEffects() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	const UHALSkillTestDummyConfig* GetTestConfig() const { return TestConfig; }
	/** Deferred spawn only, before FinishSpawning: the config is read once while the actor is constructed. */
	void SetTestConfigForSpawn(UHALSkillTestDummyConfig* InConfig) { TestConfig = InConfig; }
protected:
	virtual void InitializeAbilitySystem() override;
	UPROPERTY(EditAnywhere, Replicated, Category="Skill Test") TObjectPtr<UHALSkillTestDummyConfig> TestConfig;
private:
	bool ConfigureDummy();
	UPROPERTY(VisibleAnywhere, Category="Skill Test") TObjectPtr<UHALAbilitySystemComponent> DummyAbilitySystem;
	UPROPERTY() TObjectPtr<UHALHealthAttributeSet> HealthAttributes;
	UPROPERTY() TObjectPtr<UHALCombatAttributeSet> CombatAttributes;
	UPROPERTY() TObjectPtr<UHALImpactAttributeSet> ImpactAttributes;
	UPROPERTY() TObjectPtr<UHALMovementAttributeSet> MovementAttributes;
	UPROPERTY(VisibleAnywhere, Category="Skill Test") TObjectPtr<UWidgetComponent> TestHealth;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALSkillTestDummyView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FText Label;
	UPROPERTY(BlueprintReadOnly) float HP = 0;
	UPROPERTY(BlueprintReadOnly) float MaxHP = 1;
	UPROPERTY(BlueprintReadOnly) bool bReady = false;
	UPROPERTY(BlueprintReadOnly) bool bStunned = false;
};

UCLASS(Blueprintable)
class HAL_FUTURE_CREATION_API UHALSkillTestDummyWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetDummy(AHALSkillTestDummy* InDummy) { Dummy = InDummy; }
	UFUNCTION(BlueprintPure, Category="Skill Test") FHALSkillTestDummyView GetDummyView() const;
	UFUNCTION(BlueprintImplementableEvent, Category="Skill Test") void ReceiveDummyView(const FHALSkillTestDummyView& View);
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool ParentEnabled) const override;
private:
	void Refresh();
	UPROPERTY(Transient) TWeakObjectPtr<AHALSkillTestDummy> Dummy;
	FTimerHandle RefreshTimer;
	bool bNative = true;
};
