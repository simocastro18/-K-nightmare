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

// 2. LA FUNZIONE DI PARTENZA
void AStrategyUnit::StartMoving(TArray<FVector> NewPath)
{
	if (NewPath.Num() > 0)
	{
		PathToFollow = NewPath;
		CurrentPathIndex = 0;
		bIsMoving = true;
	}
}

// 3. LA MAGIA DELLO SCIVOLAMENTO (TICK)
void AStrategyUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsMoving && PathToFollow.IsValidIndex(CurrentPathIndex))
	{
		FVector TargetLocation = PathToFollow[CurrentPathIndex];
		TargetLocation.Z += 50.0f; // Il tuo offset per non sprofondare nella cella

		// FMath::VInterpConstantTo crea lo scivolamento fluido e costante verso il target
		FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), TargetLocation, DeltaTime, MoveSpeed);
		SetActorLocation(NewLocation);

		// Controlliamo se siamo arrivati vicinissimi al punto bersaglio (distanza minore di 10)
		if (FVector::Dist(GetActorLocation(), TargetLocation) < 10.0f)
		{
			// Snappiamo esattamente al centro per precisione
			SetActorLocation(TargetLocation); 
			
			// Passiamo alla prossima curva del percorso
			CurrentPathIndex++;

			if (CurrentPathIndex >= PathToFollow.Num())
			{
				bIsMoving = false;
				bHasMoved = true;

				UE_LOG(LogTemp, Warning, TEXT("Movimento terminato su X:%d Y:%d"), CurrentTile->GetGridPosition().X, CurrentTile->GetGridPosition().Y);

				if (IsValid(GameFieldRef))
				{
					GameFieldRef->HighlightAttackableTiles(this);

					// SE SONO UN GIOCATORE:
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
					// SE SONO L'INTELLIGENZA ARTIFICIALE: (AUTO-ATTACCO)
					else
					{
						bool bAttacked = false;

						// Cerca tra i bersagli rossi
						for (ATile* TargetTile : GameFieldRef->AttackableTiles)
						{
							if (IsValid(TargetTile->UnitOnTile))
							{
								AStrategyUnit* TargetUnit = Cast<AStrategyUnit>(TargetTile->UnitOnTile);
								if (TargetUnit && TargetUnit->UnitTeam == ETeam::Player)
								{
									int32 Damage = CalculateDamageToDeal();
									TargetUnit->ReceiveDamage(Damage);
									UE_LOG(LogTemp, Error, TEXT("IA: %s attacca e infligge %d danni a %s!"), *this->UnitLogID, Damage, *TargetUnit->UnitLogID);
									bAttacked = true;
									break; // Colpisce il primo che trova e si ferma
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

						// Passa la palla al prossimo nemico
						AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
						if (GM)
						{
							// Mettiamo un micro-ritardo di 1 secondo prima che parta il prossimo nemico per non fare tutto in 1 millisecondo
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
	UE_LOG(LogTemp, Warning, TEXT("IA: %s sta pensando..."), *UnitLogID);

	if (!IsValid(GameFieldRef))
	{
		bIsTurnFinished = true;
		AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
		if (GM) GM->ProcessAITurn();
		return;
	}

	// 1. TROVA IL BERSAGLIO PIU' VICINO
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

	// Se non ci sono bersagli vivi, passa il turno
	if (!ClosestTarget)
	{
		bHasMovedThisTurn = true;
		bHasAttacked = true;
		bIsTurnFinished = true;

		AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
		if (GM) GM->ProcessAITurn();
		return;
	}

	// 2. DOVE POSSO MUOVERMI? (Questo accenderà le celle azzurre per 1.5 secondi)
	GameFieldRef->HighlightReachableTiles(this);

	// 3. SCEGLIE LA CELLA MIGLIORE (Più vicina al bersaglio)
	AIBestTargetTile = CurrentTile;
	float BestDistToTarget = FVector::Dist(CurrentTile->GetActorLocation(), ClosestTarget->GetActorLocation());

	for (ATile* Tile : GameFieldRef->HighlightedTiles)
	{
		float Dist = FVector::Dist(Tile->GetActorLocation(), ClosestTarget->GetActorLocation());
		if (Dist < BestDistToTarget)
		{
			BestDistToTarget = Dist;
			AIBestTargetTile = Tile;
		}
	}

	// 4. PAUSA AD EFFETTO! Lasciamo il percorso azzurro visibile per 1.5 secondi
	GetWorld()->GetTimerManager().SetTimer(AIThinkTimerHandle, this, &AStrategyUnit::ExecuteAIMovement, 1.5f, false);
}

// 5. DOPO LA PAUSA, L'IA SI MUOVE
void AStrategyUnit::ExecuteAIMovement()
{
	if (!IsValid(GameFieldRef)) return;

	// Spengo le luci azzurre
	GameFieldRef->ClearHighlightedTiles();

	// Muoviti!
	if (AIBestTargetTile != CurrentTile)
	{
		// Libero la vecchia
		for (auto& Pair : GameFieldRef->TileMap)
		{
			if (Pair.Value->UnitOnTile == this) { Pair.Value->UnitOnTile = nullptr; break; }
		}

		// Occupo la nuova
		AIBestTargetTile->UnitOnTile = this;
		CurrentTile = AIBestTargetTile;

		TArray<FVector> Path = GameFieldRef->GetPathToTile(AIBestTargetTile);
		StartMoving(Path);
		bHasMovedThisTurn = true;
	}
	else
	{
		// Inganno il Tick per attaccare da fermo
		bHasMovedThisTurn = true;

		PathToFollow.Empty();
		PathToFollow.Add(GetActorLocation());
		CurrentPathIndex = 0;
		bIsMoving = true;
	}
}