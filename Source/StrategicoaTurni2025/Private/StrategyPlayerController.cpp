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

	if (!ClickedTile->bIsWalkable || ClickedTile->Status == ETileStatus::OBSTACLE)
	{
		UE_LOG(LogTemp, Warning, TEXT("DEBUG: Click su Ostacolo/Acqua! Ignoro il click."));
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

	if (CurrentSelectedTile == ClickedTile)
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
	else
	{
		if (IsValid(CurrentSelectedTile))
		{
			CurrentSelectedTile->OnSelectionChanged(false);
		}
		ClickedTile->OnSelectionChanged(true);
		CurrentSelectedTile = ClickedTile;
	}

	// 2. Controllo GameMode e Turno
	AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
	if (GM && GM->GetCurrentTurnState() != ETurnState::PlayerTurn)
	{
		UE_LOG(LogTemp, Error, TEXT("DEBUG: Click bloccato perche' non e' il turno del Player!"));
		return;
	}

	// =========================================================
	// 2.5. SE CLICCO UNA CELLA ROSSA (ATTACCO) 
	// =========================================================
	if (IsValid(SelectedUnit) && IsValid(GameFieldRef) && GameFieldRef->AttackableTiles.Contains(ClickedTile))
	{
		AStrategyUnit* TargetUnit = Cast<AStrategyUnit>(ClickedTile->UnitOnTile);
		if (IsValid(TargetUnit))
		{
			int32 Damage = SelectedUnit->CalculateDamageToDeal();
			TargetUnit->ReceiveDamage(Damage);

			UE_LOG(LogTemp, Error, TEXT("ATTACCO! %s infligge %d danni a %s!"), *SelectedUnit->UnitLogID, Damage, *TargetUnit->UnitLogID);

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

	// =========================================================
	// 3. SE CLICCO SU UN'UNITA'
	// =========================================================
	if (IsValid(ClickedTile->UnitOnTile))
	{
		AStrategyUnit* ClickedUnit = Cast<AStrategyUnit>(ClickedTile->UnitOnTile);
		if (ClickedUnit)
		{
			UE_LOG(LogTemp, Warning, TEXT("DEBUG: Cliccata unita %s | Team: %d"), *ClickedUnit->UnitLogID, (int32)ClickedUnit->UnitTeam);

			if (ClickedUnit->UnitTeam != ETeam::Player)
			{
				UE_LOG(LogTemp, Error, TEXT("DEBUG: Click su nemico! Ignoro selezione."));
				return;
			}

			if (ClickedUnit->bHasMovedThisTurn && ClickedUnit->bHasAttacked)
			{
				UE_LOG(LogTemp, Error, TEXT("DEBUG: Unita' stanca!"));
				return;
			}

			// IL CODICE CORRETTO VA QUI DENTRO!
			if (SelectedUnit == ClickedUnit)
			{
				SelectedUnit = nullptr;
				if (IsValid(GameFieldRef))
				{
					GameFieldRef->ClearHighlightedTiles();
					GameFieldRef->ClearAttackableTiles();
				}
			}
			else
			{
				if (IsValid(GameFieldRef))
				{
					GameFieldRef->ClearHighlightedTiles();
					GameFieldRef->ClearAttackableTiles();
				}
				SelectedUnit = ClickedUnit;

				if (IsValid(GameFieldRef))
				{
					// Se deve ancora muoversi: Dijkstra (Azzurro) E Bersagli (Rosso)
					if (!SelectedUnit->bHasMovedThisTurn)
					{
						UE_LOG(LogTemp, Warning, TEXT("DEBUG: Lancio Dijkstra per %s"), *SelectedUnit->UnitLogID);
						GameFieldRef->HighlightReachableTiles(SelectedUnit);

						// AGGIUNTA FONDAMENTALE: Mostra subito chi possiamo colpire senza muoverci!
						GameFieldRef->HighlightAttackableTiles(SelectedUnit);
					}
					// Se ha già mosso ma deve attaccare: Solo Bersagli (Rosso)
					else if (!SelectedUnit->bHasAttacked)
					{
						UE_LOG(LogTemp, Warning, TEXT("DEBUG: %s ha gia' mosso, cerco bersagli!"), *SelectedUnit->UnitLogID);
						GameFieldRef->HighlightAttackableTiles(SelectedUnit);
					}
				}
			}
		}
	}
	// =========================================================
	// 4. SE CLICCO SUL VUOTO (MOVIMENTO)
	// =========================================================
	else
	{
		if (IsValid(SelectedUnit))
		{
			if (IsValid(GameFieldRef) && GameFieldRef->HighlightedTiles.Contains(ClickedTile))
			{
				// Libero vecchia cella
				for (auto& Pair : GameFieldRef->TileMap)
				{
					if (Pair.Value->UnitOnTile == SelectedUnit)
					{
						Pair.Value->UnitOnTile = nullptr;
						break;
					}
				}

				// Occupo nuova cella
				ClickedTile->UnitOnTile = SelectedUnit;
				SelectedUnit->CurrentTile = ClickedTile;

				// Lancio Movimento!
				TArray<FVector> Path = GameFieldRef->GetPathToTile(ClickedTile);
				SelectedUnit->StartMoving(Path);

				SelectedUnit->bHasMovedThisTurn = true;
				GameFieldRef->ClearHighlightedTiles();
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("DEBUG: Click fuori dal percorso. Deseleziono."));
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