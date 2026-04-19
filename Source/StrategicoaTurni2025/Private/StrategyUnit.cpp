#include "StrategyUnit.h"
#include "GameField.h"
#include "Tile.h"
#include "StrategyGameMode.h"
#include "StrategyTower.h"
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
	UE_LOG(LogTemp, Warning, TEXT("IA: %s sta pensando col nuovo Algoritmo..."), *UnitLogID);

	if (!IsValid(GameFieldRef))
	{
		bIsTurnFinished = true;
		AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
		if (GM) GM->ProcessAITurn();
		return;
	}

	ATile* TargetTile = nullptr;

	// ==========================================
	// 1. CERCA LA TORRE CON PRIORITA'
	// ==========================================
	TArray<AActor*> AllTowers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyTower::StaticClass(), AllTowers);

	AStrategyTower* BestTower = nullptr;
	float MinTowerDist = 999999.0f;

	for (AActor* Actor : AllTowers)
	{
		AStrategyTower* Tower = Cast<AStrategyTower>(Actor);
		// Puntiamo la torre SOLO se non è già controllata da noi
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

	// ==========================================
	// 2. CERCA IL NEMICO PIU' VICINO
	// ==========================================
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

	// ==========================================
	// 3. IL CERVELLO: SCELTA DEL BERSAGLIO
	// ==========================================
	// Sottraiamo "1500" (circa 15 celle) alla distanza della torre per darle una priorità enorme!
	if (BestTower && (MinTowerDist - 1500.0f) < MinEnemyDist)
	{
		// Abbiamo scelto la torre! Cerchiamo una cella CALPESTABILE nella sua Zona di Cattura (Raggio 2)
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
					UE_LOG(LogTemp, Warning, TEXT("IA: %s punta alla Torre in X:%d Y:%d"), *UnitLogID, BestTower->GridPosition.X, BestTower->GridPosition.Y);
					break;
				}
			}
		}
	}

	// Se non ha trovato una cella per la torre, punta il nemico con logiche diverse per classe
	if (!TargetTile && ClosestEnemy)
	{
		if (this->AttackType == EAttackType::RANGED)
		{
			// --- CERVELLO DELLO SNIPER (A* PER L'ATTACCO) ---
			ATile* BestSniperTile = nullptr;
			int32 MinDistToSniper = 999999;

			for (auto& Elem : GameFieldRef->TileMap)
			{
				ATile* T = Elem.Value;

				// Cerca una cella libera (o quella in cui si trova già lo Sniper, se è già in posizione perfetta!)
				if (T && T->bIsWalkable && T->Status != ETileStatus::OBSTACLE && (!IsValid(T->UnitOnTile) || T == this->CurrentTile))
				{
					int32 DistToEnemy = FMath::Abs(T->GetGridPosition().X - ClosestEnemy->CurrentTile->GetGridPosition().X) +
						FMath::Abs(T->GetGridPosition().Y - ClosestEnemy->CurrentTile->GetGridPosition().Y);

					// Regola 1 & 2: A tiro (<= AttackRange) E in posizione di vantaggio/pari livello
					if (DistToEnemy <= this->AttackRange && T->Elevation >= ClosestEnemy->CurrentTile->Elevation)
					{
						// Regola 3: Trova la più vicina allo Sniper per non sprecare turni di movimento
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
				UE_LOG(LogTemp, Warning, TEXT("IA: Lo Sniper %s cerca l'appostamento in X:%d Y:%d"), *UnitLogID, TargetTile->GetGridPosition().X, TargetTile->GetGridPosition().Y);
			}
			else
			{
				// Paracadute: se il nemico è sulla montagna più alta e non c'è spazio, vacci vicino
				TargetTile = ClosestEnemy->CurrentTile;
			}
		}
		else
		{
			// --- CERVELLO DEL BRAWLER ---
			// Il Brawler è corpo a corpo, deve andare dritto in faccia al nemico
			TargetTile = ClosestEnemy->CurrentTile;
			UE_LOG(LogTemp, Warning, TEXT("IA: Il Brawler %s punta dritto al nemico %s"), *UnitLogID, *ClosestEnemy->UnitLogID);
		}
	}

	// Paracadute di sicurezza finale
	if (!TargetTile)
	{
		bHasMovedThisTurn = true; bHasAttacked = true; bIsTurnFinished = true;
		AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
		if (GM) GM->ProcessAITurn();
		return;
	}

	// ==========================================
	// 4. LANCIO DELL'ALGORITMO (SCELTO DAL PLAYER)
	// ==========================================
	TArray<ATile*> TargetPath;

	// Leggiamo la scelta dal GameMode!
	AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());

	if (GM && GM->ActiveAIAlgorithm == EAIAlgorithm::Greedy)
	{
		UE_LOG(LogTemp, Warning, TEXT("IA: %s usa GREEDY BEST-FIRST!"), *UnitLogID);
		TargetPath = GameFieldRef->FindPathGreedy(CurrentTile, TargetTile);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("IA: %s usa A*!"), *UnitLogID);
		TargetPath = GameFieldRef->FindPathAStar(CurrentTile, TargetTile);
	}

	AIBestTargetTile = CurrentTile;
	int32 AccumulatedCost = 0;

	GameFieldRef->ClearHighlightedTiles();

	// Scorriamo TargetPath (che conterrà o A* o Greedy)
	for (ATile* StepTile : TargetPath)
	{
		if (ClosestEnemy && StepTile == ClosestEnemy->CurrentTile) break;

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
		// SALVIAMO LA CELLA DI PARTENZA PRIMA DI MUOVERCI!
		ATile* StartingTile = CurrentTile;

		for (auto& Pair : GameFieldRef->TileMap)
		{
			if (Pair.Value->UnitOnTile == this) { Pair.Value->UnitOnTile = nullptr; break; }
		}

		AIBestTargetTile->UnitOnTile = this;
		CurrentTile = AIBestTargetTile; // Ora possiamo aggiornarla

		TArray<FVector> Path = GameFieldRef->GetPathToTile(AIBestTargetTile);
		StartMoving(Path);
		bHasMovedThisTurn = true;

		// ==========================================
		// LOG DI MOVIMENTO IA FORMATTATO (Requisito 9)
		// ==========================================
		AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
		if (GM && StartingTile)
		{
			FString UnitInitial = (this->AttackType == EAttackType::RANGED) ? TEXT("S") : TEXT("B");

			char StartLetter = 'A' + StartingTile->GetGridPosition().Y;
			char EndLetter = 'A' + AIBestTargetTile->GetGridPosition().Y;

			// Formato richiesto (es. "AI: B A0 -> B2")
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

	// ==========================================
	// LOG DI ATTACCO FORMATTATO (Requisito 9)
	// ==========================================
	AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
	if (GM && TargetUnit && TargetUnit->CurrentTile)
	{
		// 1. Identificativo Player: "HP" per il Giocatore, "AI" per il computer
		FString TeamID = (this->UnitTeam == ETeam::Player) ? TEXT("HP") : TEXT("AI");

		// 2. Identificativo Unità: "S" per Sniper, "B" per Brawler
		FString UnitInitial = (this->AttackType == EAttackType::RANGED) ? TEXT("S") : TEXT("B");

		// 3. Calcolo della Cella Bersaglio (Lettera Y, Numero X)
		char TargetLetter = 'A' + TargetUnit->CurrentTile->GetGridPosition().Y;
		int32 TargetNumber = TargetUnit->CurrentTile->GetGridPosition().X;

		// 4. Creazione della stringa finale esatta (es. "HP: S G8 -> 7")
		FString AttackMsg = FString::Printf(TEXT("%s: %s %c%d -> %d"), *TeamID, *UnitInitial, TargetLetter, TargetNumber, Damage);

		GM->AddGameLog(AttackMsg);
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