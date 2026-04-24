#include "StrategyGameMode.h"
#include "GameField.h"
#include "Kismet/GameplayStatics.h"
#include "StrategyTower.h"
#include "StrategyUnit.h"
#include "Tile.h"
#include "Blueprint/UserWidget.h"

void AStrategyGameMode::BeginPlay()
{
	Super::BeginPlay();

	AActor* FoundField = UGameplayStatics::GetActorOfClass(GetWorld(), AGameField::StaticClass());
	if (FoundField)
	{
		MapGenerator = Cast<AGameField>(FoundField);
	}
}

void AStrategyGameMode::HandleGameOver(ETeam Winner)
{
	// Activate kill switch to prevent further actions
	bIsGameOver = true;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	// Select the appropriate widget based on the winner
	TSubclassOf<UUserWidget> WidgetToCreate = (Winner == ETeam::Player) ? WinWidgetClass : LoseWidgetClass;

	if (WidgetToCreate)
	{
		// Create and display the widget
		UUserWidget* GameOverWidget = CreateWidget<UUserWidget>(GetWorld(), WidgetToCreate);
		if (GameOverWidget)
		{
			GameOverWidget->AddToViewport();

			// Lock game input and release the mouse cursor to the UI
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
	// Retrieve the current level name and reload it
	FString CurrentLevelName = GetWorld()->GetMapName();
	CurrentLevelName.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);

	UGameplayStatics::OpenLevel(GetWorld(), FName(*CurrentLevelName));
}

void AStrategyGameMode::StartGameWithConfig(FGameConfig Config)
{
	ActiveAIAlgorithm = Config.SelectedAIAlgorithm;

	if (MapGenerator)
	{
		MapGenerator->NoiseScale = Config.NoiseScale;
		MapGenerator->GridSizeX = Config.GridSizeX;
		MapGenerator->GridSizeY = Config.GridSizeY;

		MapGenerator->GenerateGridData();
		MapGenerator->SpawnInitialEntities();
	}

	CurrentTurnState = ETurnState::Deployment;

	// Initiate the coin flip to decide who deploys first
	StartCoinFlipAndDeployment();

	TriggerUIUpdate();
}

void AStrategyGameMode::StartCoinFlipAndDeployment()
{
	// 50/50 chance to decide the first player
	if (FMath::RandBool())
	{
		FirstTurnWinner = ETeam::Player;
		bPlayerDeploysNext = true;
		AddGameLog(TEXT("Coin: HP places first!"));
	}
	else
	{
		FirstTurnWinner = ETeam::AI;
		bPlayerDeploysNext = false;
		AddGameLog(TEXT("Coin: AI places first!"));
	}

	AdvanceDeployment();
}

void AStrategyGameMode::AdvanceDeployment()
{
	// Check if both teams have placed all their units
	if (PlayerUnitsPlaced >= 2 && AIUnitsPlaced >= 2)
	{
		AddGameLog(TEXT("Deployment completed!"));

		// Restore full game and UI input to the player
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC) { PC->SetInputMode(FInputModeGameAndUI()); }

		// The winner of the coin flip takes the first turn
		if (FirstTurnWinner == ETeam::Player)
		{
			CurrentTurnState = ETurnState::PlayerTurn;
			AddGameLog(TEXT("=== HP starts ==="));
		}
		else
		{
			CurrentTurnState = ETurnState::AITurn;
			AddGameLog(TEXT("=== AI starts ==="));
			GetWorldTimerManager().SetTimerForNextTick(this, &AStrategyGameMode::ProcessAITurn);
		}

		TriggerUIUpdate();
		return;
	}

	if (bPlayerDeploysNext)
	{
		// Wait for the player to use the deployment UI
	}
	else
	{
		// Trigger AI deployment
		if (MapGenerator)
		{
			MapGenerator->SpawnSingleAIUnit(AIUnitsPlaced);
			AIUnitsPlaced++;

			// Pass the turn back to the player and re-evaluate the deployment state
			bPlayerDeploysNext = true;
			AdvanceDeployment();
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

	if (bIsGameOver) return;

	CurrentTurnNumber++;

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
		// Trigger the first available enemy unit
		GetWorldTimerManager().SetTimerForNextTick(this, &AStrategyGameMode::ProcessAITurn);
	}
	else
	{
		CurrentTurnState = ETurnState::PlayerTurn;
	}

	// 4. BROADCAST: Tell the UI that the turn state or number has changed!
	TriggerUIUpdate();
}

void AStrategyGameMode::ProcessAITurn()
{
	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyUnit::StaticClass(), AllUnits);

	AStrategyUnit* NextAIUnit = nullptr;

	// Find the first AI unit that hasn't acted yet
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
		// Execute the turn for the found unit
		NextAIUnit->ExecuteAITurn();
	}
	else
	{
		// All AI units have finished, end the AI turn
		EndTurn();
	}
}

void AStrategyGameMode::EvaluateTowers()
{
	// 1. TROVA TUTTO UNA SOLA VOLTA
	TArray<AActor*> AllTowers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyTower::StaticClass(), AllTowers);

	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyUnit::StaticClass(), AllUnits);

	PlayerTowerCount = 0;
	AITowerCount = 0;

	// 2. CALCOLA IL NUOVO STATO DELLE TORRI (Chi è dentro la zona?)
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

		// Aggiorna lo stato attuale
		if (bPlayerInZone && bAIInZone) Tower->CurrentState = ETowerState::Contested;
		else if (bPlayerInZone) Tower->CurrentState = ETowerState::ControlledPlayer;
		else if (bAIInZone) Tower->CurrentState = ETowerState::ControlledAI;

		if (OldState != Tower->CurrentState)
		{
			Tower->UpdateTowerVisuals(Tower->CurrentState);
		}

		if (Tower->CurrentState == ETowerState::ControlledPlayer) PlayerTowerCount++;
		else if (Tower->CurrentState == ETowerState::ControlledAI) AITowerCount++;
	}

	// 3. PREPARA LE VARIABILI PER LA UI (Ora che gli stati sono aggiornati!)
	if (AllTowers.Num() >= 3)
	{
		// FORMULA CORRETTA: Ordiniamo in base all'asse orizzontale (X)!
		AllTowers.Sort([](const AActor& A, const AActor& B) {
			const AStrategyTower* TowerA = Cast<AStrategyTower>(&A);
			const AStrategyTower* TowerB = Cast<AStrategyTower>(&B);

			if (TowerA && TowerB)
			{
				// CAMBIATO Y in X QUI SOTTO:
				return TowerA->GridPosition.X < TowerB->GridPosition.X; 
			}
			return false;
		});

		// Assegniamo i valori ordinati all'HUD (Sinistra=0, Centro=1, Destra=2)
		if (AStrategyTower* WestT = Cast<AStrategyTower>(AllTowers[0])) StateTowerWest = WestT->CurrentState;
		if (AStrategyTower* MidT = Cast<AStrategyTower>(AllTowers[1])) StateTowerMid = MidT->CurrentState;
		if (AStrategyTower* EastT = Cast<AStrategyTower>(AllTowers[2])) StateTowerEast = EastT->CurrentState;
	}

	// 4. CONDIZIONI DI VITTORIA
	if (PlayerTowerCount < 2) PlayerDominanceTurns = 0;
	if (AITowerCount < 2) AIDominanceTurns = 0;

	if (PlayerTowerCount >= 2)
	{
		PlayerDominanceTurns++;
		if (PlayerDominanceTurns >= 4)
		{
			UE_LOG(LogTemp, Log, TEXT("VICTORY! Player defended 2 towers for 2 full rounds!"));
			AddGameLog(TEXT("VICTORY: You defended the towers!"));
			this->HandleGameOver(ETeam::Player);
			return; // Importante fermarsi qui se la partita è finita
		}
	}
	else if (AITowerCount >= 2)
	{
		AIDominanceTurns++;
		if (AIDominanceTurns >= 4)
		{
			UE_LOG(LogTemp, Log, TEXT("DEFEAT! AI defended 2 towers for 2 full rounds!"));
			AddGameLog(TEXT("DEFEAT: AI defended the towers!"));
			this->HandleGameOver(ETeam::AI);
			return; // Importante fermarsi qui se la partita è finita
		}
	}

	// 5. INVIO DEL SEGNALE ALL'INTERFACCIA
	TriggerUIUpdate();
}

FString AStrategyGameMode::GetCellCoordinateName(int32 X, int32 Y)
{
	// Convert the Y axis (0-24) to the corresponding letter (A-Y)
	// ASCII 'A' is 65. 65 + 0 = 'A', 65 + 1 = 'B', etc.
	char Letter = 'A' + FMath::Clamp(Y, 0, 24);

	// Format the final string (e.g., "C4")
	return FString::Printf(TEXT("%c%d"), Letter, X);
}

void AStrategyGameMode::AddGameLog(const FString& Message)
{
	// Broadcast the message to the UI event listener
	OnGameLogAdded.Broadcast(Message);
}

FString AStrategyGameMode::GetGridLetter(int32 Index)
{
	char Letter = 'A' + FMath::Clamp(Index, 0, 24);
	return FString::Printf(TEXT("%c"), Letter);
}