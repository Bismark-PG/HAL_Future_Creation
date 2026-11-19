#include "HALVehicleDoodleStatusComponent.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"
#include "VehicleHealthComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"

bool FHALVehicleDoodleStatusConfig::Validate(FString& Error) const
{
	Error.Reset();
	if (!bEnabled) { return true; }
	if (!FMath::IsFinite(StunSizeWidthFraction) || StunSizeWidthFraction <= 0 || !FMath::IsFinite(LowHealthSizeWidthFraction) || LowHealthSizeWidthFraction <= 0)
	{ Error = TEXT("Doodle status sprite sizes must be positive."); return false; }
	if (!FMath::IsFinite(StunLiftWidthFraction) || StunLiftWidthFraction < 0 || !FMath::IsFinite(LowHealthLiftWidthFraction) || LowHealthLiftWidthFraction < 0)
	{ Error = TEXT("Doodle status lifts must not be negative."); return false; }
	if (!FMath::IsFinite(LowHealthFraction) || LowHealthFraction <= 0 || LowHealthFraction >= 1)
	{ Error = TEXT("Low health fraction must be between zero and one."); return false; }
	return true;
}

#if WITH_EDITOR
bool UHALVehicleDoodleStatusSettings::CollectCookPackages(TArray<FName>& Packages, FString& Error) const
{
	Error.Reset();
	if (!Status.Validate(Error)) { return false; }
	if (!Status.bEnabled) { return true; }
	for (const TSoftObjectPtr<UMaterialInterface>& Material : { Status.StunMaterial, Status.LowHealthMaterial })
	{
		if (Material.IsNull()) { continue; }
		const FString Package = Material.ToSoftObjectPath().GetLongPackageName();
		if (!FPackageName::IsValidLongPackageName(Package) || !Material.LoadSynchronous())
		{ Error = FString::Printf(TEXT("Doodle status material %s: missing package or wrong asset class."), *Material.ToString()); return false; }
		Packages.AddUnique(FName(*Package));
	}
	return true;
}
#endif

bool HALVehicleDoodleStatus::ShowStunStars(bool bStunned, float CurrentHP)
{
	return bStunned && FMath::IsFinite(CurrentHP) && CurrentHP > 0;
}

bool HALVehicleDoodleStatus::ShowLowHealth(const FHALVehicleDoodleStatusConfig& Config, float CurrentHP, float MaxHP)
{
	if (!Config.bLowHealth || !FMath::IsFinite(CurrentHP) || !FMath::IsFinite(MaxHP) || MaxHP <= 0 || CurrentHP <= 0) { return false; }
	return CurrentHP / MaxHP <= Config.LowHealthFraction;
}

UHALVehicleDoodleStatusComponent::UHALVehicleDoodleStatusComponent()
{
	// Ticks only while a mark is on screen, to keep it upright over a car that tumbles.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UHALVehicleDoodleStatusComponent::Bind(UHALAbilitySystemComponent* InAbilitySystem, UVehicleHealthComponent* InHealth)
{
	Unbind();
	const FHALVehicleDoodleStatusConfig& Settings = GetDefault<UHALVehicleDoodleStatusSettings>()->Status;
	FString Error;
	if (!InAbilitySystem || !Settings.bEnabled || GetNetMode() == NM_DedicatedServer || !FApp::CanEverRender()) { return; }
	if (!Settings.Validate(Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("HAL DoodleStatus: disabled, settings invalid: %s"), *Error);
		return;
	}
	// Resolved once at bind time; a tag or HP change never loads an asset synchronously.
	if (!StunMaterial && !Settings.StunMaterial.IsNull()) { StunMaterial = Settings.StunMaterial.LoadSynchronous(); }
	if (Settings.bLowHealth && !LowHealthMaterial && !Settings.LowHealthMaterial.IsNull()) { LowHealthMaterial = Settings.LowHealthMaterial.LoadSynchronous(); }
	AbilitySystem = InAbilitySystem;
	Health = InHealth;
	StunHandle = InAbilitySystem->RegisterGameplayTagEvent(HALGameplayTags::State_Stun, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::StunChanged);
	if (InHealth) { InHealth->OnHealthChanged.AddDynamic(this, &ThisClass::HealthChanged); }
	bReady = true;
	Refresh();
}

void UHALVehicleDoodleStatusComponent::Unbind()
{
	if (UHALAbilitySystemComponent* ASC = AbilitySystem.Get(); ASC && StunHandle.IsValid())
	{ ASC->RegisterGameplayTagEvent(HALGameplayTags::State_Stun, EGameplayTagEventType::NewOrRemoved).Remove(StunHandle); }
	StunHandle.Reset();
	if (UVehicleHealthComponent* Bound = Health.Get()) { Bound->OnHealthChanged.RemoveDynamic(this, &ThisClass::HealthChanged); }
	AbilitySystem.Reset();
	Health.Reset();
	bReady = false;
	Refresh();
}

void UHALVehicleDoodleStatusComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Unbind();
	Super::EndPlay(Reason);
}

void UHALVehicleDoodleStatusComponent::StunChanged(FGameplayTag, int32) { Refresh(); }
void UHALVehicleDoodleStatusComponent::HealthChanged(float, float) { Refresh(); }

UMaterialBillboardComponent* UHALVehicleDoodleStatusComponent::EnsureSprite(TObjectPtr<UMaterialBillboardComponent>& Sprite,
	UMaterialInterface* Material, float SizeCm)
{
	AActor* Owner = GetOwner();
	if (IsValid(Sprite) || !IsValid(Material) || !IsValid(Owner) || !Owner->GetRootComponent()) { return Sprite; }
	Sprite = NewObject<UMaterialBillboardComponent>(Owner);
	if (!Sprite) { return nullptr; }
	Sprite->SetupAttachment(Owner->GetRootComponent());
	// Placed in world space above the car: a mark must stay overhead when the car rolls over.
	Sprite->SetUsingAbsoluteLocation(true);
	Sprite->SetUsingAbsoluteRotation(true);
	Sprite->SetUsingAbsoluteScale(true);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sprite->SetCastShadow(false);
	Sprite->bReceivesDecals = false;
	Sprite->SetVisibility(false);
	Sprite->RegisterComponent();
	// BaseSize is the half extent, so the configured size stays a real width in cm.
	Sprite->AddElement(Material, nullptr, false, SizeCm * .5f, SizeCm * .5f, nullptr);
	return Sprite;
}

void UHALVehicleDoodleStatusComponent::Refresh()
{
	const FHALVehicleDoodleStatusConfig& Settings = GetDefault<UHALVehicleDoodleStatusSettings>()->Status;
	const UHALAbilitySystemComponent* ASC = AbilitySystem.Get();
	const UVehicleHealthComponent* Bound = Health.Get();
	const bool bHealthKnown = Bound && Bound->IsHealthReady();
	// Without a health component (a test dummy) the car counts as alive for the stars.
	const float CurrentHP = bHealthKnown ? Bound->GetCurrentHP() : 1.f;
	bStars = bReady && ASC && HALVehicleDoodleStatus::ShowStunStars(ASC->HasMatchingGameplayTag(HALGameplayTags::State_Stun), CurrentHP);
	bLowHealth = bReady && bHealthKnown && HALVehicleDoodleStatus::ShowLowHealth(Settings, CurrentHP, Bound->GetMaxHP());
	const float BodyWidth = GetBodyWidthCm();
	if (bStars && BodyWidth > 0) { EnsureSprite(StunSprite, StunMaterial, BodyWidth * Settings.StunSizeWidthFraction); }
	if (bLowHealth && BodyWidth > 0) { EnsureSprite(LowHealthSprite, LowHealthMaterial, BodyWidth * Settings.LowHealthSizeWidthFraction); }
	if (bStars || bLowHealth) { PlaceSprites(); }
	if (IsValid(StunSprite)) { StunSprite->SetVisibility(bStars); }
	if (IsValid(LowHealthSprite)) { LowHealthSprite->SetVisibility(bLowHealth); }
	SetComponentTickEnabled(bStars || bLowHealth);
}

float UHALVehicleDoodleStatusComponent::GetBodyWidthCm() const
{
	const AActor* Owner = GetOwner();
	const UPrimitiveComponent* Body = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	// Local bounds times scale: the car's own width, whatever way it is facing or rolling.
	return Body ? float(2. * Body->CalcBounds(FTransform::Identity).BoxExtent.Y * FMath::Abs(Body->GetComponentScale().Y)) : 0.f;
}

void UHALVehicleDoodleStatusComponent::PlaceSprites()
{
	const AActor* Owner = GetOwner();
	const UPrimitiveComponent* Body = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	if (!Body) { return; }
	const FHALVehicleDoodleStatusConfig& Settings = GetDefault<UHALVehicleDoodleStatusSettings>()->Status;
	// World bounds, world up: independent of the car's roll and pitch.
	const FBoxSphereBounds Bounds = Body->Bounds;
	const FVector Top = Bounds.Origin + FVector(0, 0, Bounds.BoxExtent.Z);
	const float BodyWidth = GetBodyWidthCm();
	if (IsValid(StunSprite)) { StunSprite->SetWorldLocation(Top + FVector(0, 0, BodyWidth * Settings.StunLiftWidthFraction)); }
	if (IsValid(LowHealthSprite)) { LowHealthSprite->SetWorldLocation(Top + FVector(0, 0, BodyWidth * Settings.LowHealthLiftWidthFraction)); }
}

void UHALVehicleDoodleStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (!bStars && !bLowHealth) { SetComponentTickEnabled(false); return; }
	PlaceSprites();
}
