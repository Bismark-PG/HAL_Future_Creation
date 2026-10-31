#include "HALShockedPresentationComponent.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"

UHALShockedPresentationComponent::UHALShockedPresentationComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UHALShockedPresentationComponent::Bind(UHALAbilitySystemComponent* InASC)
{
	if (!InASC || InASC->GetAvatarActor() != GetOwner()) { Unbind(); return; }
	if (ASC == InASC && TagHandle.IsValid()) { SetShocked(ASC->HasMatchingGameplayTag(HALGameplayTags::State_Visual_Shocked)); return; }
	Unbind(); ASC = InASC;
	TagHandle = ASC->RegisterGameplayTagEvent(HALGameplayTags::State_Visual_Shocked,EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this,&ThisClass::TagChanged);
	SetShocked(ASC->HasMatchingGameplayTag(HALGameplayTags::State_Visual_Shocked));
}
void UHALShockedPresentationComponent::Unbind()
{
	if (ASC) { ASC->RegisterGameplayTagEvent(HALGameplayTags::State_Visual_Shocked,EGameplayTagEventType::NewOrRemoved).Remove(TagHandle); }
	TagHandle.Reset(); ASC = nullptr; SetShocked(false);
}
void UHALShockedPresentationComponent::SetShocked(bool bActive)
{
	if (bShocked == bActive) { return; }
	bShocked = bActive; OnShockedChanged.Broadcast(bShocked);
}
void UHALShockedPresentationComponent::TagChanged(FGameplayTag Tag,int32 Count) { SetShocked(Count > 0); }
void UHALShockedPresentationComponent::EndPlay(const EEndPlayReason::Type Reason) { Unbind(); Super::EndPlay(Reason); }
