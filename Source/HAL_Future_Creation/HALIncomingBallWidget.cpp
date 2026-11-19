#include "HALIncomingBallWidget.h"
#include "BasicBallActor.h"
#include "HALBallComicFXSettings.h"
#include "HALMagnetBeamPresentationComponent.h"
#include "VehicleHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/App.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "HAL/IConsoleManager.h"

// Off by default. A single player cannot be shot at by someone else, so this is the only way to look at the arrow
// alone: it also announces this player's own shots and balls that are still on screen. Presentation only.
static TAutoConsoleVariable<int32> CVarHALIncomingBallDebug(TEXT("hal.IncomingBallDebug"), 0,
	TEXT("1 = the incoming ball arrow also shows for the local player's own launched balls and for balls on screen."));

bool HALIncomingBall::IsIncoming(const FHALBallComicFXConfig& Config, const FVector& BallLocation, const FVector& BallVelocity,
	const FVector& VehicleLocation, float& OutSecondsToArrival)
{
	OutSecondsToArrival = 0;
	if (BallLocation.ContainsNaN() || BallVelocity.ContainsNaN() || VehicleLocation.ContainsNaN()) { return false; }
	const FVector2D Offset(VehicleLocation.X - BallLocation.X, VehicleLocation.Y - BallLocation.Y);
	const FVector2D Velocity(BallVelocity.X, BallVelocity.Y);
	const double SpeedSquared = Velocity.SizeSquared();
	if (SpeedSquared < FMath::Square(double(Config.IncomingMinimumSpeed))) { return false; }
	const double Seconds = FVector2D::DotProduct(Offset, Velocity) / SpeedSquared;
	if (Seconds <= 0 || Seconds > Config.IncomingMaximumSeconds) { return false; }
	if ((Offset - Velocity * Seconds).SizeSquared() > FMath::Square(double(Config.IncomingMissDistanceCm))) { return false; }
	OutSecondsToArrival = float(Seconds);
	return true;
}

FVector2D HALIncomingBall::EdgePoint(const FVector2D& Direction, float InsetFraction)
{
	const double Half = .5 - FMath::Clamp(double(InsetFraction), 0., .45);
	const FVector2D Unit = Direction.GetSafeNormal();
	if (Unit.IsNearlyZero()) { return FVector2D(.5, .5); }
	// Scale the ray until it touches the inset frame on whichever axis it reaches first.
	const double Scale = Half / FMath::Max(FMath::Abs(Unit.X), FMath::Abs(Unit.Y));
	return FVector2D(.5, .5) + Unit * Scale;
}

TSharedRef<SWidget> UHALIncomingBallWidget::RebuildWidget() { return SNew(SBox); }

void UHALIncomingBallWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UHALIncomingBallWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const FHALBallComicFXConfig& Settings = GetDefault<UHALBallComicFXSettings>()->Comic;
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UVehicleHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
	int32 ViewX = 0, ViewY = 0;
	if (PC) { PC->GetViewportSize(ViewX, ViewY); }
	// No car to warn (spectating, eliminated, between rounds): drop everything at once.
	if (!Settings.bEnabled || !Settings.bIncomingWarning || !Pawn || ViewX <= 0 || ViewY <= 0
		|| (Health && Health->IsHealthReady() && Health->GetCurrentHP() <= 0))
	{ Arrows.Reset(); return; }
	const int32 OwnPlayerId = PC->PlayerState ? PC->PlayerState->GetPlayerId() : INDEX_NONE;
	const bool bDebug = CVarHALIncomingBallDebug.GetValueOnGameThread() != 0;
	for (FHALIncomingBallArrow& Arrow : Arrows) { Arrow.SecondsToArrival = -1; }
	for (TActorIterator<ABasicBallActor> It(GetWorld()); It; ++It)
	{
		const ABasicBallActor* Ball = *It;
		const FBallRepState State = Ball->GetBallRepState();
		const USphereComponent* Root = Ball->GetPhysicsRoot();
		const bool bOwnShot = State.LastLauncherPawn.Get() == Pawn || (OwnPlayerId != INDEX_NONE && State.LastLauncherPlayerId == OwnPlayerId);
		if (State.State != EBasicBallState::Launched || !Root || (bOwnShot && !bDebug)) { continue; }
		float Seconds = 0;
		if (!HALIncomingBall::IsIncoming(Settings, Ball->GetActorLocation(), Root->GetComponentVelocity(), Pawn->GetActorLocation(), Seconds))
		{
			if (!bDebug) { continue; }
			Seconds = 0;
		}
		FVector2D Screen;
		const bool bProjected = PC->ProjectWorldLocationToScreen(Ball->GetActorLocation(), Screen, false);
		const FVector2D Fraction(Screen.X / ViewX, Screen.Y / ViewY);
		// On screen the ink trail already does the job; the arrow is only for what the camera cannot show.
		if (!bDebug && bProjected && Fraction.X >= 0 && Fraction.X <= 1 && Fraction.Y >= 0 && Fraction.Y <= 1) { continue; }
		FVector2D OwnScreen;
		if (!bProjected || !PC->ProjectWorldLocationToScreen(Pawn->GetActorLocation(), OwnScreen, false)) { continue; }
		FHALIncomingBallArrow* Arrow = Arrows.FindByPredicate([Ball](const FHALIncomingBallArrow& Item) { return Item.Ball.Get() == Ball; });
		if (!Arrow) { Arrow = &Arrows.AddDefaulted_GetRef(); Arrow->Ball = Ball; }
		Arrow->Direction = (Screen - OwnScreen).GetSafeNormal();
		Arrow->Position = HALIncomingBall::EdgePoint(Fraction - FVector2D(.5, .5), Settings.IncomingEdgeInsetFraction);
		Arrow->SecondsToArrival = Seconds;
		const auto* Beam = Ball->FindComponentByClass<UHALMagnetBeamPresentationComponent>();
		Arrow->Color = Beam ? Beam->ResolveIdentityColor(State.LastLauncherPawn, State.LastLauncherPlayerId) : FLinearColor::White;
	}
	for (int32 Index = Arrows.Num() - 1; Index >= 0; --Index)
	{
		FHALIncomingBallArrow& Arrow = Arrows[Index];
		const bool bHolds = Arrow.SecondsToArrival >= 0 && Arrow.Ball.IsValid();
		if (bHolds) { Arrow.ConfirmedSeconds += InDeltaTime; }
		if (Arrow.bShown) { Arrow.ShownSeconds += InDeltaTime; }
		// Hysteresis: confirm before showing, then stay for a minimum time, so a grazing ball never flickers.
		if (!Arrow.bShown && bHolds && Arrow.ConfirmedSeconds >= Settings.IncomingConfirmSeconds) { Arrow.bShown = true; Arrow.ShownSeconds = 0; }
		if (!bHolds && (!Arrow.bShown || Arrow.ShownSeconds >= Settings.IncomingMinimumShowSeconds || !Arrow.Ball.IsValid())) { Arrows.RemoveAt(Index); }
	}
	// Soonest first; only the most urgent few are drawn.
	Arrows.Sort([](const FHALIncomingBallArrow& A, const FHALIncomingBallArrow& B)
	{ return (A.SecondsToArrival < 0 ? MAX_flt : A.SecondsToArrival) < (B.SecondsToArrival < 0 ? MAX_flt : B.SecondsToArrival); });
}

int32 UHALIncomingBallWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Base = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FHALBallComicFXConfig& Settings = GetDefault<UHALBallComicFXSettings>()->Comic;
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FVector2f Size(float(LocalSize.X), float(LocalSize.Y));
	if (Arrows.IsEmpty() || Size.X <= 0 || Size.Y <= 0) { return Base; }
	const FSlateResourceHandle Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
	const float Length = Size.Y * Settings.IncomingArrowScreenFraction;
	// A fat doodle arrow pointing along +X, in units of its length: tail slab then head.
	static const FVector2f Outline[] = { {-.5f, -.2f}, {.02f, -.2f}, {.02f, -.46f}, {.5f, 0.f}, {.02f, .46f}, {.02f, .2f}, {-.5f, .2f} };
	static const int32 Triangles[] = { 0, 1, 5, 0, 5, 6, 2, 3, 4 };
	// Look constants of the doodle arrow, in units of its own length.
	constexpr float InkGrow = 1.22f;       // contour pass scale: the black outline around the fill
	constexpr float HeavyInkShift = .06f;  // down-right offset of the heavy ink side
	constexpr float PopStartScale = .3f;   // pop-in: from this scale ...
	constexpr float PopPeakScale = 1.2f;   // ... up to this one, then snap to 1
	constexpr float ColorLift = .15f;      // identity colour lifted toward white, as on the world space doodles
	constexpr int32 PoseCount = 3;
	// Drawn "on twos": the pose changes a few times a second instead of every frame.
	const int32 Pose = int32(FApp::GetCurrentTime() * Settings.IncomingBoilFps) % PoseCount;
	int32 Drawn = 0;
	for (const FHALIncomingBallArrow& Arrow : Arrows)
	{
		if (!Arrow.bShown) { continue; }
		if (++Drawn > Settings.IncomingMaximumArrows) { break; }
		const float Pop = Settings.IncomingPopSeconds > 0 ? FMath::Clamp(Arrow.ShownSeconds / Settings.IncomingPopSeconds, 0.f, 1.f) : 1.f;
		const float Scale = Length * (Pop < 1 ? FMath::Lerp(PopStartScale, PopPeakScale, Pop) : 1.f);
		const float Wobble = FMath::DegreesToRadians(float(Pose - 1) * Settings.IncomingWobbleDegrees);
		const FVector2f Unit(Arrow.Direction.GetSafeNormal());
		const FVector2f AxisX(Unit.X * FMath::Cos(Wobble) - Unit.Y * FMath::Sin(Wobble), Unit.X * FMath::Sin(Wobble) + Unit.Y * FMath::Cos(Wobble));
		const FVector2f AxisY(-AxisX.Y, AxisX.X);
		const FVector2f Centre(float(Arrow.Position.X) * Size.X, float(Arrow.Position.Y) * Size.Y);
		// Three passes: heavy ink side, ink contour, flat fill. Same language as the world space doodles.
		const struct { float Grow; FVector2f Shift; FLinearColor Color; } Passes[] = {
			{ InkGrow, FVector2f(Length * HeavyInkShift, Length * HeavyInkShift), FLinearColor::Black },
			{ InkGrow, FVector2f::ZeroVector, FLinearColor::Black },
			{ 1.f, FVector2f::ZeroVector, FLinearColor::LerpUsingHSV(Arrow.Color, FLinearColor::White, ColorLift) } };
		for (int32 PassIndex = 0; PassIndex < 3; ++PassIndex)
		{
			TArray<FSlateVertex> Vertices;
			TArray<SlateIndex> Indices;
			const FColor Tint = Passes[PassIndex].Color.ToFColor(true);
			for (const FVector2f& Point : Outline)
			{
				const FVector2f Local = Centre + Passes[PassIndex].Shift + (AxisX * Point.X + AxisY * Point.Y) * Scale * Passes[PassIndex].Grow;
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(AllottedGeometry.GetAccumulatedRenderTransform(), Local, FVector2f(.5f), Tint));
			}
			for (const int32 Index : Triangles) { Indices.Add(SlateIndex(Index)); }
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, Base + 1 + PassIndex, Resource, Vertices, Indices, nullptr, 0, 0);
		}
	}
	return Base + 4;
}

void UHALIncomingBallHUDSubsystem::EnsureForWorld(UWorld* World)
{
	const FHALBallComicFXConfig& Settings = GetDefault<UHALBallComicFXSettings>()->Comic;
	if (!World || !Settings.bEnabled || !Settings.bIncomingWarning || !FApp::CanEverRender() || World->GetNetMode() == NM_DedicatedServer) { return; }
	for (auto It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ULocalPlayer* Player = PC && PC->IsLocalController() ? PC->GetLocalPlayer() : nullptr;
		UHALIncomingBallHUDSubsystem* Subsystem = Player ? Player->GetSubsystem<UHALIncomingBallHUDSubsystem>() : nullptr;
		if (!Subsystem) { continue; }
		// A new world or a replaced controller (travel) gets a fresh overlay bound to the current one.
		if (Subsystem->Widget && (Subsystem->Widget->GetWorld() != World || Subsystem->Widget->GetOwningPlayer() != PC))
		{ Subsystem->Widget->RemoveFromParent(); Subsystem->Widget = nullptr; }
		if (!Subsystem->Widget)
		{
			Subsystem->Widget = CreateWidget<UHALIncomingBallWidget>(PC, UHALIncomingBallWidget::StaticClass());
			if (Subsystem->Widget) { Subsystem->Widget->AddToViewport(GetDefault<UHALBallComicFXSettings>()->Comic.IncomingZOrder); }
			if (!Subsystem->Cleanup.IsValid()) { Subsystem->Cleanup = FWorldDelegates::OnWorldCleanup.AddUObject(Subsystem, &ThisClass::CleanupWorld); }
		}
	}
}

void UHALIncomingBallHUDSubsystem::CleanupWorld(UWorld* World, bool, bool)
{
	if (Widget && Widget->GetWorld() == World) { Widget->RemoveFromParent(); Widget = nullptr; }
}

void UHALIncomingBallHUDSubsystem::Deinitialize()
{
	if (Widget) { Widget->RemoveFromParent(); Widget = nullptr; }
	FWorldDelegates::OnWorldCleanup.Remove(Cleanup);
	Super::Deinitialize();
}
