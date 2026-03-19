#include "StrategyGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "StrategyTower.h"
#include "StrategyUnit.h"
#include "Tile.h"
#include "Tile.h"

void AStrategyGameMode::BeginPlay()
{
	Super::BeginPlay();
	//CurrentTurnState = ETurnState::PlayerTurn;
	//UE_LOG(LogTemp, Warning, TEXT("=== INIZIO PARTITA: TURNO DEL GIOCATORE ==="));

	AActor* FoundField = UGameplayStatics::GetActorOfClass(GetWorld(), AGameField::StaticClass());
	if (FoundField)
	{
		MapGenerator = Cast<AGameField>(FoundField);
	}
}

void AStrategyGameMode::StartGameWithConfig(float InNoiseScale, int32 InGridSizeX, int32 InGridSizeY)
{
	if (MapGenerator)
	{
		MapGenerator->NoiseScale = InNoiseScale;
		MapGenerator->GridSizeX = InGridSizeX;
		MapGenerator->GridSizeY = InGridSizeY;
		MapGenerator->GenerateGridData();
		MapGenerator->SpawnInitialEntities();
		// LANCIO DELLA MONETA (Random 50/50) 
		if (FMath::RandBool())
		{
			CurrentTurnState = ETurnState::PlayerTurn;
			UE_LOG(LogTemp, Warning, TEXT("=== LANCIO MONETA: Vince il GIOCATORE! ==="));
		}
		else
		{
			CurrentTurnState = ETurnState::AITurn;
			UE_LOG(LogTemp, Warning, TEXT("=== LANCIO MONETA: Vince l'IA! ==="));
			// Avviamo l'IA se ha vinto lei
			GetWorldTimerManager().SetTimerForNextTick(this, &AStrategyGameMode::ProcessAITurn);
		}
	}
}

void AStrategyGameMode::CheckRemainingMoves()
{
	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyUnit::StaticClass(), AllUnits);

	bool bPlayerCanAct = false;
	bool bAICanAct = false;

	for (AActor* Actor : AllUnits)
	{
		AStrategyUnit* Unit = Cast<AStrategyUnit>(Actor);
		if (Unit && !Unit->bIsTurnFinished)
		{
			if (Unit->UnitTeam == ETeam::Player) bPlayerCanAct = true;
			else bAICanAct = true;
		}
	}

	if (CurrentTurnState == ETurnState::PlayerTurn && !bPlayerCanAct)
	{
		EndTurn();
	}
}

void AStrategyGameMode::EndTurn()
{
	EvaluateTowers();
	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyUnit::StaticClass(), AllUnits);

	for (AActor* Actor : AllUnits)
	{
		AStrategyUnit* Unit = Cast<AStrategyUnit>(Actor);
		if (Unit)
		{
			Unit->bHasMovedThisTurn = false;
			Unit->bHasAttacked = false;
			Unit->bIsTurnFinished = false;
			Unit->bHasMoved = false;
		}
	}

	if (CurrentTurnState == ETurnState::PlayerTurn)
	{
		CurrentTurnState = ETurnState::AITurn;
		UE_LOG(LogTemp, Warning, TEXT("=== TURNO DELL'INTELLIGENZA ARTIFICIALE ==="));
		// Invece di saltare, chiamiamo il primo nemico!
		GetWorldTimerManager().SetTimerForNextTick(this, &AStrategyGameMode::ProcessAITurn);
	}
	else
	{
		CurrentTurnState = ETurnState::PlayerTurn;
		UE_LOG(LogTemp, Warning, TEXT("=== NUOVO ROUND: TURNO DEL GIOCATORE ==="));
	}
}

// IL MOTORE DELL'IA
void AStrategyGameMode::ProcessAITurn()
{
	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyUnit::StaticClass(), AllUnits);

	AStrategyUnit* NextAIUnit = nullptr;

	// Cerchiamo il primo nemico che non ha ancora agito
	for (AActor* Actor : AllUnits)
	{
		AStrategyUnit* Unit = Cast<AStrategyUnit>(Actor);
		if (Unit && Unit->UnitTeam == ETeam::AI && !Unit->bIsTurnFinished)
		{
			NextAIUnit = Unit;
			break;
		}
	}

	if (NextAIUnit)
	{
		// Trovato! Gli diciamo di giocare il suo turno
		NextAIUnit->ExecuteAITurn();
	}
	else
	{
		// Tutti i nemici hanno finito, torna al Player
		EndTurn();
	}
}

void AStrategyGameMode::EvaluateTowers()
{
	TArray<AActor*> AllTowers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyTower::StaticClass(), AllTowers);

	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyUnit::StaticClass(), AllUnits);

	// Azzeriamo i contatori prima di ricalcolare
	PlayerTowerCount = 0;
	AITowerCount = 0;

	for (AActor* TowerActor : AllTowers)
	{
		AStrategyTower* Tower = Cast<AStrategyTower>(TowerActor);
		if (!Tower) continue;

		bool bPlayerInZone = false;
		bool bAIInZone = false;

		// 1. Controlliamo chi c'è nella Zona di Cattura (Raggio 2) 
		for (AActor* UnitActor : AllUnits)
		{
			AStrategyUnit* Unit = Cast<AStrategyUnit>(UnitActor);
			if (Unit && Unit->CurrentHealth > 0 && Unit->CurrentTile)
			{
				FIntPoint UnitPos = Unit->CurrentTile->GetGridPosition();

				// Calcolo della distanza (inclusa la diagonale) 
				int32 DistX = FMath::Abs(UnitPos.X - Tower->GridPosition.X);
				int32 DistY = FMath::Abs(UnitPos.Y - Tower->GridPosition.Y);

				if (DistX <= 2 && DistY <= 2)
				{
					if (Unit->UnitTeam == ETeam::Player) bPlayerInZone = true;
					else if (Unit->UnitTeam == ETeam::AI) bAIInZone = true;
				}
			}
		}

		// 2. La Macchina a Stati
		ETowerState OldState = Tower->CurrentState;

		if (bPlayerInZone && bAIInZone)
		{
			Tower->CurrentState = ETowerState::Contested;
		}
		else if (bPlayerInZone)
		{
			Tower->CurrentState = ETowerState::ControlledPlayer;
		}
		else if (bAIInZone)
		{
			Tower->CurrentState = ETowerState::ControlledAI;
		}

		// Se lo stato è cambiato, avvisiamo il Blueprint per cambiare colore!
		if (OldState != Tower->CurrentState)
		{
			Tower->OnStateChanged(Tower->CurrentState);
			UE_LOG(LogTemp, Warning, TEXT("Torre X:%d Y:%d ha cambiato stato!"), Tower->GridPosition.X, Tower->GridPosition.Y);
		}

		// --- NUOVO: AGGIORNIAMO IL CONTEGGIO TORRI ---
		if (Tower->CurrentState == ETowerState::ControlledPlayer)
		{
			PlayerTowerCount++;
		}
		else if (Tower->CurrentState == ETowerState::ControlledAI)
		{
			AITowerCount++;
		}
	}

	// --- NUOVO: VALUTAZIONE DOMINIO E VITTORIA ---

	UE_LOG(LogTemp, Warning, TEXT("Punteggio Torri -> Player: %d | AI: %d"), PlayerTowerCount, AITowerCount);

	// Controllo Dominio Player (>= 2 Torri)
	if (PlayerTowerCount >= 2)
	{
		PlayerDominanceTurns++;
		UE_LOG(LogTemp, Warning, TEXT("Il Player domina per il turno %d!"), PlayerDominanceTurns);

		if (PlayerDominanceTurns >= 2)
		{
			UE_LOG(LogTemp, Error, TEXT("VITTORIA! Il Player ha mantenuto 2 torri per 2 turni consecutivi!"));
			OnGameOver(ETeam::Player);
			return; // Usciamo, la partita è finita
		}
	}
	else
	{
		// Se perde il dominio, il contatore si azzera
		PlayerDominanceTurns = 0;
	}

	// Controllo Dominio AI (>= 2 Torri)
	if (AITowerCount >= 2)
	{
		AIDominanceTurns++;
		UE_LOG(LogTemp, Warning, TEXT("L'IA domina per il turno %d!"), AIDominanceTurns);

		if (AIDominanceTurns >= 2)
		{
			UE_LOG(LogTemp, Error, TEXT("SCONFITTA! L'IA ha mantenuto 2 torri per 2 turni consecutivi!"));
			OnGameOver(ETeam::AI);
			return; // Usciamo, la partita è finita
		}
	}
	else
	{
		// Se perde il dominio, il contatore si azzera
		AIDominanceTurns = 0;
	}
}
