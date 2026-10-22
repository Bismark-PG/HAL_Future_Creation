#include "HALPlayerController.h"
#include "HALBombActor.h"
#include "HALDebugSettings.h"
#include "TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

void AHALPlayerController::ServerSpawnDebugBomb_Implementation()
{
#if !UE_BUILD_SHIPPING
	auto* Vehicle = Cast<ATestVehiclePawn>(GetPawn());
	const auto* Settings = GetDefault<UHALDebugSettings>();
	AHALBombActor* Bomb = nullptr;
	FString Message;
	if (!HasAuthority() || GetNetMode() == NM_Client || !Vehicle || Vehicle->GetController() != this || !Vehicle->IsConfigurationValid())
	{
		Message = TEXT("HALSpawnBomb: no valid possessed vehicle.");
	}
	else if (!FMath::IsFinite(Settings->BombForwardDistance) || Settings->BombForwardDistance < 1 || Settings->BombForwardDistance > 5000
		|| !FMath::IsFinite(Settings->BombHeightOffset) || Settings->BombHeightOffset < 0 || Settings->BombHeightOffset > 1000)
	{
		Message = TEXT("HALSpawnBomb: invalid distances in Project Settings > HAL Debug > Bomb.");
	}
	else
	{
		auto* Class = Settings->DefaultBombClass.LoadSynchronous();
		const FVector Forward = Vehicle->GetActorForwardVector().GetSafeNormal2D();
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract) || !Class->GetDefaultObject<AHALBombActor>()->GetBombConfig().IsValid() || Forward.IsNearlyZero())
		{
			Message = TEXT("HALSpawnBomb: invalid bomb class/configuration or horizontal vehicle heading.");
		}
		else
		{
			FVector Origin, Extent;
			Vehicle->GetActorBounds(true, Origin, Extent);
			const float Radius = Class->GetDefaultObject<AHALBombActor>()->GetBodyRadius();
			const float Distance = FMath::Max(Settings->BombForwardDistance, FVector::DotProduct(Forward.GetAbs(),Extent)+Radius+25);
			const FVector Location = Origin+Forward*Distance+FVector(0,0,Settings->BombHeightOffset);
			const FCollisionQueryParams Query(SCENE_QUERY_STAT(DebugBombPlacement),false);
			if (!FMath::IsFinite(Radius) || Radius <= 0 || Radius > 1000
				|| GetWorld()->OverlapBlockingTestByChannel(Location,FQuat::Identity,ECC_PhysicsBody,FCollisionShape::MakeSphere(Radius),Query))
			{
				Message = TEXT("HALSpawnBomb: invalid radius or blocked position; move away from obstacles and retry.");
			}
			else
			{
				FActorSpawnParameters Params;
				Params.Owner = PlayerState;
				Params.Instigator = Vehicle;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
				Bomb = GetWorld()->SpawnActor<AHALBombActor>(Class,Location,FRotator::ZeroRotator,Params);
				if (IsValid(Bomb) && (Bomb->IsArmed() || Bomb->StartCountdown()))
				{
					Message = FString::Printf(TEXT("HALSpawnBomb: Spawned=%s Fuse=%.2fs Radius=%.1fcm Damage=%.1f Tier=%d (authority countdown started)."),
						*Bomb->GetName(),Bomb->GetBombConfig().FuseSeconds,Bomb->GetBombConfig().ExplosionRadius,Bomb->GetBombConfig().Damage,int32(Bomb->GetBombConfig().ImpactTier));
				}
				else
				{
					if (IsValid(Bomb)) { Bomb->Destroy(); }
					Bomb = nullptr;
					Message = TEXT("HALSpawnBomb: spawn/countdown failed; no live test bomb retained.");
				}
			}
		}
	}
	UE_LOG(LogTemp,Display,TEXT("%s"),*Message);
	ClientDebugBombResult(Message, IsValid(Bomb));
#endif
}

void AHALPlayerController::ClientDebugBombResult_Implementation(const FString& Message, bool bSuccess)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority()) { UE_LOG(LogTemp,Display,TEXT("%s"),*Message); }
	if (GEngine) { GEngine->AddOnScreenDebugMessage(INDEX_NONE,5,bSuccess?FColor::Green:FColor::Yellow,Message); }
#endif
}
