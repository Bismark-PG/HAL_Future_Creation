#include "HALSkillTestDummy.h"
#include "HALAbilitySystemComponent.h"
#include "HALHealthAttributeSet.h"
#include "HALCombatAttributeSet.h"
#include "HALImpactAttributeSet.h"
#include "HALMovementAttributeSet.h"
#include "HALGameFlowPermissions.h"
#include "VehicleDefinition.h"
#include "VehicleHealthComponent.h"
#include "HALImpactReceiverComponent.h"
#include "HALPursuitTargetComponent.h"
#include "HALMovementAreaReceiver.h"
#include "HALGameplayTags.h"
#include "Physics/NetworkPhysicsSettingsComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Misc/DataValidation.h"

bool UHALSkillTestDummyConfig::Validate(FString& Error) const
{
	if (!VehicleDefinition || !VehicleDefinition->Validate(Error)) { if (Error.IsEmpty()) { Error=TEXT("Set a valid vehicle Definition."); } return false; }
	if (VehicleDefinition->CollisionShape != EVehicleCollisionShape::ConvexMesh)
	{ Error=TEXT("Skill dummy requires a formal ConvexMesh vehicle, not a Passive Box Definition."); return false; }
	if (Label.IsEmpty() || Label.ToString().Len()>40) { Error=TEXT("Use a label between 1 and 40 characters."); return false; }
	return true;
}
#if WITH_EDITOR
EDataValidationResult UHALSkillTestDummyConfig::IsDataValid(FDataValidationContext& Context) const
{ FString Error; if (Validate(Error)) { return EDataValidationResult::Valid; } Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
#endif

AHALSkillTestDummy::AHALSkillTestDummy()
{
	ConfigurationSource=EVehicleConfigurationSource::Definition;
	AutoPossessPlayer=EAutoReceiveInput::Disabled; AutoPossessAI=EAutoPossessAI::Disabled;
	PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bStartWithTickEnabled=false; PrimaryActorTick.TickGroup=TG_PostPhysics;
	DummyAbilitySystem=CreateDefaultSubobject<UHALAbilitySystemComponent>(TEXT("DummyAbilitySystem"));
	DummyAbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	HealthAttributes=CreateDefaultSubobject<UHALHealthAttributeSet>(TEXT("DummyHealth")); DummyAbilitySystem->AddAttributeSetSubobject(HealthAttributes.Get());
	CombatAttributes=CreateDefaultSubobject<UHALCombatAttributeSet>(TEXT("DummyCombat")); DummyAbilitySystem->AddAttributeSetSubobject(CombatAttributes.Get());
	ImpactAttributes=CreateDefaultSubobject<UHALImpactAttributeSet>(TEXT("DummyImpact")); DummyAbilitySystem->AddAttributeSetSubobject(ImpactAttributes.Get());
	MovementAttributes=CreateDefaultSubobject<UHALMovementAttributeSet>(TEXT("DummyMovement")); DummyAbilitySystem->AddAttributeSetSubobject(MovementAttributes.Get());
	TestHealth=CreateDefaultSubobject<UWidgetComponent>(TEXT("TestHealth")); TestHealth->SetupAttachment(GetRootComponent());
	TestHealth->SetWidgetSpace(EWidgetSpace::Screen); TestHealth->SetWidgetClass(UHALSkillTestDummyWidget::StaticClass());
	TestHealth->SetDrawSize(FVector2D(150,60)); TestHealth->SetPivot(FVector2D(.5f,.5f)); TestHealth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AHALSkillTestDummy::OnConstruction(const FTransform& Transform)
{
	ConfigurationSource=EVehicleConfigurationSource::Definition; Definition=TestConfig?TestConfig->VehicleDefinition.Get():nullptr;
	Super::OnConstruction(Transform);
}
void AHALSkillTestDummy::PreInitializeComponents()
{
	AutoPossessPlayer=EAutoReceiveInput::Disabled; AutoPossessAI=EAutoPossessAI::Disabled;
	ConfigurationSource=EVehicleConfigurationSource::Definition; Definition=TestConfig?TestConfig->VehicleDefinition.Get():nullptr;
	// This unowned body uses ordinary server-authoritative rigid-body replication, never owner history.
	NetworkPhysicsSettings->SettingsDataAsset=nullptr;
	if (GetNetMode()==NM_Client && !TestConfig)
	{
		// Per-instance config arrives in the initial bunch after component construction.
		APawn::PreInitializeComponents(); return;
	}
	Super::PreInitializeComponents();
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		FString Error;
		if (!TestConfig || !TestConfig->Validate(Error))
		{ bConfigurationValid=false; CollisionRoot->SetSimulatePhysics(false); CollisionRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			UE_LOG(LogTemp,Error,TEXT("Skill dummy %s: %s"),*GetName(),*Error); }
		DummyAbilitySystem->InitAbilityActorInfo(this,this);
	}
}
UAbilitySystemComponent* AHALSkillTestDummy::GetAbilitySystemComponent() const { return DummyAbilitySystem; }
bool AHALSkillTestDummy::ConfigureDummy()
{
	FString Error;
	ConfigurationSource=EVehicleConfigurationSource::Definition; Definition=TestConfig?TestConfig->VehicleDefinition.Get():nullptr;
	bConfigurationValid=TestConfig && TestConfig->Validate(Error) && ApplyDefinition(false) && ImpactReceiver->CheckConfiguredRules();
	if (!bConfigurationValid)
	{ CollisionRoot->SetSimulatePhysics(false); CollisionRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision); UE_LOG(LogTemp,Error,TEXT("Skill dummy config failed: %s"),*Error); }
	return bConfigurationValid;
}
void AHALSkillTestDummy::PostNetInit()
{
	if (GetNetMode()==NM_Client && !HasActorBegunPlay()) { ConfigureDummy(); DummyAbilitySystem->InitAbilityActorInfo(this,this); }
	Super::PostNetInit();
}
void AHALSkillTestDummy::InitializeAbilitySystem()
{
	if (!IsConfigurationValid()) { return; }
	DummyAbilitySystem->InitAbilityActorInfo(this,this);
	BindVehicleAbilitySystem(DummyAbilitySystem,INDEX_NONE);
	// Deliberately no GrantVehicleAbilities, PlayerState or synthetic controller.
}
bool AHALSkillTestDummy::CanReceiveSkillEffects() const
{
	return !IsActorBeingDestroyed() && IsConfigurationValid() && DummyAbilitySystem->GetAvatarActor()==this
		&& DummyAbilitySystem->IsCombatInitialized() && DummyAbilitySystem->IsImpactInitialized()
		&& Health->IsHealthReady() && Health->GetCurrentHP()>0
		&& HALGameFlow::Permissions(GetWorld(),this).bWorldInteractions;
}
void AHALSkillTestDummy::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		// No room elimination callback owns this actor. Retire accepted states on defeat, keeping HP readable.
		DummyAbilitySystem->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute()).AddWeakLambda(this,
			[this](const FOnAttributeChangeData& Change)
			{
				if (Change.NewValue>0 || IsActorBeingDestroyed()) { return; }
				PursuitTarget->Clear(); MovementAreaReceiver->ClearForOwnerExit();
				DummyAbilitySystem->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(FGameplayTagContainer(HALGameplayTags::Effect_Scope_Avatar)));
			});
	}
	if (FApp::CanEverRender() && GetNetMode()!=NM_DedicatedServer && TestConfig && TestConfig->bShowHealth)
	{
		TestHealth->SetWidgetClass(TestConfig->WidgetClass?TestConfig->WidgetClass:TSubclassOf<UHALSkillTestDummyWidget>(UHALSkillTestDummyWidget::StaticClass()));
		TestHealth->InitWidget(); if (auto* W=Cast<UHALSkillTestDummyWidget>(TestHealth->GetUserWidgetObject())) { W->SetDummy(this); }
		SetActorTickEnabled(true); Tick(0);
	}
	else { TestHealth->SetVisibility(false); }
}
void AHALSkillTestDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FVector Center,Extent; GetActorBounds(true,Center,Extent);
	TestHealth->SetWorldLocation(Center+FVector(0,0,Extent.Z+30));
}
void AHALSkillTestDummy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AHALSkillTestDummy,TestConfig); }
