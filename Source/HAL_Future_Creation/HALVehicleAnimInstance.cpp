#include "HALVehicleAnimInstance.h"
#include "HALVehiclePresentationComponent.h"
#include "GameFramework/Actor.h"

void UHALVehicleAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Source = GetOwningActor() ? GetOwningActor()->FindComponentByClass<UHALVehiclePresentationComponent>() : nullptr;
	Presentation = FHALVehiclePresentationFrame();
}

void UHALVehicleAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	check(IsInGameThread());
	if (!Source.IsValid())
	{
		Source = GetOwningActor() ? GetOwningActor()->FindComponentByClass<UHALVehiclePresentationComponent>() : nullptr;
	}
	Presentation = Source.IsValid() && Source->GetPresentationMesh() == GetSkelMeshComponent()
		? Source->GetPresentationFrame() : FHALVehiclePresentationFrame();
}
