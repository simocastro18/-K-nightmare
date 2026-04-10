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
	// =========================================================
	// 1. FASE DI SCHIERAMENTO (DEPLOYMENT)
	// =========================================================
	if (GM && GM->GetCurrentTurnState() == ETurnState::Deployment)
	{
		if (ClassToSpawn != nullptr && ClickedTile->Status == ETileStatus::EMPTY && ClickedTile->GetGridPosition().X <= 2)
		{
			// --- NUOVO: CONTROLLO DEL LIMITE UNITA' ---
			// Leggiamo il tipo di unità dal mouse prima di farla nascere fisicamente
			AStrategyUnit* DefaultUnit = Cast<AStrategyUnit>(ClassToSpawn->GetDefaultObject());
			if (DefaultUnit)
			{
				if (DefaultUnit->AttackType == EAttackType::MELEE && bBrawlerPlaced)
				{
					UE_LOG(LogTemp, Warning, TEXT("Hai gia' piazzato il Brawler!"));
					ClassToSpawn = nullptr;
					if (GameFieldRef) GameFieldRef->ClearHighlightedTiles();
					return;
				}
				if (DefaultUnit->AttackType == EAttackType::RANGED && bSniperPlaced)
				{
					UE_LOG(LogTemp, Warning, TEXT("Hai gia' piazzato lo Sniper!"));
					ClassToSpawn = nullptr;
					if (GameFieldRef) GameFieldRef->ClearHighlightedTiles();
					return;
				}
			}

			// --- SPAWN EFFETTIVO ---
			FVector SpawnLoc = ClickedTile->GetActorLocation() + FVector(0, 0, 100);
			FRotator SpawnRot(0.0f, -90.0f, 0.0f);

			AActor* NewUnit = GetWorld()->SpawnActor<AActor>(ClassToSpawn, SpawnLoc, SpawnRot);

			if (NewUnit)
			{
				NewUnit->SetActorScale3D(FVector(0.5f, 0.5f, 0.5f)); // La scala rimpicciolita!
				ClickedTile->SetTileStatus(0, ETileStatus::OCCUPIED);
				ClickedTile->SetUnitOnTile(NewUnit);

				AStrategyUnit* StratUnit = Cast<AStrategyUnit>(NewUnit);
				if (StratUnit)
				{
					StratUnit->GameFieldRef = GameFieldRef;
					StratUnit->InitializeUnit(TEXT("Player_Unit"), ETeam::Player, -90.0f, ClickedTile);

					// --- NUOVO: REGISTRA IL PIAZZAMENTO! ---
					if (StratUnit->AttackType == EAttackType::MELEE) bBrawlerPlaced = true;
					if (StratUnit->AttackType == EAttackType::RANGED) bSniperPlaced = true;
				}

				ClassToSpawn = nullptr;
				if (GameFieldRef) GameFieldRef->ClearHighlightedTiles();
			}
		}
		return;
	}
	// =========================================================
	// CONTROLLO OSTACOLI (Vale per Movimento e Attacco)
	// =========================================================
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

	CurrentSelectedTile = ClickedTile;

	// 2. Controllo GameMode e Turno (Se siamo qui, siamo in PlayerTurn o AITurn)
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
					if (!SelectedUnit->bHasMovedThisTurn)
					{
						UE_LOG(LogTemp, Warning, TEXT("DEBUG: Lancio Dijkstra per %s"), *SelectedUnit->UnitLogID);
						GameFieldRef->HighlightReachableTiles(SelectedUnit);
						GameFieldRef->HighlightAttackableTiles(SelectedUnit);
					}
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

				if (GM)
				{
					FString MoveMsg = FString::Printf(TEXT("MOVIMENTO: Il Giocatore sposta %s in X:%d Y:%d"), *SelectedUnit->UnitLogID, ClickedTile->GetGridPosition().X, ClickedTile->GetGridPosition().Y);
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