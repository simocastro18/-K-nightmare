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
	/*else
	{
		if (IsValid(CurrentSelectedTile))
		{
			CurrentSelectedTile->OnSelectionChanged(false);
		}
		ClickedTile->OnSelectionChanged(true);
		CurrentSelectedTile = ClickedTile;
	}
	*/

	CurrentSelectedTile = ClickedTile;

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
				// --- AGGIUNGI SOLO QUESTA RIGA QUI ---
				if (IsValid(GameFieldRef))
				{
					GameFieldRef->ClearHighlightedTiles(); // Spegne l'azzurro
					GameFieldRef->ClearAttackableTiles();  // <--- QUESTA SPEGNE IL ROSSO RESIDUO!
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