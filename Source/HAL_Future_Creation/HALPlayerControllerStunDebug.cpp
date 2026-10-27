#include "HALPlayerController.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"
#include "HALMatchGameState.h"
#include "TestVehiclePawn.h"
#include "VehicleHealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarHALStunDebug(TEXT("hal.StunDebug"), 1,
	TEXT("Allow development host-console HALStun PlayerId DurationSeconds. 0=disabled, 1=enabled. No client RPC."));
#endif

void AHALPlayerController::HALStun(int32 PlayerId, float DurationSeconds)
{
#if !UE_BUILD_SHIPPING
	FString Message;
	bool bSuccess = false;
	const auto* Match = GetWorld() ? GetWorld()->GetGameState<AHALMatchGameState>() : nullptr;
	if (!HasAuthority() || !IsLocalController())
	{
		Message = TEXT("HALStun: rejected; use the host console. Usage: HALStun PlayerId DurationSeconds.");
	}
	else if (CVarHALStunDebug.GetValueOnGameThread() == 0)
	{
		Message = TEXT("HALStun: rejected; hal.StunDebug is disabled on the server.");
	}
	else if (PlayerId < 0 || !FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.f)
	{
		Message = TEXT("HALStun: rejected; use HALStun PlayerId DurationSeconds with a nonnegative ID and positive finite game seconds.");
	}
	else if (!Match || Match->GetMatchPhase() != EHALMatchPhase::Playing)
	{
		Message = TEXT("HALStun: rejected; start the match (Playing) first.");
	}
	else
	{
		ATestVehiclePawn* Target = nullptr;
		bool bAmbiguous = false;
		for (TActorIterator<ATestVehiclePawn> It(GetWorld()); It; ++It)
		{
			const auto* State = It->GetPlayerState();
			if (!State || State->GetPlayerId() != PlayerId) { continue; }
			if (Target) { bAmbiguous = true; break; }
			Target = *It;
		}
		auto* ASC = Target ? Cast<UHALAbilitySystemComponent>(Target->GetAbilitySystemComponent()) : nullptr;
		const auto* Health = Target ? Target->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
		if (bAmbiguous || !IsValid(Target) || !Target->IsConfigurationValid() || !Target->GetController()
			|| Target->GetController()->GetPawn() != Target || !ASC || ASC->GetAvatarActor() != Target
			|| ASC->GetOwnerActor() != Target->GetPlayerState() || !ASC->IsHealthInitialized()
			|| !Health || !Health->IsHealthReady() || Health->GetCurrentHP() <= 0.f)
		{
			Message = FString::Printf(TEXT("HALStun: rejected; no unique ready living player vehicle for PlayerId=%d. Use HALGASStatus to find IDs."), PlayerId);
		}
		else
		{
			bSuccess = ASC->ApplyStun(DurationSeconds, this);
			if (bSuccess) { ASC->GetOwner()->ForceNetUpdate(); }
			Message = FString::Printf(TEXT("HALStun: Accepted=%d PlayerId=%d Vehicle=%s Requested=%.3fs Stunned=%d Remaining=%.3fs GameTime=%.3f"),
				bSuccess, PlayerId, *GetNameSafe(Target), DurationSeconds, ASC->IsStunned(),
				ASC->GetEffectTimeRemaining(HALGameplayTags::State_Stun), GetWorld()->GetTimeSeconds());
		}
	}
	UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
	if (GEngine && IsLocalController())
	{ GEngine->AddOnScreenDebugMessage(-1, 6.f, bSuccess ? FColor::Green : FColor::Yellow, Message); }
#endif
}
