#include "HALPlayerController.h"
#include "HALDebugSettings.h"
#include "HALExpandingHealingArea.h"
#include "TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

void AHALPlayerController::HALSpawnHealingArea()
{
#if !UE_BUILD_SHIPPING
	if (!IsLocalController()) { return; }
	if (HasAuthority()) { ServerSpawnDebugHealingArea_Implementation(); }
	else { ServerSpawnDebugHealingArea(); }
#endif
}

void AHALPlayerController::ServerSpawnDebugHealingArea_Implementation()
{
#if !UE_BUILD_SHIPPING
	const auto* Vehicle = Cast<ATestVehiclePawn>(GetPawn());
	const auto* Settings = GetDefault<UHALDebugSettings>();
	AHALExpandingHealingArea* Area = nullptr;
	FString Message;
	if (!HasAuthority() || GetNetMode() == NM_Client || !IsValid(Vehicle)
		|| Vehicle->GetController() != this || !Vehicle->IsConfigurationValid())
	{
		Message = TEXT("HALSpawnHealingArea: no valid possessed vehicle.");
	}
	else if (!FMath::IsFinite(Settings->HealingAreaForwardDistance)
		|| Settings->HealingAreaForwardDistance < 0 || Settings->HealingAreaForwardDistance > 5000)
	{
		Message = TEXT("HALSpawnHealingArea: invalid forward distance in Project Settings > HAL Debug > Healing Area.");
	}
	else
	{
		auto* Class = Settings->DefaultHealingAreaClass.LoadSynchronous();
		const FVector Forward = Vehicle->GetActorForwardVector().GetSafeNormal2D();
		if (!Class || !Class->IsChildOf(AHALExpandingHealingArea::StaticClass())
			|| Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
			|| !Class->GetDefaultObject<AHALExpandingHealingArea>()->GetAreaConfig().IsValid())
		{
			Message = TEXT("HALSpawnHealingArea: missing/invalid area class or defaults in HAL Debug; no fallback spawned.");
		}
		else if (Forward.IsNearlyZero())
		{
			Message = TEXT("HALSpawnHealingArea: vehicle has no horizontal heading.");
		}
		else
		{
			FActorSpawnParameters Params;
			Params.Owner = PlayerState;
			Params.Instigator = GetPawn();
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector Location = Vehicle->GetActorLocation() + Forward * Settings->HealingAreaForwardDistance;
			Area = GetWorld()->SpawnActor<AHALExpandingHealingArea>(Class, Location, FRotator::ZeroRotator, Params);
			if (IsValid(Area))
			{
				const auto& Config = Area->GetAreaConfig();
				Message = FString::Printf(TEXT("HALSpawnHealingArea: Spawned=%s Class=%s Location=%s Expansion=%.2fs Hold=%.2fs MaxRadius=%.1fcm"),
					*Area->GetName(), *Class->GetName(), *Location.ToCompactString(),
					Config.ExpansionSeconds, Config.DurationSeconds, Config.MaximumRadius);
			}
			else { Message = TEXT("HALSpawnHealingArea: spawn failed."); }
		}
	}
	UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
	ClientDebugHealingAreaResult(Message, IsValid(Area));
#endif
}

void AHALPlayerController::ClientDebugHealingAreaResult_Implementation(const FString& Message, bool bSuccess)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority()) { UE_LOG(LogTemp, Display, TEXT("%s"), *Message); }
	if (GEngine) { GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, bSuccess ? FColor::Green : FColor::Yellow, Message); }
#endif
}
