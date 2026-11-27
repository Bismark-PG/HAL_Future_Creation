#include "HALAbilitySystemComponent.h"

#include "HALGameFlowPermissions.h"
#include "HALGameplayTags.h"
#include "HALHealthAttributeSet.h"
#include "HALMatchGameState.h"
#include "HALMatchFeverDefinition.h"
#include "HALRoomComponent.h"
#include "HALRoomConfig.h"
#include "HALSideShiftComponent.h"
#include "HALVehicleUltimateAbility.h"
#include "TestVehiclePawn.h"
#include "TimerManager.h"

namespace
{
	const FGameplayAbilitySpec* PrimarySpec(const UHALAbilitySystemComponent& ASC)
	{
		return ASC.GetActivatableAbilities().FindByPredicate([](const FGameplayAbilitySpec& Spec)
		{
			return !Spec.PendingRemove && Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(HALGameplayTags::Input_Ability_Primary);
		});
	}
}

void UHALAbilitySystemComponent::InitializeUltimateEnergy()
{
	const auto* Vehicle = Cast<ATestVehiclePawn>(GetAvatarActor());
	if (!Vehicle || !Vehicle->HasAuthority() || UltimateFullRechargeSeconds > 0 || !GetSet<UHALUltimateEnergySet>()) { return; }
	UltimateFullRechargeSeconds = Vehicle->GetVehicleAbilityConfig().FullRechargeSeconds;
	LastEnergySample = GetWorld()->GetTimeSeconds();
	SetNumericAttributeBase(UHALUltimateEnergySet::GetEnergyAttribute(), HALUltimate::Capacity);
	RefreshUltimateEnergy();
	FTimerManagerTimerParameters Params;
	Params.bLoop = true;
	Params.bMaxOncePerFrame = true;
	GetWorld()->GetTimerManager().SetTimer(UltimateTimer, this, &ThisClass::RefreshUltimateEnergy, .1f, Params);
}

void UHALAbilitySystemComponent::ClearUltimateEnergy()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(UltimateTimer); }
	if (EnergyRoom.IsValid()) { EnergyRoom->OnRoomChanged.RemoveDynamic(this, &ThisClass::RefreshUltimateEnergy); }
	EnergyRoom.Reset();
	EnergyReservation = 0;
	EnergyRound = 0;
	bFormalEnergy = bUltimateClockRunning = bUltimateActionReady = false;
	if (UltimateFullRechargeSeconds > 0) { SetNumericAttributeBase(UHALUltimateEnergySet::GetEnergyAttribute(), 0); }
	UltimateFullRechargeSeconds = 0;
}

void UHALAbilitySystemComponent::RefreshUltimateEnergy()
{
	const bool WasReady = bUltimateActionReady;
	const bool WasRunning = bUltimateClockRunning;
	UpdateUltimateBalance();
	RefreshUltimateReadiness();
	if ((WasReady != bUltimateActionReady || WasRunning != bUltimateClockRunning) && GetOwner() && GetOwner()->HasAuthority())
	{
		GetOwner()->ForceNetUpdate();
	}
}

void UHALAbilitySystemComponent::UpdateUltimateBalance()
{
	const auto* Pawn = Cast<ATestVehiclePawn>(GetAvatarActor());
	if (!Pawn || !Pawn->HasAuthority() || UltimateFullRechargeSeconds <= 0) { return; }
	const auto* Match = GetWorld()->GetGameState<AHALMatchGameState>();
	auto* Room = Match ? Match->GetRoom() : nullptr;
	if (Room && !Room->IsEnabled()) { Room = nullptr; }
	if (EnergyRoom.Get() != Room)
	{
		if (EnergyRoom.IsValid()) { EnergyRoom->OnRoomChanged.RemoveDynamic(this, &ThisClass::RefreshUltimateEnergy); }
		EnergyRoom = Room;
		if (Room) { Room->OnRoomChanged.AddUniqueDynamic(this, &ThisClass::RefreshUltimateEnergy); }
	}
	const bool Formal = Room && Room->GetData().Phase != EHALRoomPhase::Open;
	const uint32 Round = Formal ? Room->GetData().RoundId : 0;
	const double Now = GetWorld()->GetTimeSeconds();
	const double Elapsed = FMath::Max(0., Now - LastEnergySample);
	LastEnergySample = Now;
	const auto Attribute = UHALUltimateEnergySet::GetEnergyAttribute();
	const auto* Health = GetSet<UHALHealthAttributeSet>();
	const bool MayRecharge = Health && Health->GetCurrentHP() > 0 && IsHealthInitialized()
		&& Pawn->GetController() && Pawn->GetController()->GetPawn() == Pawn
		&& HALGameFlow::Permissions(GetWorld(), Pawn).bAbilities;
	if (Formal != bFormalEnergy || Round != EnergyRound)
	{
		bFormalEnergy = Formal;
		EnergyRound = Round;
		EnergyReservation = 0;
		const auto* Config = Room ? Room->GetServerConfig() : nullptr;
		UltimateSampleServerTime = Now;
		SetNumericAttributeBase(Attribute, Formal ? (Config ? Config->StartingUltimateEnergy : 0.f) : HALUltimate::Capacity);
	}
	else if (MayRecharge && bUltimateClockRunning && Elapsed > 0)
	{
		const float Previous = GetNumericAttributeBase(Attribute);
		if (Previous < HALUltimate::Capacity)
		{
			const float Next = float(FMath::Min(double(HALUltimate::Capacity), Previous + Elapsed * HALUltimate::Capacity / UltimateFullRechargeSeconds));
			UltimateSampleServerTime = Now;
			SetNumericAttributeBase(Attribute, Next);
			const auto* Spec = PrimarySpec(*this);
			const auto* Ability = Spec ? Cast<UHALVehicleUltimateAbility>(Spec->Ability) : nullptr;
			if (Ability)
			{
				const float Cost = Ability->GetUltimateCost();
				if (Cost > 0.f && FMath::FloorToInt(Next / Cost) > FMath::FloorToInt(Previous / Cost))
				{ GetOwner()->ForceNetUpdate(); }
			}
		}
	}
	// Settle the interval above at its previous rate. Room change notification marks the exact rate boundary.
	const auto* Fever = Room ? Room->GetActiveFever() : nullptr;
	const float FullRechargeSeconds = Pawn->GetVehicleAbilityConfig().FullRechargeSeconds / (Fever ? Fever->NaturalEnergyRateMultiplier : 1.f);
	if (UltimateFullRechargeSeconds != FullRechargeSeconds)
	{
		UltimateFullRechargeSeconds = FullRechargeSeconds;
		UltimateSampleServerTime = Now;
		GetOwner()->ForceNetUpdate();
	}
	bUltimateClockRunning = MayRecharge;
}

void UHALAbilitySystemComponent::RefreshUltimateReadiness()
{
	bUltimateActionReady = false;
	const auto* Spec = PrimarySpec(*this);
	if (!bUltimateClockRunning || EnergyReservation || !Spec || Spec->IsActive() || !AbilityActorInfo.IsValid()) { return; }
	const auto* Ability = Cast<UHALVehicleUltimateAbility>(Spec->GetPrimaryInstance() ? Spec->GetPrimaryInstance() : Spec->Ability.Get());
	if (!Ability || !CanPayUltimate(Ability->GetUltimateCost())) { return; }
	if (Ability->IsA<UHALSideShiftAbility>())
	{
		const auto* Shift = GetAvatarActor()->FindComponentByClass<UHALSideShiftComponent>();
		bUltimateActionReady = Shift && Shift->CanStartWithDirection() && !AreAbilityTagsBlocked(Ability->GetAssetTags());
	}
	else { bUltimateActionReady = Ability->CanActivateAbility(Spec->Handle, AbilityActorInfo.Get()); }
}

FHALUltimateEnergyView UHALAbilitySystemComponent::GetUltimateEnergyView() const
{
	FHALUltimateEnergyView View;
	const auto* Attributes = GetSet<UHALUltimateEnergySet>();
	const auto* Spec = PrimarySpec(*this);
	const auto* Ability = Spec ? Cast<UHALVehicleUltimateAbility>(Spec->Ability) : nullptr;
	if (!Attributes || !Ability || UltimateFullRechargeSeconds <= 0) { return View; }
	View.FullRechargeSeconds = UltimateFullRechargeSeconds;
	View.SampleServerTime = UltimateSampleServerTime;
	View.Energy = GetNumericAttributeBase(UHALUltimateEnergySet::GetEnergyAttribute());
	View.Cost = Ability->GetUltimateCost();
	View.bHasEnergy = CanPayUltimate(View.Cost);
	View.bRecharging = bUltimateClockRunning && View.Energy < HALUltimate::Capacity;
	View.bCanActivate = View.bHasEnergy && bUltimateActionReady;
	return View;
}

bool UHALAbilitySystemComponent::CanPayUltimate(float Cost) const
{
	return UltimateFullRechargeSeconds > 0 && Cost > 0 && Cost <= HALUltimate::Capacity && GetSet<UHALUltimateEnergySet>()
		&& GetNumericAttributeBase(UHALUltimateEnergySet::GetEnergyAttribute()) >= Cost;
}

float UHALAbilitySystemComponent::GrantUltimateEnergy(float Points)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !FMath::IsFinite(Points) || Points <= 0) { return 0; }
	RefreshUltimateEnergy();
	const auto* Spec = PrimarySpec(*this);
	if (!bUltimateClockRunning || !Spec || !Spec->Ability->IsA<UHALVehicleUltimateAbility>()) { return 0; }
	const auto Attribute = UHALUltimateEnergySet::GetEnergyAttribute();
	const float Before = GetNumericAttributeBase(Attribute);
	const float Accepted = FMath::Min(Points, HALUltimate::Capacity - Before);
	if (Accepted > 0)
	{
		UltimateSampleServerTime = LastEnergySample;
		SetNumericAttributeBase(Attribute, Before + Accepted);
		RefreshUltimateReadiness();
		GetOwner()->ForceNetUpdate();
	}
	return Accepted;
}

uint32 UHALAbilitySystemComponent::ReserveUltimate(float Cost)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || EnergyReservation) { return 0; }
	UpdateUltimateBalance();
	if (!bUltimateClockRunning || !CanPayUltimate(Cost)) { return 0; }
	if (++NextEnergyReservation == 0) { ++NextEnergyReservation; }
	EnergyReservation = NextEnergyReservation;
	return EnergyReservation;
}

void UHALAbilitySystemComponent::FinishUltimate(uint32 Reservation, float Cost, bool bStarted)
{
	if (!Reservation || EnergyReservation != Reservation) { return; }
	if (bStarted)
	{
		UltimateSampleServerTime = LastEnergySample;
		const auto Attribute = UHALUltimateEnergySet::GetEnergyAttribute();
		SetNumericAttributeBase(Attribute, FMath::Max(0.f, GetNumericAttributeBase(Attribute) - Cost));
		UE_CLOG(HALAbilities::ShouldLog(), LogHALAbility, Log, TEXT("Ultimate spent Owner=%s Cost=%.1f Remaining=%.2f"),
			*GetNameSafe(GetOwner()), Cost, GetNumericAttribute(Attribute));
	}
	EnergyReservation = 0;
	RefreshUltimateReadiness();
	GetOwner()->ForceNetUpdate();
}
