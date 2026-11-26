#include "HALHealingAreaGizmoComponent.h"

UHALHealingAreaGizmoComponent::UHALHealingAreaGizmoComponent()
{
	bIsEditorOnly = true;
	PrimaryComponentTick.bCanEverTick = false;
	SetCanEverAffectNavigation(false);
}
