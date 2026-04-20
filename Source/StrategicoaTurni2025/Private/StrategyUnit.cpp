#include "StrategyUnit.h"
#include "GameField.h"
#include "Tile.h"
#include "StrategyGameMode.h"
#include "StrategyTower.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

AStrategyUnit::AStrategyUnit()
{
	// Tick is enabled to allow smooth movement interpolation
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	UnitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UnitMesh"));
	UnitMesh->SetupAttachment(SceneRoot);

	// Default fallback values
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

	// Initialize health to maximum upon spawning
	CurrentHealth = MaxHealth;
}

int32 AStrategyUnit::CalculateDamageToDeal()
{
	// Extract a random damage value between the class minimum and maximum
	return FMath::RandRange(MinDamage, MaxDamage);
}

void AStrategyUnit::ReceiveDamage(int32 DamageAmount)
{
	CurrentHealth -= DamageAmount;

	// Prevent health from dropping below zero
	if (CurrentHealth < 0) CurrentHealth = 0;

	// 1. Update the health bar UI
	OnHealthChanged(CurrentHealth, MaxHealth);

	// 2. Calculate position and spawn floating damage text
	FVector DamageTextLocation = GetActorLocation() + FVector(0.0f, 0.0f, 150.0f);
	OnShowFloatingDamage(DamageAmount, DamageTextLocation);

	// Handle unit defeat and respawn
	if (CurrentHealth <= 0)
	{
		RespawnUnit();
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

		// Z-offset to prevent the unit from clipping through the floor tile
		TargetLocation.Z += 50.0f;

		FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), TargetLocation, DeltaTime, MoveSpeed);
		SetActorLocation(NewLocation);

		if (FVector::Dist(GetActorLocation(), TargetLocation) < 10.0f)
		{
			SetActorLocation(TargetLocation);
			CurrentPathIndex++;

			// Movement sequence completed
			if (CurrentPathIndex >= PathToFollow.Num())
			{
				bIsMoving = false;
				bHasMoved = true;

				if (IsValid(GameFieldRef))
				{
					GameFieldRef->HighlightAttackableTiles(this);

					if (this->UnitTeam == ETeam::Player)
					{
						// If no targets are in range, auto-finish the unit's turn
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
						// AI Auto-Attack Logic
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

						this->bHasAttacked = true;
						this->bIsTurnFinished = true;
						GameFieldRef->ClearAttackableTiles();

						AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
						if (GM)
						{
							// Delay next AI action slightly for visual pacing
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
	if (!IsValid(GameFieldRef))
	{
		bIsTurnFinished = true;
		AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
		if (GM) GM->ProcessAITurn();
		return;
	}

	ATile* TargetTile = nullptr;

	// 1. SEARCH FOR HIGHEST PRIORITY TOWER
	TArray<AActor*> AllTowers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyTower::StaticClass(), AllTowers);

	AStrategyTower* BestTower = nullptr;
	float MinTowerDist = 999999.0f;

	for (AActor* Actor : AllTowers)
	{
		AStrategyTower* Tower = Cast<AStrategyTower>(Actor);

		// Ignore towers already controlled by the AI
		if (Tower && Tower->CurrentState != ETowerState::ControlledAI)
		{
			float Dist = FVector::Dist(GetActorLocation(), Tower->GetActorLocation());
			if (Dist < MinTowerDist)
			{
				MinTowerDist = Dist;
				BestTower = Tower;
			}
		}
	}

	// 2. SEARCH FOR CLOSEST ENEMY
	AStrategyUnit* ClosestEnemy = nullptr;
	float MinEnemyDist = 999999.0f;

	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyUnit::StaticClass(), AllUnits);

	for (AActor* Actor : AllUnits)
	{
		AStrategyUnit* Enemy = Cast<AStrategyUnit>(Actor);
		if (Enemy && Enemy->UnitTeam == ETeam::Player && Enemy->CurrentHealth > 0)
		{
			float Dist = FVector::Dist(GetActorLocation(), Enemy->GetActorLocation());
			if (Dist < MinEnemyDist)
			{
				MinEnemyDist = Dist;
				ClosestEnemy = Enemy;
			}
		}
	}

	// 3. BRAIN: TARGET SELECTION
	// Apply a massive weight reduction to tower distance to prioritize objectives
	if (BestTower && (MinTowerDist - 1500.0f) < MinEnemyDist)
	{
		// Find a valid, walkable tile within the tower's capture radius (Range 2)
		for (auto& Elem : GameFieldRef->TileMap)
		{
			ATile* T = Elem.Value;
			if (T && T->bIsWalkable && T->Status != ETileStatus::OBSTACLE && !IsValid(T->UnitOnTile))
			{
				int32 DistX = FMath::Abs(T->GetGridPosition().X - BestTower->GridPosition.X);
				int32 DistY = FMath::Abs(T->GetGridPosition().Y - BestTower->GridPosition.Y);

				if (DistX <= 2 && DistY <= 2)
				{
					TargetTile = T;
					break;
				}
			}
		}
	}

	// If no valid tower tile is found, fallback to hunting the closest enemy
	if (!TargetTile && ClosestEnemy)
	{
		if (this->AttackType == EAttackType::RANGED)
		{
			// SNIPER BRAIN: Tactical positioning
			ATile* BestSniperTile = nullptr;
			int32 MinDistToSniper = 999999;

			for (auto& Elem : GameFieldRef->TileMap)
			{
				ATile* T = Elem.Value;

				if (T && T->bIsWalkable && T->Status != ETileStatus::OBSTACLE && (!IsValid(T->UnitOnTile) || T == this->CurrentTile))
				{
					int32 DistToEnemy = FMath::Abs(T->GetGridPosition().X - ClosestEnemy->CurrentTile->GetGridPosition().X) +
						FMath::Abs(T->GetGridPosition().Y - ClosestEnemy->CurrentTile->GetGridPosition().Y);

					// Rule 1 & 2: Must be within AttackRange AND possess a height advantage or equal footing
					if (DistToEnemy <= this->AttackRange && T->Elevation >= ClosestEnemy->CurrentTile->Elevation)
					{
						// Rule 3: Minimize movement distance to reach the firing position
						int32 DistToSniper = FMath::Abs(T->GetGridPosition().X - this->CurrentTile->GetGridPosition().X) +
							FMath::Abs(T->GetGridPosition().Y - this->CurrentTile->GetGridPosition().Y);

						if (DistToSniper < MinDistToSniper)
						{
							MinDistToSniper = DistToSniper;
							BestSniperTile = T;
						}
					}
				}
			}

			if (BestSniperTile)
			{
				TargetTile = BestSniperTile;
			}
			else
			{
				// Fallback: If no clear shot is available, move towards the enemy
				TargetTile = ClosestEnemy->CurrentTile;
			}
		}
		else
		{
			// BRAWLER BRAIN: Direct confrontation
			TargetTile = ClosestEnemy->CurrentTile;
		}
	}

	// Absolute fallback: End turn if completely trapped
	if (!TargetTile)
	{
		bHasMovedThisTurn = true; bHasAttacked = true; bIsTurnFinished = true;
		AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
		if (GM) GM->ProcessAITurn();
		return;
	}

	// 4. ALGORITHM EXECUTION
	TArray<ATile*> TargetPath;
	AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());

	if (GM && GM->ActiveAIAlgorithm == EAIAlgorithm::Greedy)
	{
		TargetPath = GameFieldRef->FindPathGreedy(CurrentTile, TargetTile);
	}
	else
	{
		TargetPath = GameFieldRef->FindPathAStar(CurrentTile, TargetTile);
	}

	AIBestTargetTile = CurrentTile;
	int32 AccumulatedCost = 0;

	GameFieldRef->ClearHighlightedTiles();

	// Traverse the generated path and stop at the limit of the unit's MovementRange
	for (ATile* StepTile : TargetPath)
	{
		if (ClosestEnemy && StepTile == ClosestEnemy->CurrentTile) break;

		int32 StepCost = (StepTile->Elevation > AIBestTargetTile->Elevation) ? 2 : 1;

		if (AccumulatedCost + StepCost <= MovementRange && StepTile->UnitOnTile == nullptr)
		{
			AccumulatedCost += StepCost;
			AIBestTargetTile = StepTile;

			// Highlight the AI path with the specific AI visual flag
			StepTile->OnSelectionChanged(true, true);
			GameFieldRef->HighlightedTiles.Add(StepTile);
		}
		else
		{
			break;
		}
	}

	// Delay physical movement to allow the player to see the intended path
	GetWorld()->GetTimerManager().SetTimer(AIThinkTimerHandle, this, &AStrategyUnit::ExecuteAIMovement, 1.5f, false);
}

void AStrategyUnit::ExecuteAIMovement()
{
	if (!IsValid(GameFieldRef)) return;

	GameFieldRef->ClearHighlightedTiles();

	if (AIBestTargetTile != CurrentTile)
	{
		// Cache starting tile for the combat log
		ATile* StartingTile = CurrentTile;

		for (auto& Pair : GameFieldRef->TileMap)
		{
			if (Pair.Value->UnitOnTile == this) { Pair.Value->UnitOnTile = nullptr; break; }
		}

		AIBestTargetTile->UnitOnTile = this;
		CurrentTile = AIBestTargetTile;

		TArray<FVector> Path = GameFieldRef->GetPathToTile(AIBestTargetTile);
		StartMoving(Path);
		bHasMovedThisTurn = true;

		// FORMATTED AI MOVEMENT LOG (Requirement 9)
		AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
		if (GM && StartingTile)
		{
			FString UnitInitial = (this->AttackType == EAttackType::RANGED) ? TEXT("S") : TEXT("B");

			char StartLetter = 'A' + StartingTile->GetGridPosition().Y;
			char EndLetter = 'A' + AIBestTargetTile->GetGridPosition().Y;

			FString MoveMsg = FString::Printf(TEXT("AI: %s %c%d -> %c%d"), *UnitInitial, StartLetter, StartingTile->GetGridPosition().X, EndLetter, AIBestTargetTile->GetGridPosition().X);

			GM->AddGameLog(MoveMsg);
		}
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

	// Apply faction specific materials
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

void AStrategyUnit::AttackTarget(AStrategyUnit* TargetUnit)
{
	if (!IsValid(TargetUnit) || !IsValid(this->CurrentTile) || !IsValid(TargetUnit->CurrentTile)) return;

	// 1. Calculate and apply base damage
	int32 Damage = CalculateDamageToDeal();
	TargetUnit->ReceiveDamage(Damage);

	// FORMATTED COMBAT LOG (Requirement 9)
	AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
	if (GM && TargetUnit && TargetUnit->CurrentTile)
	{
		FString TeamID = (this->UnitTeam == ETeam::Player) ? TEXT("HP") : TEXT("AI");
		FString UnitInitial = (this->AttackType == EAttackType::RANGED) ? TEXT("S") : TEXT("B");

		char TargetLetter = 'A' + TargetUnit->CurrentTile->GetGridPosition().Y;
		int32 TargetNumber = TargetUnit->CurrentTile->GetGridPosition().X;

		FString AttackMsg = FString::Printf(TEXT("ATK: %s Attack %c%d (-%d HP)"), *UnitInitial, TargetLetter, TargetNumber, Damage);
		GM->AddGameLog(AttackMsg);
	}

	// 2. COUNTER-ATTACK RULES
	if (this->AttackType == EAttackType::RANGED)
	{
		int32 DistX = FMath::Abs(this->CurrentTile->GetGridPosition().X - TargetUnit->CurrentTile->GetGridPosition().X);
		int32 DistY = FMath::Abs(this->CurrentTile->GetGridPosition().Y - TargetUnit->CurrentTile->GetGridPosition().Y);
		int32 Distance = DistX + DistY;

		// Snipe counter-attack conditions: Target is a sniper, or target is a brawler at range 1
		bool bTargetIsSniper = (TargetUnit->AttackType == EAttackType::RANGED);
		bool bTargetIsBrawlerAtRange1 = (TargetUnit->AttackType == EAttackType::MELEE && Distance == 1);

		if (bTargetIsSniper || bTargetIsBrawlerAtRange1)
		{
			int32 CounterDamage = FMath::RandRange(1, 3);
			this->ReceiveDamage(CounterDamage);

			// On-screen debug alert for the counter-attack
			if (GEngine) {
				GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Orange, FString::Printf(TEXT("COUNTER-ATTACK! %s takes %d reflect damage!"), *this->UnitLogID, CounterDamage));
			}
		}
	}
}