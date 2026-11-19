#pragma once
#include "CoreMinimal.h"
#include "HALRoomTypes.h"
#include "UObject/Interface.h"
#include "HALFrontEndViewTypes.generated.h"

class UTexture2D;
class UWidget;

/** Local display values, never a request payload or an authoritative rule store. */
USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALEntryView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bAttract=true;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bConnecting=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") FText Message;
	UPROPERTY(BlueprintReadOnly, Category="UI") FString LastHostAddress;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALVehicleDisplayView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="UI") FName VehicleId;
	UPROPERTY(BlueprintReadOnly, Category="UI") FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category="UI") FText Description;
	UPROPERTY(BlueprintReadOnly, Category="UI") TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(BlueprintReadOnly, Category="UI") TSoftObjectPtr<UTexture2D> PreviewImage;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALRoomSeatView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="UI") int32 Seat=INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category="UI") int32 Number=0;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bOccupied=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bLocal=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bHost=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bClientLoaded=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") FLinearColor Color=FLinearColor::Gray;
	UPROPERTY(BlueprintReadOnly, Category="UI") FString PlayerName;
	UPROPERTY(BlueprintReadOnly, Category="UI") FName VehicleId;
	UPROPERTY(BlueprintReadOnly, Category="UI") EHALRoomPlayerPhase Phase=EHALRoomPlayerPhase::Unassigned;
};

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALRoomView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bRoomAvailable=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bCatalogLoaded=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bMenuVisible=true;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bHost=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bRequestPending=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bCanSelect=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bCanConfirm=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bCanCancel=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bCanStart=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bCanResume=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bCanReturnToRoom=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bCanLeave=false;
	UPROPERTY(BlueprintReadOnly, Category="UI") EHALRoomPhase Phase=EHALRoomPhase::Inactive;
	UPROPERTY(BlueprintReadOnly, Category="UI") EHALRoomPlayerPhase LocalPhase=EHALRoomPlayerPhase::Unassigned;
	UPROPERTY(BlueprintReadOnly, Category="UI") int32 LocalNumber=0;
	UPROPERTY(BlueprintReadOnly, Category="UI") FLinearColor LocalColor=FLinearColor::White;
	UPROPERTY(BlueprintReadOnly, Category="UI") FName SelectedVehicleId;
	UPROPERTY(BlueprintReadOnly, Category="UI") FText SelectedVehicleName;
	UPROPERTY(BlueprintReadOnly, Category="UI") FText Description;
	UPROPERTY(BlueprintReadOnly, Category="UI") TObjectPtr<UTexture2D> LoadedPreview;
	UPROPERTY(BlueprintReadOnly, Category="UI") FText Status;
	UPROPERTY(BlueprintReadOnly, Category="UI") int32 MinimumPlayers=0;
	UPROPERTY(BlueprintReadOnly, Category="UI") int32 LoadedPlayers=0;
	UPROPERTY(BlueprintReadOnly, Category="UI") int32 ExpectedPlayers=0;
	UPROPERTY(BlueprintReadOnly, Category="UI") int64 RoundId=0;
	/** Display only, derived from the shared server deadline; never starts the match. */
	UPROPERTY(BlueprintReadOnly, Category="UI") float CountdownRemaining=0;
	UPROPERTY(BlueprintReadOnly, Category="Match") FHALRoundMatchData Match;
	UPROPERTY(BlueprintReadOnly, Category="Match") float MatchRemaining=0;
	UPROPERTY(BlueprintReadOnly, Category="Match") float ResultsRemaining=0;
	UPROPERTY(BlueprintReadOnly, Category="Match") bool bShowResults=false;
	UPROPERTY(BlueprintReadOnly, Category="Match") bool bSpectating=false;
	UPROPERTY(BlueprintReadOnly, Category="Match") bool bCanCycleSpectator=false;
	UPROPERTY(BlueprintReadOnly, Category="Match") int32 SpectatedNumber=0;
	UPROPERTY(BlueprintReadOnly, Category="Match") FText ResultSummary;
	UPROPERTY(BlueprintReadOnly, Category="UI") TArray<FHALRoomSeatView> Seats;
	UPROPERTY(BlueprintReadOnly, Category="UI") TArray<FHALVehicleDisplayView> Vehicles;
};

/** Local presentation only. Pending requests never freeze phase transitions. */
UENUM(BlueprintType)
enum class EHALRoomScreen : uint8 { Selection, Practice, Loading, Countdown, Match, Spectator, Results, Transition };

USTRUCT(BlueprintType)
struct HAL_FUTURE_CREATION_API FHALRoomPresentation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="UI") EHALRoomScreen Screen=EHALRoomScreen::Transition;
	UPROPERTY(BlueprintReadOnly, Category="UI") bool bShowLocalMenu=false;
};

/** Contract shared by Designer children of the room root. Does not own gameplay state. */
UINTERFACE(BlueprintType)
class HAL_FUTURE_CREATION_API UHALRoomScreenInterface : public UInterface
{
	GENERATED_BODY()
};
class HAL_FUTURE_CREATION_API IHALRoomScreenInterface
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="UI|Room Screen") void ApplyView(const FHALRoomView& View);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="UI|Room Screen") void SetScreenActive(bool bActive);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="UI|Room Screen") UWidget* GetDefaultFocusTarget() const;
};
