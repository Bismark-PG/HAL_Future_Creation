#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HALRoomTypes.h"
#include "HALRoomComponent.generated.h"

class UHALRoomConfig;
class AHALPlayerController;
class APawn;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHALRoomChanged);

/** Dormant on legacy NetTest. Only server GameMode may initialize/register/advance it. No Tick or spawning. */
UCLASS()
class HAL_FUTURE_CREATION_API UHALRoomComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHALRoomComponent();
	UFUNCTION(BlueprintPure, Category="Room") const FHALRoomData& GetData() const { return Data; }
	bool IsEnabled() const { return Data.Phase != EHALRoomPhase::Inactive; }
	bool CanAdmitPlayer() const;
	EHALRoomResult Initialize(UHALRoomConfig* Config);
	void EnablePracticeRoom();
	void CloseForReturn();
	const UHALRoomConfig* GetServerConfig() const { return ServerConfig; }
	EHALRoomResult AddMember(AHALPlayerController* Controller);
	void RemoveMember(AHALPlayerController* Controller);
	EHALRoomResult Request(AHALPlayerController* Controller, EHALRoomAction Action, FName VehicleId, uint32 ExpectedRevision);
	/** GF-3 native callback after actual Avatar initialization; clients cannot declare themselves ready. */
	EHALRoomResult CompleteAvatar(AHALPlayerController* Controller, uint32 ExpectedRevision, uint32 ExpectedRound = 0);
	EHALRoomResult FailAvatar(AHALPlayerController* Controller, uint32 ExpectedRevision, EHALRoomResult Reason);
	bool CanInitializeAvatar(const AHALPlayerController* Controller, const APawn* Next) const;
	void InvalidateAvatarForLifecycle(AHALPlayerController* Controller);
	EHALRoomResult CheckStart(const AHALPlayerController* Controller) const;
	/** GF-4 will call after its travel/readiness barrier; this contract does not implement that barrier. */
	EHALRoomResult AdvanceRound(uint32 ExpectedRound, EHALRoomPhase Next);
	bool AllowsPlayerActions(const APawn* Pawn) const;
	UPROPERTY(BlueprintAssignable, Category="Room") FHALRoomChanged OnRoomChanged;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
	AHALPlayerState* Member(const AHALPlayerController* Controller) const;
	bool AvatarReady(const AHALPlayerController* Controller) const;
	void Publish();
	UFUNCTION() void OnRep_Data();
	UPROPERTY(ReplicatedUsing=OnRep_Data) FHALRoomData Data;
	UPROPERTY(Transient) TObjectPtr<UHALRoomConfig> ServerConfig;
};
