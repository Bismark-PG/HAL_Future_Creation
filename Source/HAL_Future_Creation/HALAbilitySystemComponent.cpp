#include "HALAbilitySystemComponent.h"
#include "HALStunEffect.h"
#include "HALShockedEffect.h"
#include "AbilitySystemGlobals.h"
#include "BallControlComponent.h"
#include "VehicleRecoveryComponent.h"

#include "HAL/IConsoleManager.h"
#include "HALHealthAttributeSet.h"
#include "HALCombatAttributeSet.h"
#include "HALImpactAttributeSet.h"
#include "HALGameplayTags.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogHALGAS, Log, All);
static TAutoConsoleVariable<int32> CVarHALGASLog(TEXT("hal.GASLog"), 0,
	TEXT("Log authority health initialization and Avatar cleanup. 0=off, 1=on."));

UHALAbilitySystemComponent::UHALAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
}

void UHALAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();
	StunTagHandle = RegisterGameplayTagEvent(HALGameplayTags::State_Stun, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::StunTagChanged);
	AbilityFailedCallbacks.AddUObject(this, &ThisClass::LogAbilityFailed);
	OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &ThisClass::LogEffectAdded);
	OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &ThisClass::LogEffectRemoved);
}

void UHALAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RegisterGameplayTagEvent(HALGameplayTags::State_Stun, EGameplayTagEventType::NewOrRemoved).Remove(StunTagHandle);
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		CancelAllAbilities();
		RemoveActiveEffects(FGameplayEffectQuery());
		ClearAllAbilities();
	}
	AbilityFailedCallbacks.RemoveAll(this);
	OnActiveGameplayEffectAddedDelegateToSelf.RemoveAll(this);
	OnAnyGameplayEffectRemovedDelegate().RemoveAll(this);
	BoundAvatarKey = FObjectKey();
	Super::EndPlay(EndPlayReason);
}

void UHALAbilitySystemComponent::RemoveAvatarEffects()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	CancelAllAbilities();
	const int32 Removed = RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(
		FGameplayTagContainer(HALGameplayTags::Effect_Scope_Avatar)));
	if (HALAbilities::ShouldLog())
	{
		UE_LOG(LogHALAbility, Log, TEXT("Avatar effects cleared: Owner=%s Removed=%d Cooldown=%.2fs"),
			*GetNameSafe(GetOwner()), Removed, GetEffectTimeRemaining(HALGameplayTags::Cooldown_ImpactImmunity));
	}
}

void UHALAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	const FObjectKey NextAvatar(InAvatarActor);
	if (BoundAvatarKey != FObjectKey() && BoundAvatarKey != NextAvatar) { RemoveAvatarEffects(); }
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	BoundAvatarKey = NextAvatar;
}

void UHALAbilitySystemComponent::GrantAbilities(const TArray<FHALAbilityGrant>& Grants, UObject* Source)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Source) { return; }
	TSet<FGameplayTag> Inputs;
	for (const auto& Grant : Grants)
	{
		if (!Grant.Ability || Grant.Ability->HasAnyClassFlags(CLASS_Abstract) || !Grant.InputTag.IsValid()
			|| Grant.Level < 1 || Inputs.Contains(Grant.InputTag))
		{
			UE_LOG(LogHALAbility, Error, TEXT("Invalid or duplicate input grant on %s."), *GetNameSafe(Source));
			continue;
		}
		Inputs.Add(Grant.InputTag);
		const bool bExists = GetActivatableAbilities().ContainsByPredicate([&](const FGameplayAbilitySpec& Spec)
		{
			return Spec.Ability && Spec.Ability->GetClass() == Grant.Ability && Spec.SourceObject == Source
				&& Spec.GetDynamicSpecSourceTags().HasTagExact(Grant.InputTag);
		});
		if (bExists) { continue; }
		FGameplayAbilitySpec Spec(Grant.Ability, Grant.Level, INDEX_NONE, Source);
		Spec.GetDynamicSpecSourceTags().AddTag(Grant.InputTag);
		GiveAbility(Spec);
	}
}

bool UHALAbilitySystemComponent::HasAbilityForInput(FGameplayTag InputTag) const
{
	return InputTag.IsValid() && GetActivatableAbilities().ContainsByPredicate([&](const FGameplayAbilitySpec& Spec)
	{
		return Spec.Ability && !Spec.PendingRemove && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag);
	});
}

bool UHALAbilitySystemComponent::TryActivateAbilityInput(FGameplayTag InputTag)
{
	if (!InputTag.IsValid() || !GetAvatarActor() || IsStunned()) { return false; }
	if (const auto* Recovery = GetAvatarActor()->FindComponentByClass<UVehicleRecoveryComponent>(); Recovery && Recovery->IsExecuting()) { return false; }
	TArray<FGameplayAbilitySpecHandle> Handles;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (const auto& Spec : GetActivatableAbilities())
		{
			if (Spec.Ability && !Spec.PendingRemove && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) { Handles.Add(Spec.Handle); }
		}
	}
	const bool bRequested = Handles.Num() == 1 && TryActivateAbility(Handles[0], true);
	if (HALAbilities::ShouldLog())
	{
		UE_LOG(LogHALAbility, Log, TEXT("Ability input: Owner=%s Avatar=%s Authority=%d Input=%s Matches=%d Requested=%d"),
			*GetNameSafe(GetOwner()), *GetNameSafe(GetAvatarActor()), GetOwner()->HasAuthority(),
			*InputTag.ToString(), Handles.Num(), bRequested);
	}
	return bRequested;
}

bool UHALAbilitySystemComponent::AreAbilityTagsBlocked(const FGameplayTagContainer& Tags) const
{
	const auto* Recovery = GetAvatarActor() ? GetAvatarActor()->FindComponentByClass<UVehicleRecoveryComponent>() : nullptr;
	return IsStunned() || (Recovery && Recovery->IsExecuting()) || Super::AreAbilityTagsBlocked(Tags);
}

bool UHALAbilitySystemComponent::IsStunned() const
{
	return HasMatchingGameplayTag(HALGameplayTags::State_Stun);
}

bool UHALAbilitySystemComponent::IsActorStunned(const AActor* Actor)
{
	const auto* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	return ASC && ASC->HasMatchingGameplayTag(HALGameplayTags::State_Stun);
}

bool UHALAbilitySystemComponent::ApplyStun(float DurationSeconds, AActor* SourceActor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(GetAvatarActor())
		|| !FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.f) { return false; }
	const auto Previous = GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(
		FGameplayTagContainer(HALGameplayTags::Effect_Stun)));
	if (IsStunned())
	{
		const float Remaining = GetEffectTimeRemaining(HALGameplayTags::State_Stun);
		if (Remaining < 0.f || Remaining >= DurationSeconds) { return true; }
	}
	auto Context = MakeEffectContext();
	Context.AddInstigator(IsValid(SourceActor) ? SourceActor : GetAvatarActor(), SourceActor);
	const auto Spec = MakeOutgoingSpec(UHALStunEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid()) { return false; }
	Spec.Data->SetDuration(DurationSeconds, true);
	const auto Applied = ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!Applied.IsValid()) { return false; }
	// Add before removing so refresh never emits a false end/start or another drop.
	for (const auto Handle : Previous) { if (Handle != Applied) { RemoveActiveGameplayEffect(Handle); } }
	if (HALAbilities::ShouldLog())
	{
		UE_LOG(LogHALAbility, Log, TEXT("Stun applied: Avatar=%s Source=%s Duration=%.3fs"),
			*GetNameSafe(GetAvatarActor()), *GetNameSafe(SourceActor), DurationSeconds);
	}
	return true;
}

void UHALAbilitySystemComponent::StunTagChanged(FGameplayTag Tag, int32 Count)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	if (Count <= 0)
	{
		RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(FGameplayTagContainer(HALGameplayTags::Effect_Visual_Shocked)));
		return;
	}
	if (!GetAvatarActor()) { return; }
	if (auto* Balls = GetAvatarActor()->FindComponentByClass<UBallControlComponent>()) { Balls->ReleaseForStun(); }
}

bool UHALAbilitySystemComponent::MarkShockedForCurrentStun(AActor* SourceActor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(GetAvatarActor()) || !IsStunned()) { return false; }
	if (HasMatchingGameplayTag(HALGameplayTags::State_Visual_Shocked)) { return true; }
	auto Context = MakeEffectContext(); Context.AddSourceObject(SourceActor);
	const auto Spec = MakeOutgoingSpec(UHALShockedEffect::StaticClass(),1.f,Context);
	return Spec.IsValid() && ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).IsValid();
}

float UHALAbilitySystemComponent::GetEffectTimeRemaining(FGameplayTag GrantedTag) const
{
	if (!HasMatchingGameplayTag(GrantedTag)) { return 0; }
	const TArray<float> Times = GetActiveEffectsTimeRemaining(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(GrantedTag)));
	if (Times.IsEmpty()) { return -1; } // Loose tags / remote Mixed replication do not carry a duration.
	float Remaining = 0;
	for (float Time : Times) { if (Time < 0) { return -1; } Remaining = FMath::Max(Remaining, Time); }
	return Remaining;
}

void UHALAbilitySystemComponent::LogAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& Reasons)
{
	if (HALAbilities::ShouldLog())
	{
		UE_LOG(LogHALAbility, Log, TEXT("Ability rejected: Owner=%s Avatar=%s Ability=%s Reasons=%s"),
			*GetNameSafe(GetOwner()), *GetNameSafe(GetAvatarActor()), *GetNameSafe(Ability), *Reasons.ToStringSimple());
	}
}

void UHALAbilitySystemComponent::LogEffectAdded(UAbilitySystemComponent* Target, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
	if (HALAbilities::ShouldLog())
	{
		UE_LOG(LogHALAbility, Log, TEXT("Effect added: Owner=%s Authority=%d Effect=%s Duration=%.2fs Stacks=%d"),
			*GetNameSafe(GetOwner()), GetOwner()->HasAuthority(), *GetNameSafe(Spec.Def), Spec.GetDuration(), Spec.GetStackCount());
	}
}

void UHALAbilitySystemComponent::LogEffectRemoved(const FActiveGameplayEffect& Effect)
{
	if (HALAbilities::ShouldLog())
	{
		UE_LOG(LogHALAbility, Log, TEXT("Effect removed: Owner=%s Authority=%d Effect=%s"),
			*GetNameSafe(GetOwner()), GetOwner()->HasAuthority(), *GetNameSafe(Effect.Spec.Def));
	}
}

void UHALAbilitySystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHALAbilitySystemComponent, bHealthInitialized);
	DOREPLIFETIME(UHALAbilitySystemComponent, bCombatInitialized);
	DOREPLIFETIME(UHALAbilitySystemComponent, bImpactInitialized);
	DOREPLIFETIME(UHALAbilitySystemComponent, CombatConfigVersion);
}

bool UHALAbilitySystemComponent::InitializeCombat(const FVehicleCombatConfig& Config)
{
	FString Error;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Config.Validate(Error)
		|| !GetSet<UHALCombatAttributeSet>()) { return false; }
	if (bCombatInitialized) { return true; }
	CombatConfigVersion = Config.Version;
	const bool bMigrated = Config.Version == EHALCombatConfigVersion::AttackSnapshotV1;
	// Legacy assets never silently become scaled-attack configurations.
	SetNumericAttributeBase(UHALCombatAttributeSet::GetAttackPowerAttribute(), bMigrated ? Config.AttackPower : 0.0f);
	SetNumericAttributeBase(UHALCombatAttributeSet::GetOutputMultiplierDeltaAttribute(), bMigrated ? Config.OutputMultiplierDelta : 0.0f);
	SetNumericAttributeBase(UHALCombatAttributeSet::GetDamageTakenMultiplierAttribute(), bMigrated ? Config.DamageTakenMultiplier : 1.0f);
	bCombatInitialized = true;
	GetOwner()->ForceNetUpdate();
	return true;
}

bool UHALAbilitySystemComponent::InitializeImpact(const FHALVehicleImpactConfig& Config)
{
	FString Error;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Config.Validate(Error)
		|| !GetSet<UHALImpactAttributeSet>()) { return false; }
	if (bImpactInitialized) { return true; }
	SetNumericAttributeBase(UHALImpactAttributeSet::GetTierReductionAttribute(),
		Config.Version == EHALImpactVersion::TieredV1 ? Config.TierReduction : 0);
	bImpactInitialized = true;
	GetOwner()->ForceNetUpdate();
	return true;
}

bool UHALAbilitySystemComponent::InitializeHealth(float SavedMaxHP)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !FMath::IsFinite(SavedMaxHP) || SavedMaxHP < 1.0f
		|| !GetSet<UHALHealthAttributeSet>())
	{
		return false;
	}
	if (bHealthInitialized) { return true; }
	SetNumericAttributeBase(UHALHealthAttributeSet::GetMaxHPAttribute(), SavedMaxHP);
	SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(), SavedMaxHP);
	bHealthInitialized = true;
	GetOwner()->ForceNetUpdate();
	OnHealthInitialized.Broadcast();
	if (CVarHALGASLog.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogHALGAS, Log, TEXT("Health initialized Owner=%s Avatar=%s MaxHP=%.1f"),
			*GetNameSafe(GetOwner()), *GetNameSafe(GetAvatarActor()), SavedMaxHP);
	}
	return true;
}

void UHALAbilitySystemComponent::ClearAvatarIfCurrent(const AActor* ExpectedAvatar)
{
	if (!ExpectedAvatar || BoundAvatarKey != FObjectKey(ExpectedAvatar)) { return; }
	InitAbilityActorInfo(GetOwner(), nullptr);
	if (CVarHALGASLog.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogHALGAS, Log, TEXT("Avatar cleared Owner=%s FormerAvatar=%s"),
			*GetNameSafe(GetOwner()), *GetNameSafe(ExpectedAvatar));
	}
}

void UHALAbilitySystemComponent::OnRep_HealthInitialized()
{
	OnHealthInitialized.Broadcast();
}

void UHALAbilitySystemComponent::LogHealthStatus() const
{
	const UHALHealthAttributeSet* Attributes = GetSet<UHALHealthAttributeSet>();
	if (Attributes && CVarHALGASLog.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogHALGAS, Log, TEXT("Health state Owner=%s Avatar=%s Authority=%d HP=%.1f MaxHP=%.1f"),
			*GetNameSafe(GetOwnerActor()), *GetNameSafe(GetAvatarActor()), GetOwner()->HasAuthority() ? 1 : 0,
			Attributes->GetCurrentHP(), Attributes->GetMaxHP());
	}
}
