// Copyright Epic Games, Inc. All Rights Reserved.

#include "HALDebugHUDWidget.h"

#include "ArcadeVehicleMovementComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HALMatchGameState.h"
#include "HAL/IConsoleManager.h"
#include "Styling/CoreStyle.h"
#include "TestVehiclePawn.h"
#include "TimerManager.h"
#include "VehicleHealthComponent.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDebugHUD(TEXT("hal.DebugHUD"), 0,
		TEXT("Show read-only local vehicle/GAS diagnostics (0=off, 1=on). Development only."), ECVF_Cheat);

	int32 GetVehiclePlayerId(const ATestVehiclePawn* Vehicle)
	{
		const APlayerState* State = Vehicle ? Vehicle->GetPlayerState() : nullptr;
		return State ? State->GetPlayerId() : INDEX_NONE;
	}

	FString GetHealthStatus(const ATestVehiclePawn* Vehicle)
	{
		const UVehicleHealthComponent* Health = Vehicle
			? Vehicle->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
		if (!Health || !Health->IsHealthReady())
		{
			return TEXT("Ready=0  HP=pending");
		}
		return FString::Printf(TEXT("Ready=1  HP=%.1f / %.1f"), Health->GetCurrentHP(), Health->GetMaxHP());
	}
}

TSharedRef<SWidget> UHALDebugHUDWidget::RebuildWidget()
{
	return SAssignNew(StatusPanel, SBox).WidthOverride(400.0f)
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.015f, 0.025f, 0.04f, 0.72f))
		.Padding(FMargin(12.0f, 10.0f))
		[
			SAssignNew(StatusText, STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
			.ColorAndOpacity(FLinearColor(0.85f, 0.95f, 1.0f))
			.WrapTextAt(376.0f)
		]
	];
}

void UHALDebugHUDWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	StatusText.Reset();
	StatusPanel.Reset();
	CachedViewportScale = 0.0f;
}

void UHALDebugHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshStatus();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &UHALDebugHUDWidget::RefreshStatus, 0.1f, true);
	}
}

void UHALDebugHUDWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

void UHALDebugHUDWidget::RefreshStatus()
{
	const bool bEnabled = CVarDebugHUD.GetValueOnGameThread() != 0;
	SetVisibility(bEnabled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bEnabled || !StatusText.IsValid()) { return; }

	// Keep diagnostics legible at small test-window resolutions despite the project's DPI curve.
	const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.1f);
	if (!FMath::IsNearlyEqual(CachedViewportScale, ViewportScale))
	{
		CachedViewportScale = ViewportScale;
		if (StatusPanel) { StatusPanel->SetWidthOverride(400.0f / ViewportScale); }
		StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(14.0f / ViewportScale)));
		StatusText->SetWrapTextAt(376.0f / ViewportScale);
	}

	const APlayerController* Controller = GetOwningPlayer();
	const UWorld* World = GetWorld();
	if (!Controller || !World) { return; }
	const AHALMatchGameState* MatchState = World->GetGameState<AHALMatchGameState>();
	const TCHAR* Phase = !MatchState ? TEXT("Unavailable")
		: MatchState->GetMatchPhase() == EHALMatchPhase::Playing ? TEXT("Playing") : TEXT("WaitingForPlayers");
	const TCHAR* Mode = World->GetNetMode() == NM_Client ? TEXT("Client")
		: World->GetNetMode() == NM_ListenServer ? TEXT("Listen Host") : TEXT("Standalone");
	const ATestVehiclePawn* Vehicle = Cast<ATestVehiclePawn>(Controller->GetPawn());
	FString Text = FString::Printf(TEXT("HAL DEBUG  |  %s  |  %s\nLocal PlayerId=%d  %s\n"),
		Mode, Phase, Controller->PlayerState ? Controller->PlayerState->GetPlayerId() : INDEX_NONE,
		*GetHealthStatus(Vehicle));
	if (Vehicle)
	{
		// Read the game-thread body snapshot; movement's live async physics fields must not be read here.
		const UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Vehicle->GetRootComponent());
		const FVector Velocity = Body ? Body->GetPhysicsLinearVelocity() : FVector::ZeroVector;
		const float ForwardSpeed = Body ? FVector::DotProduct(Velocity, Body->GetForwardVector()) : 0.0f;
		const FVehicleInputCmd Input = Vehicle->GetCurrentInputCommand();
		const UArcadeVehicleMovementComponent* Movement = Vehicle->FindComponentByClass<UArcadeVehicleMovementComponent>();
		Text += FString::Printf(TEXT("Speed XY=%.0f cm/s (%.1f km/h)\nForward=%.0f cm/s  Vertical=%.0f cm/s\n"
			"Input: Throttle=%.2f Brake=%.2f Steer=%.2f\nHandbrake=%d  Launch=%d\nPhysics frame=%d  Resim runs=%d\n"),
			Velocity.Size2D(), Velocity.Size2D() * 0.036f, ForwardSpeed, Velocity.Z,
			Input.Throttle, Input.Brake, Input.Steering, Input.bHandbrake ? 1 : 0, Input.bLaunch ? 1 : 0,
			Movement ? Movement->GetLastPhysicsFrame() : INDEX_NONE,
			Movement ? Movement->GetResimulationRunCount() : 0);
	}
	else
	{
		Text += TEXT("Waiting for local vehicle...\n");
	}

	// PlayerId is stable across processes; transient actor suffixes need not match.
	TArray<ATestVehiclePawn*> Vehicles;
	for (TActorIterator<ATestVehiclePawn> It(World); It; ++It) { Vehicles.Add(*It); }
	Vehicles.Sort([](const ATestVehiclePawn& A, const ATestVehiclePawn& B)
	{
		return GetVehiclePlayerId(&A) < GetVehiclePlayerId(&B);
	});
	Text += TEXT("--- Vehicles (this process's view) ---\n");
	for (const ATestVehiclePawn* Other : Vehicles)
	{
		Text += FString::Printf(TEXT("P%d%s  %s\n"), GetVehiclePlayerId(Other),
			Other == Vehicle ? TEXT(" [YOU]") : TEXT(""), *GetHealthStatus(Other));
	}
	Text += TEXT("hal.DebugHUD 0: hide");
	StatusText->SetText(FText::FromString(Text));
}
