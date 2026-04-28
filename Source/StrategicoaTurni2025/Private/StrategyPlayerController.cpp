#include "StrategyPlayerController.h"
#include "Tile.h"
#include "StrategyUnit.h" 
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "GameField.h"
#include "StrategyGameMode.h"

void AStrategyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	CurrentSelectedTile = nullptr;
	SelectedUnit = nullptr;

	AActor* FoundField = UGameplayStatics::GetActorOfClass(GetWorld(), AGameField::StaticClass());
	if (FoundField)
	{
		GameFieldRef = Cast<AGameField>(FoundField);
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AStrategyPlayerController::HandleTileClick(ATile* ClickedTile)
{
	if (!IsValid(ClickedTile)) return;

	AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());

	// 1. Deployment phase handling
	if (GM && GM->GetCurrentTurnState() == ETurnState::Deployment)
	{
		// Prevent player from interacting during the AI deployment turn
		if (!GM->bPlayerDeploysNext)
		{
			return;
		}

		// Ensure a class is selected, the tile is empty, and within the player's valid deployment zone (Rows 0,1,2)
		if (ClassToSpawn != nullptr && ClickedTile->Status == ETileStatus::EMPTY && ClickedTile->GetGridPosition().X <= 2)
		{
			// Check if the specific unit type has already been deployed to prevent duplicates
			AStrategyUnit* DefaultUnit = Cast<AStrategyUnit>(ClassToSpawn->GetDefaultObject());
			if (DefaultUnit)
			{
				if (DefaultUnit->AttackType == EAttackType::MELEE && bBrawlerPlaced) return;
				if (DefaultUnit->AttackType == EAttackType::RANGED && bSniperPlaced) return;
			}

			FVector SpawnLoc = ClickedTile->GetActorLocation() + FVector(0, 0, 100);
			FRotator SpawnRot(0.0f, -90.0f, 0.0f);
			AActor* NewUnit = GetWorld()->SpawnActor<AActor>(ClassToSpawn, SpawnLoc, SpawnRot);

			if (NewUnit)
			{
				NewUnit->SetActorScale3D(FVector(0.5f, 0.5f, 0.5f));
				ClickedTile->SetTileStatus(0, ETileStatus::OCCUPIED);
				ClickedTile->SetUnitOnTile(NewUnit);

				AStrategyUnit* StratUnit = Cast<AStrategyUnit>(NewUnit);
				if (StratUnit)
				{
					StratUnit->GameFieldRef = GameFieldRef;
					StratUnit->InitializeUnit(TEXT("Player_Unit"), ETeam::Player, -90.0f, ClickedTile);

					if (StratUnit->AttackType == EAttackType::MELEE) bBrawlerPlaced = true;
					if (StratUnit->AttackType == EAttackType::RANGED) bSniperPlaced = true;

					// Format deployment message for the Combat Log
					FString UnitInitial = (StratUnit->AttackType == EAttackType::RANGED) ? TEXT("S") : TEXT("B");
					char ColLetter = 'A' + ClickedTile->GetGridPosition().Y;
					int32 RowNum = ClickedTile->GetGridPosition().X;
					GM->AddGameLog(FString::Printf(TEXT("HP: Placed %s in %c%d"), *UnitInitial, ColLetter, RowNum));
				}

				// Reset deployment variables and visuals
				ClassToSpawn = nullptr;
				if (GameFieldRef) GameFieldRef->ClearHighlightedTiles();

				// Advance the deployment sequence and yield turn to the AI
				GM->PlayerUnitsPlaced++;
				GM->bPlayerDeploysNext = false;
				GM->AdvanceDeployment();
			}
		}
		return;
	}

	// Core gameplay validation: Prevent interaction with obstacles and water tiles
	if (!ClickedTile->bIsWalkable || ClickedTile->Status == ETileStatus::OBSTACLE)
	{
		if (IsValid(CurrentSelectedTile))
		{
			CurrentSelectedTile->OnSelectionChanged(false);
			CurrentSelectedTile = nullptr;
		}
		SelectedUnit = nullptr;
		if (IsValid(GameFieldRef))
		{
			GameFieldRef->ClearHighlightedTiles();
			GameFieldRef->ClearAttackableTiles();
		}
		return;
	}

	// Double click handling: If the player clicks the already selected tile
	if (CurrentSelectedTile == ClickedTile)
	{
		bool bIsTryingToWait = false;

		// If the selected unit is on this tile, check if they are intentionally skipping their attack phase (Wait command)
		if (SelectedUnit && SelectedUnit == ClickedTile->UnitOnTile)
		{
			if (SelectedUnit->bHasMovedThisTurn && !SelectedUnit->bHasAttacked)
			{
				bIsTryingToWait = true;
			}
		}

		// Proceed with standard deselection if it's not a Wait command
		if (!bIsTryingToWait)
		{
			ClickedTile->OnSelectionChanged(false);
			CurrentSelectedTile = nullptr;
			SelectedUnit = nullptr;

			if (IsValid(GameFieldRef))
			{
				GameFieldRef->ClearHighlightedTiles();
				GameFieldRef->ClearAttackableTiles();
			}
			return;
		}
	}

	CurrentSelectedTile = ClickedTile;

	// Turn enforcement: Block input if it's not the human player's active turn
	if (GM && GM->GetCurrentTurnState() != ETurnState::PlayerTurn)
	{
		return;
	}

	// 2. Attack execution (Clicking a red-highlighted enemy tile)
	if (IsValid(SelectedUnit) && IsValid(GameFieldRef) && GameFieldRef->AttackableTiles.Contains(ClickedTile))
	{
		AStrategyUnit* TargetUnit = Cast<AStrategyUnit>(ClickedTile->UnitOnTile);
		if (IsValid(TargetUnit))
		{
			SelectedUnit->AttackTarget(TargetUnit);

			SelectedUnit->bHasAttacked = true;
			SelectedUnit->bIsTurnFinished = true;
			SelectedUnit->bHasMovedThisTurn = true;

			GameFieldRef->ClearAttackableTiles();
			SelectedUnit = nullptr;
			CurrentSelectedTile = nullptr;

			if (GM) GM->CheckRemainingMoves();

			return;
		}
	}

	// 3. Unit selection
	if (IsValid(ClickedTile->UnitOnTile))
	{
		AStrategyUnit* ClickedUnit = Cast<AStrategyUnit>(ClickedTile->UnitOnTile);
		if (ClickedUnit)
		{
			// Prevent selecting enemy units
			if (ClickedUnit->UnitTeam != ETeam::Player)
			{
				return;
			}

			// Prevent re-selecting allied units that have already completed their turn
			if (ClickedUnit && ClickedUnit->UnitTeam == ETeam::Player)
			{
				if (ClickedUnit->bIsTurnFinished)
				{
					return;
				}
			}

			// Block interaction if the unit has exhausted all actions
			if (ClickedUnit->bHasMovedThisTurn && ClickedUnit->bHasAttacked)
			{
				return;
			}

			if (SelectedUnit == ClickedUnit)
			{
				// Wait logic: Unit skips attack phase and ends its turn
				if (SelectedUnit->bHasMovedThisTurn && !SelectedUnit->bHasAttacked)
				{
					SelectedUnit->bIsTurnFinished = true;

					if (GM)
					{
						FString UnitInitial = (SelectedUnit->AttackType == EAttackType::RANGED) ? TEXT("S") : TEXT("B");
						GM->AddGameLog(FString::Printf(TEXT("HP: %s Wait (Attack Skipped)"), *UnitInitial));

						// Verify if this was the last active unit to end the phase
						GM->CheckRemainingMoves();
					}
				}

				// Deselect unit and clear visuals
				SelectedUnit = nullptr;
				if (IsValid(GameFieldRef))
				{
					GameFieldRef->ClearHighlightedTiles();
					GameFieldRef->ClearAttackableTiles();
				}
			}
			else
			{
				// Select a new unit and display appropriate logical grids (Pathfinding / Combat)
				if (IsValid(GameFieldRef))
				{
					GameFieldRef->ClearHighlightedTiles();
					GameFieldRef->ClearAttackableTiles();
				}
				SelectedUnit = ClickedUnit;

				if (IsValid(GameFieldRef))
				{
					if (!SelectedUnit->bHasMovedThisTurn)
					{
						GameFieldRef->HighlightReachableTiles(SelectedUnit);
						GameFieldRef->HighlightAttackableTiles(SelectedUnit);
					}
					else if (!SelectedUnit->bHasAttacked)
					{
						GameFieldRef->HighlightAttackableTiles(SelectedUnit);
					}
				}
			}
		}
	}
	// 4. Movement execution (Clicking a blue-highlighted empty tile)
	else
	{
		if (IsValid(SelectedUnit))
		{
			if (IsValid(GameFieldRef) && GameFieldRef->HighlightedTiles.Contains(ClickedTile))
			{
				// Cache starting coordinates for the combat log before executing movement
				char StartLetter = 'A' + SelectedUnit->CurrentTile->GetGridPosition().Y;
				int32 StartNumber = SelectedUnit->CurrentTile->GetGridPosition().X;

				// Release the origin tile
				for (auto& Pair : GameFieldRef->TileMap)
				{
					if (Pair.Value->UnitOnTile == SelectedUnit)
					{
						Pair.Value->UnitOnTile = nullptr;
						break;
					}
				}

				// Occupy the destination tile
				ClickedTile->UnitOnTile = SelectedUnit;
				SelectedUnit->CurrentTile = ClickedTile;

				// Retrieve mathematical path and initiate physical movement
				TArray<FVector> Path = GameFieldRef->GetPathToTile(ClickedTile);
				SelectedUnit->StartMoving(Path);
				SelectedUnit->bHasMovedThisTurn = true;

				if (GM)
				{
					char EndLetter = 'A' + ClickedTile->GetGridPosition().Y;
					FString UnitInitial = (SelectedUnit->AttackType == EAttackType::RANGED) ? TEXT("S") : TEXT("B");

					FString MoveMsg = FString::Printf(TEXT("HP: %s   %c%d -> %c%d"), *UnitInitial, StartLetter, StartNumber, EndLetter, ClickedTile->GetGridPosition().X);
					GM->AddGameLog(MoveMsg);
				}

				if (IsValid(GameFieldRef))
				{
					GameFieldRef->ClearHighlightedTiles();
					GameFieldRef->ClearAttackableTiles();
				}
			}
			else
			{
				// Invalid movement click clears the current selection
				SelectedUnit = nullptr;
				if (IsValid(GameFieldRef))
				{
					GameFieldRef->ClearHighlightedTiles();
					GameFieldRef->ClearAttackableTiles();
				}
			}
		}
	}
}

void AStrategyPlayerController::ExecuteClick()
{
	FHitResult HitResult;

	// Perform a raycast from the screen space to the 3D world space
	if (GetHitResultUnderCursor(ECC_Visibility, true, HitResult))
	{
		AActor* HitActor = HitResult.GetActor();
		if (!HitActor) return;

		ATile* HitTile = Cast<ATile>(HitActor);
		if (HitTile)
		{
			HandleTileClick(HitTile);
			return;
		}

		AStrategyUnit* HitUnit = Cast<AStrategyUnit>(HitActor);
		if (HitUnit && HitUnit->CurrentTile)
		{
			HandleTileClick(HitUnit->CurrentTile);
			return;
		}
	}
}

void AStrategyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		if (ClickAction)
		{
			EnhancedInputComponent->BindAction(ClickAction, ETriggerEvent::Started, this, &AStrategyPlayerController::ExecuteClick);
		}
	}
}