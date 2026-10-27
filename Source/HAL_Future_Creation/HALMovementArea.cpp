#include "HALMovementArea.h"
#include "HALMovementAreaReceiver.h"
#include "TestVehiclePawn.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "HAL/IConsoleManager.h"

AHALMovementArea::AHALMovementArea()
{
	PrimaryActorTick.bCanEverTick = false; bReplicates = true;
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("AreaBounds")); SetRootComponent(Bounds);
	Bounds->SetBoxExtent(FVector(500, 500, 200)); Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Bounds->SetCollisionObjectType(ECC_WorldDynamic); Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	Bounds->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap); Bounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Bounds->SetGenerateOverlapEvents(true); Bounds->SetCanEverAffectNavigation(false);
	Bounds->ShapeColor = FColor::Cyan;
	Bounds->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OverlapBegan);
	Bounds->OnComponentEndOverlap.AddDynamic(this, &ThisClass::OverlapEnded);
}
void AHALMovementArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALMovementArea, Settings); DOREPLIFETIME(AHALMovementArea, bEnabled);
}
void AHALMovementArea::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) { Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision); return; }
	if (!Settings.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: invalid movement area settings; disabled."), *GetNameSafe(this)); bEnabled = false;
	}
	Bounds->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (bEnabled) { ReconcileOverlaps(); }
}
bool AHALMovementArea::ConfigureArea(const FHALMovementAreaConfig& Config)
{
	if (!HasAuthority() || !Config.IsValid()) { return false; }
	Settings = Config;
	const auto Previous = Vehicles;
	for (const auto& Entry : Previous)
	{
		if (auto* Vehicle = Entry.Get()) { RegisterVehicle(Vehicle); }
		else { Vehicles.Remove(Entry); }
	}
	ForceNetUpdate(); return true;
}
void AHALMovementArea::SetAreaEnabled(bool bInEnabled)
{
	if (!HasAuthority() || (bInEnabled && !Settings.IsValid())) { return; }
	bEnabled = bInEnabled;
	if (!bEnabled) { ClearVehicles(); }
	Bounds->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (bEnabled) { ReconcileOverlaps(); }
	ForceNetUpdate();
}
void AHALMovementArea::RegisterVehicle(ATestVehiclePawn* Vehicle)
{
	if (!HasAuthority() || !bEnabled || !IsValid(Vehicle) || !Settings.IsValid()) { return; }
	if (auto* Receiver = Vehicle->FindComponentByClass<UHALMovementAreaReceiver>(); Receiver && Receiver->RegisterArea(this, Settings))
	{ Vehicles.Add(Vehicle); Vehicle->OnEndPlay.AddUniqueDynamic(this, &ThisClass::VehicleEnded); }
}
void AHALMovementArea::ReconcileOverlaps()
{
	Bounds->UpdateOverlaps();
	TArray<UPrimitiveComponent*> Components; Bounds->GetOverlappingComponents(Components);
	for (auto* Component : Components)
	{
		auto* Vehicle = Component ? Cast<ATestVehiclePawn>(Component->GetOwner()) : nullptr;
		if (Vehicle && Component == Vehicle->GetRootComponent()) { RegisterVehicle(Vehicle); }
	}
}
void AHALMovementArea::OverlapBegan(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	int32 BodyIndex, bool bFromSweep, const FHitResult& Hit)
{
	if (!HasActorBegunPlay()) { return; }
	if (auto* Vehicle = Cast<ATestVehiclePawn>(OtherActor); Vehicle && OtherComponent == Vehicle->GetRootComponent()) { RegisterVehicle(Vehicle); }
}
void AHALMovementArea::OverlapEnded(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 BodyIndex)
{
	const auto* Log = IConsoleManager::Get().FindConsoleVariable(TEXT("hal.MovementAreaLog"));
	if (Log && Log->GetInt())
	{
		UE_LOG(LogTemp, Log, TEXT("Movement area exit: Area=%s Other=%s Component=%s Root=%s Authority=%d"),
			*GetNameSafe(this), *GetNameSafe(OtherActor), *GetNameSafe(OtherComponent),
			*GetNameSafe(OtherActor ? OtherActor->GetRootComponent() : nullptr), HasAuthority());
	}
	if (!HasAuthority()) { return; }
	if (auto* Vehicle = Cast<ATestVehiclePawn>(OtherActor); Vehicle && OtherComponent == Vehicle->GetRootComponent())
	{
		Vehicles.Remove(Vehicle);
		Vehicle->OnEndPlay.RemoveDynamic(this, &ThisClass::VehicleEnded);
		if (auto* Receiver = Vehicle->FindComponentByClass<UHALMovementAreaReceiver>()) { Receiver->UnregisterArea(this); }
	}
}
void AHALMovementArea::VehicleEnded(AActor* Actor, EEndPlayReason::Type Reason)
{
	if (auto* Vehicle = Cast<ATestVehiclePawn>(Actor)) { Vehicles.Remove(Vehicle); }
}
void AHALMovementArea::ClearVehicles()
{
	const auto Previous = MoveTemp(Vehicles); Vehicles.Reset();
	for (const auto& Entry : Previous)
	{
		if (auto* Vehicle = Entry.Get())
		{
			Vehicle->OnEndPlay.RemoveDynamic(this, &ThisClass::VehicleEnded);
			if (auto* Receiver = Vehicle->FindComponentByClass<UHALMovementAreaReceiver>()) { Receiver->UnregisterArea(this); }
		}
	}
}
void AHALMovementArea::EndPlay(const EEndPlayReason::Type Reason) { ClearVehicles(); Super::EndPlay(Reason); }
