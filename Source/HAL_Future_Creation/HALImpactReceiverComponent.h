#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HALCombatTypes.h"
#include "HALImpactReceiverComponent.generated.h"

class UPrimitiveComponent;

/** Authority contact adapter and tier-result consumer. Does not own damage or simulate movement. */
UCLASS(ClassGroup = (Combat))
class HAL_FUTURE_CREATION_API UHALImpactReceiverComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALImpactReceiverComponent();
	bool ApplyConfiguration(const FHALVehicleImpactConfig& InConfig);
	bool CheckConfiguredRules() const;
	const FHALVehicleImpactConfig& GetConfiguration() const { return Config; }
	UFUNCTION(BlueprintPure, Category = "Combat|Impact")
	FHALImpactSummary GetLastImpact() const { return LastImpact; }
	void Resolve(const FHALCombatHitRequest& Request, FHALCombatHitResult& Result);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Engine contact adapter; also usable by native contact-producing components. Authority only. */
	UFUNCTION()
	void HandleContact(UPrimitiveComponent* Body, AActor* Other, UPrimitiveComponent* OtherBody,
		FVector NormalImpulse, const FHitResult& Hit);
private:
	struct FContact { double LastSeen = 0; bool bResolved = false; };
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FContact> Contacts;
	void ExpireContacts();
	void SuppressContact(UPrimitiveComponent* OtherBody);
	FHALVehicleImpactConfig Config;
	FTimerHandle ContactTimer;
	uint32 NextEvent = 0;
	UPROPERTY(Replicated)
	FHALImpactSummary LastImpact;
};
