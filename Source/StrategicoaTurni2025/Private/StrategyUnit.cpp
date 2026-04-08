#include "StrategyUnit.h"
#include "GameField.h"
#include "Tile.h"
#include "StrategyGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

AStrategyUnit::AStrategyUnit()
{
	PrimaryActorTick.bCanEverTick = true; // Come da best practice, disattiviamo il Tick per risparmiare risorse

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

int32 AStrategyUnit::CalculateDamageToDeal()
{
	// Estrae un danno randomico tra il minimo e il massimo
	return FMath::RandRange(MinDamage, MaxDamage);
}

void AStrategyUnit::ReceiveDamage(int32 DamageAmount)
{
	CurrentHealth -= DamageAmount;
	// Assicuriamoci che la vita non vada sotto lo zero
	if (CurrentHealth < 0) CurrentHealth = 0;

	// 1. AGGIORNIAMO LA BARRA DELLA VITA
	OnHealthChanged(CurrentHealth, MaxHealth);

	// 2. CALCOLIAMO LA POSIZIONE E SPAWNIAMO IL TESTO FLUTTUANTE
	FVector DamageTextLocation = GetActorLocation() + FVector(0.0f, 0.0f, 150.0f);
	OnShowFloatingDamage(DamageAmount, DamageTextLocation);

	UE_LOG(LogTemp, Warning, TEXT("L'unità %s ha subito %d danni. Salute rimanente: %d"), *UnitLogID, DamageAmount, CurrentHealth);

	if (CurrentHealth <= 0)
	{
		RespawnUnit();
		UE_LOG(LogTemp, Warning, TEXT("L'unità %s è stata respawnata!"), *UnitLogID);
	}
}

void AStrategyUnit::StartMoving(TArray<FVector> NewPath)
{
	if (NewPath.Num() > 0)
	{
		PathToFollow = NewPath;
		CurrentPathIndex = 0;
		bIsMoving = true;
	}
}

void AStrategyUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsMoving && PathToFollow.IsValidIndex(CurrentPathIndex))
	{
		FVector TargetLocation = PathToFollow[CurrentPathIndex];
		TargetLocation.Z += 50.0f; // Offset per non sprofondare nella cella

		FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), TargetLocation, DeltaTime, MoveSpeed);
		SetActorLocation(NewLocation);

		if (FVector::Dist(GetActorLocation(), TargetLocation) < 10.0f)
		{
			SetActorLocation(TargetLocation);

			CurrentPathIndex++;

			if (CurrentPathIndex >= PathToFollow.Num())
			{
				bIsMoving = false;
				bHasMoved = true;

				UE_LOG(LogTemp, Warning, TEXT("Movimento terminato su X:%d Y:%d"), CurrentTile->GetGridPosition().X, CurrentTile->GetGridPosition().Y);

				if (IsValid(GameFieldRef))
				{
					GameFieldRef->HighlightAttackableTiles(this);

					if (this->UnitTeam == ETeam::Player)
					{
						if (GameFieldRef->AttackableTiles.Num() == 0)
						{
							this->bHasAttacked = true;
							this->bIsTurnFinished = true;
						}

						AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
						if (GM) GM->CheckRemainingMoves();
					}
					else
					{
						bool bAttacked = false;

						for (ATile* TargetTile : GameFieldRef->AttackableTiles)
						{
							if (IsValid(TargetTile->UnitOnTile))
							{
								AStrategyUnit* TargetUnit = Cast<AStrategyUnit>(TargetTile->UnitOnTile);
								if (TargetUnit && TargetUnit->UnitTeam == ETeam::Player)
								{
									this->AttackTarget(TargetUnit);
									bAttacked = true;
									break;
								}
							}
						}

						if (!bAttacked)
						{
							UE_LOG(LogTemp, Warning, TEXT("IA: Nessun bersaglio a tiro per %s."), *this->UnitLogID);
						}

						this->bHasAttacked = true;
						this->bIsTurnFinished = true;
						GameFieldRef->ClearAttackableTiles();

						AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
						if (GM)
						{
							FTimerHandle WaitHandle;
							GetWorld()->GetTimerManager().SetTimer(WaitHandle, GM, &AStrategyGameMode::ProcessAITurn, 1.0f, false);
						}
					}
				}
			}
		}
	}
}

void AStrategyUnit::ExecuteAITurn()
{
	UE_LOG(LogTemp, Warning, TEXT("IA: %s sta pensando col nuovo Algoritmo A*..."), *UnitLogID);

	if (!IsValid(GameFieldRef))
	{
		bIsTurnFinished = true;
		AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
		if (GM) GM->ProcessAITurn();
		return;
	}

	AStrategyUnit* ClosestTarget = nullptr;
	float MinDist = 999999.0f;

	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyUnit::StaticClass(), AllUnits);

	for (AActor* Actor : AllUnits)
	{
		AStrategyUnit* Enemy = Cast<AStrategyUnit>(Actor);
		if (Enemy && Enemy->UnitTeam == ETeam::Player && Enemy->CurrentHealth > 0)
		{
			float Dist = FVector::Dist(GetActorLocation(), Enemy->GetActorLocation());
			if (Dist < MinDist)
			{
				MinDist = Dist;
				ClosestTarget = Enemy;
			}
		}
	}

	if (!ClosestTarget)
	{
		bHasMovedThisTurn = true; bHasAttacked = true; bIsTurnFinished = true;
		AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
		if (GM) GM->ProcessAITurn();
		return;
	}

	TArray<ATile*> AStarPath = GameFieldRef->FindPathAStar(CurrentTile, ClosestTarget->CurrentTile);

	AIBestTargetTile = CurrentTile;
	int32 AccumulatedCost = 0;

	GameFieldRef->ClearHighlightedTiles();

	for (ATile* StepTile : AStarPath)
	{
		if (StepTile == ClosestTarget->CurrentTile) break;

		int32 StepCost = (StepTile->Elevation > AIBestTargetTile->Elevation) ? 2 : 1;

		if (AccumulatedCost + StepCost <= MovementRange && StepTile->UnitOnTile == nullptr)
		{
			AccumulatedCost += StepCost;
			AIBestTargetTile = StepTile;

			StepTile->OnSelectionChanged(true);
			GameFieldRef->HighlightedTiles.Add(StepTile);
		}
		else
		{
			break;
		}
	}

	GetWorld()->GetTimerManager().SetTimer(AIThinkTimerHandle, this, &AStrategyUnit::ExecuteAIMovement, 1.5f, false);
}

void AStrategyUnit::ExecuteAIMovement()
{
	if (!IsValid(GameFieldRef)) return;

	GameFieldRef->ClearHighlightedTiles();

	if (AIBestTargetTile != CurrentTile)
	{
		for (auto& Pair : GameFieldRef->TileMap)
		{
			if (Pair.Value->UnitOnTile == this) { Pair.Value->UnitOnTile = nullptr; break; }
		}

		AIBestTargetTile->UnitOnTile = this;
		CurrentTile = AIBestTargetTile;

		TArray<FVector> Path = GameFieldRef->GetPathToTile(AIBestTargetTile);
		StartMoving(Path);
		bHasMovedThisTurn = true;
	}
	else
	{
		bHasMovedThisTurn = true;
		PathToFollow.Empty();
		PathToFollow.Add(GetActorLocation());
		CurrentPathIndex = 0;
		bIsMoving = true;
	}
}

void AStrategyUnit::RespawnUnit()
{
	UE_LOG(LogTemp, Warning, TEXT("L'unita' %s e' stata sconfitta e torna alla base!"), *UnitLogID);

	if (CurrentTile && CurrentTile->UnitOnTile == this)
	{
		CurrentTile->UnitOnTile = nullptr;
	}

	CurrentHealth = MaxHealth;
	OnHealthChanged(CurrentHealth, MaxHealth);

	if (OriginalSpawnTile)
	{
		CurrentTile = OriginalSpawnTile;
		CurrentTile->UnitOnTile = this;

		FVector NewLocation = CurrentTile->GetActorLocation() + FVector(0, 0, 100);
		SetActorLocation(NewLocation);

		bIsMoving = false;
		PathToFollow.Empty();
	}
}

void AStrategyUnit::InitializeUnit(const FString& InUnitLogID, ETeam InUnitTeam, float InInitialYaw, ATile* StartingTile)
{
	this->UnitLogID = InUnitLogID;
	this->UnitTeam = InUnitTeam;

	this->CurrentTile = StartingTile;
	this->OriginalSpawnTile = StartingTile;
	this->CurrentHealth = MaxHealth;
	this->bHasActedThisTurn = false;

	this->PlayerOwner = (InUnitTeam == ETeam::Player) ? 0 : 1;

	SetActorRotation(FRotator(0.0f, InInitialYaw, 0.0f));

	if (UnitMesh)
	{
		UMaterialInterface* MatToUse = nullptr;

		if (UnitTeam == ETeam::Player)
		{
			MatToUse = PlayerMaterial;
		}
		else if (UnitTeam == ETeam::AI)
		{
			MatToUse = AIMaterial;
		}

		if (MatToUse != nullptr)
		{
			int32 NumMaterials = UnitMesh->GetNumMaterials();
			for (int32 i = 0; i < NumMaterials; ++i)
			{
				UnitMesh->SetMaterial(i, MatToUse);
			}
		}
	}

	OnSetupHealthBar(UnitTeam);
	OnHealthChanged(CurrentHealth, MaxHealth);
}

// =========================================================
// ECCO LA FUNZIONE CHE MANCAVA!
// =========================================================
void AStrategyUnit::AttackTarget(AStrategyUnit* TargetUnit)
{
	if (!IsValid(TargetUnit) || !IsValid(this->CurrentTile) || !IsValid(TargetUnit->CurrentTile)) return;

	// 1. Calcola e infligge il danno base
	int32 Damage = CalculateDamageToDeal();
	TargetUnit->ReceiveDamage(Damage);

	// Log a schermo dell'attacco principale (Rosso)
	if (GEngine) {
		FString AttackerName = (this->UnitTeam == ETeam::Player) ? TEXT("Giocatore") : TEXT("IA");
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("%s: %s infligge %d danni a %s!"), *AttackerName, *this->UnitLogID, Damage, *TargetUnit->UnitLogID));
	}

	// 2. REGOLE DEL CONTRATTACCO 
	if (this->AttackType == EAttackType::RANGED)
	{
		int32 DistX = FMath::Abs(this->CurrentTile->GetGridPosition().X - TargetUnit->CurrentTile->GetGridPosition().X);
		int32 DistY = FMath::Abs(this->CurrentTile->GetGridPosition().Y - TargetUnit->CurrentTile->GetGridPosition().Y);
		int32 Distance = DistX + DistY;

		bool bTargetIsSniper = (TargetUnit->AttackType == EAttackType::RANGED);
		bool bTargetIsBrawlerAtRange1 = (TargetUnit->AttackType == EAttackType::MELEE && Distance == 1);

		if (bTargetIsSniper || bTargetIsBrawlerAtRange1)
		{
			int32 CounterDamage = FMath::RandRange(1, 3);
			this->ReceiveDamage(CounterDamage);

			// Log a schermo del contrattacco (Arancione)
			if (GEngine) {
				GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Orange, FString::Printf(TEXT("CONTRATTACCO! %s subisce %d danni di riflesso!"), *this->UnitLogID, CounterDamage));
			}
		}
	}
}