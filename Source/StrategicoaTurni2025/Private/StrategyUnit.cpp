#include "StrategyUnit.h"
#include "Math/UnrealMathUtility.h"

AStrategyUnit::AStrategyUnit()
{
	PrimaryActorTick.bCanEverTick = false; // Come da best practice, disattiviamo il Tick per risparmiare risorse

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	UnitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UnitMesh"));
	UnitMesh->SetupAttachment(SceneRoot);

	// Valori di Paracadute
	UnitLogID = TEXT("U");
	MovementRange = 0;
	AttackType = EAttackType::MELEE;
	AttackRange = 1;
	MinDamage = 1;
	MaxDamage = 1;
	MaxHealth = 1;

	CurrentHealth = 1;
	PlayerOwner = -1;
	CurrentTile = nullptr;
	bHasActedThisTurn = false;
}

void AStrategyUnit::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth; // All'avvio la vita è al massimo
}

void AStrategyUnit::InitializeUnit(int32 InOwnerID, ATile* StartingTile)
{
	PlayerOwner = InOwnerID;
	CurrentTile = StartingTile;
	CurrentHealth = MaxHealth;
	bHasActedThisTurn = false;
}

int32 AStrategyUnit::CalculateDamageToDeal()
{
	// Estrae un danno randomico tra il minimo e il massimo
	return FMath::RandRange(MinDamage, MaxDamage);
}

void AStrategyUnit::ReceiveDamage(int32 DamageAmount)
{
	CurrentHealth -= DamageAmount;

	UE_LOG(LogTemp, Warning, TEXT("L'unità %s ha subito %d danni. Salute rimanente: %d"), *UnitLogID, DamageAmount, CurrentHealth);

	if (CurrentHealth <= 0)
	{
		CurrentHealth = 0;
		// Rimuoviamo l'unità dalla griglia (verrà poi gestito il respawn come da PDF)
		UE_LOG(LogTemp, Warning, TEXT("L'unità %s è stata distrutta!"), *UnitLogID);
		// TODO: Avvisare il GameMode prima di distruggere l'Actor per gestire il Respawn
		Destroy();
	}
}