#include "HALPlayerController.h"
#include "HALDebugSettings.h"
#include "HALExpandingSlipperyArea.h"
#include "TestVehiclePawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

void AHALPlayerController::HALSpawnSlipperyArea()
{
#if !UE_BUILD_SHIPPING
	if (!IsLocalController()) { return; }
	if (HasAuthority()) { ServerSpawnDebugSlipperyArea_Implementation(); }
	else { ServerSpawnDebugSlipperyArea(); }
#endif
}

void AHALPlayerController::ServerSpawnDebugSlipperyArea_Implementation()
{
#if !UE_BUILD_SHIPPING
	const auto* Vehicle = Cast<ATestVehiclePawn>(GetPawn());
	const auto* Settings = GetDefault<UHALDebugSettings>();
	AHALExpandingSlipperyArea* Area = nullptr;
	FString Message;
	if (!HasAuthority() || GetNetMode() == NM_Client || !IsValid(Vehicle)
		|| Vehicle->GetController() != this || !Vehicle->IsConfigurationValid())
	{
		Message = TEXT("HALSpawnSlipperyArea: no valid possessed vehicle.");
	}
	else if (!FMath::IsFinite(Settings->SlipperyAreaForwardDistance)
		|| Settings->SlipperyAreaForwardDistance < 0 || Settings->SlipperyAreaForwardDistance > 5000)
	{
		Message = TEXT("HALSpawnSlipperyArea: invalid forward distance in Project Settings > HAL Debug > Slippery Area.");
	}
	else
	{
		auto* Class = Settings->DefaultSlipperyAreaClass.LoadSynchronous();
		const FVector Forward = Vehicle->GetActorForwardVector().GetSafeNormal2D();
		if (!Class || !Class->IsChildOf(AHALExpandingSlipperyArea::StaticClass())
			|| Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
			|| !Class->GetDefaultObject<AHALExpandingSlipperyArea>()->GetAreaConfig().IsValid())
		{
			Message = TEXT("HALSpawnSlipperyArea: missing/invalid area class or defaults in HAL Debug; no fallback spawned.");
		}
		else if (Forward.IsNearlyZero())
		{
			Message = TEXT("HALSpawnSlipperyArea: vehicle has no horizontal heading.");
		}
		else
		{
			FActorSpawnParameters Params;
			Params.Owner = PlayerState;
			Params.Instigator = GetPawn();
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector Location = Vehicle->GetActorLocation() + Forward * Settings->SlipperyAreaForwardDistance;
			Area = GetWorld()->SpawnActor<AHALExpandingSlipperyArea>(Class, Location, FRotator::ZeroRotator, Params);
			if (IsValid(Area))
			{
				const auto& Config = Area->GetAreaConfig();
				Message = FString::Printf(TEXT("HALSpawnSlipperyArea: Spawned=%s Class=%s Location=%s Expansion=%.2fs Hold=%.2fs MaxRadius=%.1fcm"),
					*Area->GetName(), *Class->GetName(), *Location.ToCompactString(),
					Config.ExpansionSeconds, Config.DurationSeconds, Config.MaximumRadius);
			}
			else { Message = TEXT("HALSpawnSlipperyArea: spawn failed."); }
		}
	}
	UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
	ClientDebugSlipperyAreaResult(Message, IsValid(Area));
#endif
}

void AHALPlayerController::ClientDebugSlipperyAreaResult_Implementation(const FString& Message, bool bSuccess)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority()) { UE_LOG(LogTemp, Display, TEXT("%s"), *Message); }
	if (GEngine) { GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, bSuccess ? FColor::Green : FColor::Yellow, Message); }
#endif
}
