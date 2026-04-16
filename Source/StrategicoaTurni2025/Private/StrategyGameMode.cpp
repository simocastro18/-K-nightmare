#include "StrategyGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "StrategyTower.h"
#include "StrategyUnit.h"
#include "Tile.h"
#include "Blueprint/UserWidget.h" // Serve per gestire i Widget

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

void AStrategyGameMode::HandleGameOver(ETeam Winner)
{
	bIsGameOver = true; // ATTIVAZIONE KILL SWITCH
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	// 1. Scegliamo quale widget creare
	TSubclassOf<UUserWidget> WidgetToCreate = (Winner == ETeam::Player) ? WinWidgetClass : LoseWidgetClass;

	if (WidgetToCreate)
	{
		// 2. Creiamo il widget
		UUserWidget* GameOverWidget = CreateWidget<UUserWidget>(GetWorld(), WidgetToCreate);
		if (GameOverWidget)
		{
			GameOverWidget->AddToViewport();

			// 3. Impostiamo l'input mode (Blocca il gioco, sblocca il mouse)
			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(GameOverWidget->TakeWidget());
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = true;
			UGameplayStatics::SetGamePaused(GetWorld(), true);
		}
	}
}

void AStrategyGameMode::RestartGame()
{
	// Prende il nome del livello attuale e lo ricarica
	FString CurrentLevelName = GetWorld()->GetMapName();
	CurrentLevelName.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);

	UGameplayStatics::OpenLevel(GetWorld(), FName(*CurrentLevelName));
}

void AStrategyGameMode::StartGameWithConfig(FGameConfig Config)
{
	if (MapGenerator)
	{
		MapGenerator->NoiseScale = Config.NoiseScale;
		MapGenerator->GridSizeX = Config.GridSizeX;
		MapGenerator->GridSizeY = Config.GridSizeY;

		MapGenerator->GenerateGridData();
		MapGenerator->SpawnInitialEntities();
	}

	// IL FIX FONDAMENTALE: Niente moneta qui! Mettiamo il gioco in pausa per lo schieramento
	CurrentTurnState = ETurnState::Deployment;
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

	if (bIsGameOver) return;

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

		// FIX: Il turno aumenta SOLO quando ricomincia il round completo!
		CurrentTurnNumber++;
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

	PlayerTowerCount = 0;
	AITowerCount = 0;

	for (AActor* TowerActor : AllTowers)
	{
		AStrategyTower* Tower = Cast<AStrategyTower>(TowerActor);
		if (!Tower) continue;

		bool bPlayerInZone = false;
		bool bAIInZone = false;

		for (AActor* UnitActor : AllUnits)
		{
			AStrategyUnit* Unit = Cast<AStrategyUnit>(UnitActor);
			if (Unit && Unit->CurrentHealth > 0 && Unit->CurrentTile)
			{
				FIntPoint UnitPos = Unit->CurrentTile->GetGridPosition();
				int32 DistX = FMath::Abs(UnitPos.X - Tower->GridPosition.X);
				int32 DistY = FMath::Abs(UnitPos.Y - Tower->GridPosition.Y);

				if (DistX <= 2 && DistY <= 2)
				{
					if (Unit->UnitTeam == ETeam::Player) bPlayerInZone = true;
					else if (Unit->UnitTeam == ETeam::AI) bAIInZone = true;
				}
			}
		}

		ETowerState OldState = Tower->CurrentState;

		if (bPlayerInZone && bAIInZone) Tower->CurrentState = ETowerState::Contested;
		else if (bPlayerInZone) Tower->CurrentState = ETowerState::ControlledPlayer;
		else if (bAIInZone) Tower->CurrentState = ETowerState::ControlledAI;

		if (OldState != Tower->CurrentState)
		{
			Tower->UpdateTowerVisuals(Tower->CurrentState);
			UE_LOG(LogTemp, Warning, TEXT("Torre X:%d Y:%d ha cambiato stato!"), Tower->GridPosition.X, Tower->GridPosition.Y);
		}

		if (Tower->CurrentState == ETowerState::ControlledPlayer) PlayerTowerCount++;
		else if (Tower->CurrentState == ETowerState::ControlledAI) AITowerCount++;
	}

	UE_LOG(LogTemp, Warning, TEXT("Punteggio Torri -> Player: %d | AI: %d"), PlayerTowerCount, AITowerCount);

	if (CurrentTurnState == ETurnState::PlayerTurn)
	{
		if (PlayerTowerCount >= 2)
		{
			PlayerDominanceTurns++;
			UE_LOG(LogTemp, Warning, TEXT("Il Player domina per il turno %d!"), PlayerDominanceTurns);
			if (PlayerDominanceTurns >= 2)
			{
				UE_LOG(LogTemp, Error, TEXT("VITTORIA! Il Player ha mantenuto 2 torri per 2 turni consecutivi!"));

				// NUOVO LOG
				AddGameLog(TEXT("VITTORIA: Hai dominato le torri per 2 turni!"));

				this->HandleGameOver(ETeam::Player);
				return;
			}
		}
		else
		{
			PlayerDominanceTurns = 0;
		}
	}
	else if (CurrentTurnState == ETurnState::AITurn)
	{
		if (AITowerCount >= 2)
		{
			AIDominanceTurns++;
			UE_LOG(LogTemp, Warning, TEXT("L'IA domina per il turno %d!"), AIDominanceTurns);
			if (AIDominanceTurns >= 2)
			{
				UE_LOG(LogTemp, Error, TEXT("SCONFITTA! L'IA ha mantenuto 2 torri per 2 turni consecutivi!"));

				// NUOVO LOG
				AddGameLog(TEXT("SCONFITTA: L'IA ha dominato le torri per 2 turni!"));

				this->HandleGameOver(ETeam::AI);
				return;
			}
		}
		else
		{
			AIDominanceTurns = 0;
		}
	}
}

void AStrategyGameMode::StartFirstTurn()
{
	// 1. RESET DI SICUREZZA: Assicuriamoci che tutte le unità siano fresche
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

	// 2. SBLOCCO DEL MOUSE: Togliamo il focus dal bottone e ridiamolo al gioco
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		FInputModeGameAndUI InputMode;
		PC->SetInputMode(InputMode);
	}

	// 3. LANCIO DELLA MONETA (Random 50/50)
	if (FMath::RandBool())
	{
		CurrentTurnState = ETurnState::PlayerTurn;

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Green, TEXT("LANCIO MONETA: Testa! Inizia il GIOCATORE!"));
		UE_LOG(LogTemp, Warning, TEXT("=== LANCIO MONETA: Vince il GIOCATORE! ==="));

		AddGameLog(TEXT("Lancio Moneta: Inizia il Giocatore!"));
	}
	else
	{
		CurrentTurnState = ETurnState::AITurn;

		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, TEXT("LANCIO MONETA: Croce! Inizia l'IA!"));
		UE_LOG(LogTemp, Warning, TEXT("=== LANCIO MONETA: Vince l'IA! ==="));

		AddGameLog(TEXT("Lancio Moneta: Inizia l'Intelligenza Artificiale!"));

		GetWorldTimerManager().SetTimerForNextTick(this, &AStrategyGameMode::ProcessAITurn);
	}
}

FString AStrategyGameMode::GetCellCoordinateName(int32 X, int32 Y)
{
	// Convertiamo la Y (0-24) nella lettera corrispondente (A-Y)
	// La 'A' in codice ASCII è 65. Quindi 65 + 0 = 'A', 65 + 1 = 'B', ecc.
	char Letter = 'A' + FMath::Clamp(Y, 0, 24);

	// Formattiamo la stringa finale (es. "C4")
	return FString::Printf(TEXT("%c%d"), Letter, X);
}

void AStrategyGameMode::AddGameLog(const FString& Message)
{
	// Suona il megafono passando il messaggio!
	OnGameLogAdded.Broadcast(Message);
}

FString AStrategyGameMode::GetGridLetter(int32 Index)
{
	char Letter = 'A' + FMath::Clamp(Index, 0, 24);
	return FString::Printf(TEXT("%c"), Letter);
}