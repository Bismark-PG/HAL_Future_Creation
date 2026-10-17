#include "HALPlayerController.h"
#include "HALAbilitySystemComponent.h"
#include "HALDebugSettings.h"
#include "HALGameplayTags.h"
#include "HALMatchGameState.h"
#include "HALMovementAttributeSet.h"
#include "HALMovementBoostEffect.h"
#include "TestVehiclePawn.h"
#include "VehicleHealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarHALMovementBoostDebug(TEXT("hal.MovementBoostDebug"), 1,
	TEXT("Authority development boost requests: 0=disabled, 1=enabled. Clear remains available in Playing."));
#endif

void AHALPlayerController::HALMovementBoost()
{
#if !UE_BUILD_SHIPPING
	if (!IsLocalController()) { return; }
	if (HasAuthority()) { ServerRequestMovementBoost_Implementation(false); }
	else { ServerRequestMovementBoost(false); }
#endif
}

void AHALPlayerController::HALClearMovementBoost()
{
#if !UE_BUILD_SHIPPING
	if (!IsLocalController()) { return; }
	if (HasAuthority()) { ServerRequestMovementBoost_Implementation(true); }
	else { ServerRequestMovementBoost(true); }
#endif
}

void AHALPlayerController::ServerRequestMovementBoost_Implementation(bool bRemove)
{
#if !UE_BUILD_SHIPPING
	// The owning RPC carries intent only. Target, definition, duration and scales
	// all come from current authority state; there is no arbitrary effect/target RPC.
	auto* Vehicle = Cast<ATestVehiclePawn>(GetPawn());
	auto* ASC = Vehicle ? Cast<UHALAbilitySystemComponent>(Vehicle->GetAbilitySystemComponent()) : nullptr;
	const auto* Match = GetWorld()->GetGameState<AHALMatchGameState>();
	const auto* Health = Vehicle ? Vehicle->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
	FString Message;
	bool bSuccess = false;
	if (!HasAuthority() || !IsValid(PlayerState) || !IsValid(Vehicle) || Vehicle->GetController() != this || !Vehicle->IsConfigurationValid()
		|| !ASC || ASC->GetAvatarActor() != Vehicle || ASC->GetOwnerActor() != PlayerState
		|| !ASC->IsHealthInitialized() || !Health || !Health->IsHealthReady() || !ASC->GetSet<UHALMovementAttributeSet>())
	{
		Message = TEXT("HALMovementBoost: rejected; no ready owned vehicle/ASC.");
	}
	else if (!Match || Match->GetMatchPhase() != EHALMatchPhase::Playing)
	{
		Message = TEXT("HALMovementBoost: rejected; start the match (Playing) first.");
	}
	else if (!bRemove && CVarHALMovementBoostDebug.GetValueOnGameThread() == 0)
	{
		Message = TEXT("HALMovementBoost: rejected; hal.MovementBoostDebug is disabled on the server.");
	}
	else if (bRemove)
	{
		const int32 Removed = ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(
			FGameplayTagContainer(HALGameplayTags::Effect_Movement_Boost)));
		bSuccess = true;
		Message = FString::Printf(TEXT("HALMovementBoost: Cleared PlayerId=%d Removed=%d GameTime=%.3f"),
			PlayerState->GetPlayerId(), Removed, GetWorld()->GetTimeSeconds());
	}
	else
	{
		UClass* EffectClass = GetDefault<UHALDebugSettings>()->MovementBoostEffectClass.LoadSynchronous();
		const auto* Effect = EffectClass && !EffectClass->HasAnyClassFlags(CLASS_Abstract)
			? Cast<UHALMovementBoostEffect>(EffectClass->GetDefaultObject()) : nullptr;
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		const auto Spec = Effect ? ASC->MakeOutgoingSpec(EffectClass, 1, Context) : FGameplayEffectSpecHandle();
		if (!Spec.IsValid() || Effect->DurationPolicy != EGameplayEffectDurationType::HasDuration
			|| !FMath::IsFinite(Spec.Data->GetDuration()) || Spec.Data->GetDuration() <= 0
			|| Effect->GetStackingType() != EGameplayEffectStackingType::AggregateByTarget || Effect->GetStackLimitCount() != 1
			|| Effect->StackDurationRefreshPolicy != EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication
			|| Effect->bDenyOverflowApplication
			|| !Effect->GetAssetTags().HasTagExact(HALGameplayTags::Effect_Scope_Avatar)
			|| !Effect->GetAssetTags().HasTagExact(HALGameplayTags::Effect_Movement_Boost)
			|| !Effect->GetGrantedTags().HasTagExact(HALGameplayTags::State_Movement_Boost))
		{
			Message = TEXT("HALMovementBoost: rejected; invalid GE in Project Settings > HAL Debug > Movement. No fallback applied.");
		}
		else
		{
			const auto Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
			const auto* Active = ASC->GetActiveGameplayEffect(Handle);
			bSuccess = Active != nullptr;
			Message = FString::Printf(TEXT("HALMovementBoost: Accepted=%d PlayerId=%d Effect=%s Remaining=%.3f Stacks=%d GameTime=%.3f (physics uses next boundary)"),
				bSuccess, PlayerState->GetPlayerId(), *GetNameSafe(EffectClass),
				ASC->GetEffectTimeRemaining(HALGameplayTags::State_Movement_Boost), Active ? Active->Spec.GetStackCount() : 0, GetWorld()->GetTimeSeconds());
		}
	}
	if (bSuccess) { ASC->GetOwner()->ForceNetUpdate(); }
	UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
	ClientMovementBoostResult(Message, bSuccess);
#endif
}

void AHALPlayerController::ClientMovementBoostResult_Implementation(const FString& Message, bool bSuccess)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority()) { UE_LOG(LogTemp, Display, TEXT("%s"), *Message); }
	if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 5, bSuccess ? FColor::Green : FColor::Yellow, Message); }
#endif
}
