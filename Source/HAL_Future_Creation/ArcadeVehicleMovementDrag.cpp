#include "ArcadeVehicleMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

void UArcadeVehicleMovementComponent::ResolveDragDriveFrame(
	FBodyInstance& Body, FVector& InOutForwardDirection, float& InOutForwardSpeed) const
{
	// The authority bridge grants this historical flow only while the Drag GE is active.
	// An absent flow must preserve the caller's original direction AND speed without querying the body.
	if (!CurrentMovement.GravityDrive.Source.IsValid() || CurrentMovement.GravityDrive.Strength <= 0
		|| InputCommand.Throttle <= KINDA_SMALL_NUMBER || InputCommand.Brake > KINDA_SMALL_NUMBER) { return; }
	auto Handle = Body.GetBodyInstanceAsyncPhysicsTickHandle();
	if (!Handle.IsValid()) { return; }
	InOutForwardDirection = CurrentMovement.GravityDrive.Bend(
		Handle->X(), InOutForwardDirection, InputCommand.Steering, CurrentMovementServerFrame);
	InOutForwardSpeed = FVector::DotProduct(Handle->V(), InOutForwardDirection);
}
