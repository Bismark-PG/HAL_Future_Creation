#pragma once

#include "HALBlackHole.h"
#include "HALSpecialBall.h"
#include "HALBlackHoleBall.generated.h"

class UNiagaraSystem;

/** The same arena field and its three Niagara stages, scaled for a single projectile. */
UCLASS(BlueprintType)
class HAL_FUTURE_CREATION_API UHALBlackHoleBallDefinition : public UHALSpecialBallDefinition
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Black Hole") TSubclassOf<AHALBlackHole> FieldClass;
	/** Fraction of the authored field's planar radius and soft center. */
	UPROPERTY(EditAnywhere, Category="Black Hole", meta=(ClampMin=".05", ClampMax="1")) float RadiusScale = .5f;
	/** Scales inward, swirl and pulse strength; also eases the entry braking, grip loss and throttle bending. */
	UPROPERTY(EditAnywhere, Category="Black Hole", meta=(ClampMin=".05", ClampMax="1")) float StrengthScale = .45f;
	/** Fraction of both expansion and hold time. Short holds reserve room for one pulse. */
	UPROPERTY(EditAnywhere, Category="Black Hole", meta=(ClampMin=".05", ClampMax="1")) float DurationScale = .4f;
	/** Arena collapse system used when the free ball runs out of time. Receives User.Radius and User.SwirlSign. */
	UPROPERTY(EditAnywhere, Category="Effects") TObjectPtr<UNiagaraSystem> ExpirationVortex;
	bool ResolveField(FHALBlackHoleConfig& Out) const;
	virtual bool Validate(FString& Error) const override;
};

UCLASS()
class HAL_FUTURE_CREATION_API AHALBlackHoleBall : public AHALContactSpecialBall
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
protected:
	virtual UHALSpecialBallDefinition* DefaultType() const override;
	virtual bool ApplyContact(AActor* Other, const FVector& Origin, const FVector& Normal, const FVector& Travel) override;
private:
	FVector GroundPoint(AActor* Struck, const FVector& Contact, const FVector& Normal) const;
};
