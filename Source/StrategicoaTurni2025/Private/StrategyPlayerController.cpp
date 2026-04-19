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
		// CONTROLLO TURNO SCHIERAMENTO
		if (!GM->bPlayerDeploysNext)
		{
			UE_LOG(LogTemp, Warning, TEXT("Attendi! E' il turno dell'IA di piazzare."));
			return;
		}

		if (ClassToSpawn != nullptr && ClickedTile->Status == ETileStatus::EMPTY && ClickedTile->GetGridPosition().X <= 2)
		{
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

					// --- NUOVO: LOG DI SCHIERAMENTO (Ora è al sicuro dai Crash!) ---
					FString UnitInitial = (StratUnit->AttackType == EAttackType::RANGED) ? TEXT("S") : TEXT("B");
					char ColLetter = 'A' + ClickedTile->GetGridPosition().Y;
					int32 RowNum = ClickedTile->GetGridPosition().X;
					GM->AddGameLog(FString::Printf(TEXT("HP: Piazzato %s in %c%d"), *UnitInitial, ColLetter, RowNum));
				}

				ClassToSpawn = nullptr;
				if (GameFieldRef) GameFieldRef->ClearHighlightedTiles();

				// AVANZA LO SCHIERAMENTO! Passa la palla all'IA
				GM->PlayerUnitsPlaced++;
				GM->bPlayerDeploysNext = false;
				GM->AdvanceDeployment();
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

	// =========================================================
	// FIX: GESTIONE DOPPIO CLICK SULLA STESSA CELLA
	// =========================================================
	if (CurrentSelectedTile == ClickedTile)
	{
		// Controlliamo se stiamo cliccando la nostra unita' attiva per farla riposare (Wait)
		bool bIsTryingToWait = false;
		if (SelectedUnit && SelectedUnit == ClickedTile->UnitOnTile)
		{
			if (SelectedUnit->bHasMovedThisTurn && !SelectedUnit->bHasAttacked)
			{
				bIsTryingToWait = true;
			}
		}

		// Se NON stiamo facendo Wait, allora procedi con la normale deselezione
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

			if (ClickedUnit && ClickedUnit->UnitTeam == ETeam::Player)
			{
				// --- NUOVO: IL LUCCHETTO ---
				// Se clicchi un'unità che ha già finito il turno (ha attaccato o ha fatto "Wait"), ignora il click!
				if (ClickedUnit->bIsTurnFinished)
				{
					UE_LOG(LogTemp, Warning, TEXT("Azione negata: %s ha gia' concluso il suo turno!"), *ClickedUnit->UnitLogID);
					return; // Ferma tutto, non la seleziona!
				}
			}

			if (ClickedUnit->bHasMovedThisTurn && ClickedUnit->bHasAttacked)
			{
				UE_LOG(LogTemp, Error, TEXT("DEBUG: Unita' stanca!"));
				return;
			}

			if (SelectedUnit == ClickedUnit)
			{
				// --- LOGICA: SALTA L'ATTACCO (WAIT) ---
				// Se l'unità si è già mossa ma non ha attaccato, ri-cliccarla termina il suo turno!
				if (SelectedUnit->bHasMovedThisTurn && !SelectedUnit->bHasAttacked)
				{
					UE_LOG(LogTemp, Warning, TEXT("L'unita' %s rinuncia all'attacco e si mette in attesa."), *SelectedUnit->UnitLogID);
					SelectedUnit->bIsTurnFinished = true;

					// Aggiorniamo il Combat Log per far sapere che ha saltato l'azione
					if (GM)
					{
						FString UnitInitial = (SelectedUnit->AttackType == EAttackType::RANGED) ? TEXT("S") : TEXT("B");
						GM->AddGameLog(FString::Printf(TEXT("HP: %s Attesa (Salta Attacco)"), *UnitInitial));
						GM->CheckRemainingMoves(); // Sblocca il flusso e passa al nemico se era l'ultima unità!
					}
				}

				// Deseleziona l'unità (eseguito in ogni caso)
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
				// SALVIAMO LE COORDINATE VECCHIE PRIMA DI SOVRASCRIVERLE!
				char StartLetter = 'A' + SelectedUnit->CurrentTile->GetGridPosition().Y;
				int32 StartNumber = SelectedUnit->CurrentTile->GetGridPosition().X;

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
					char EndLetter = 'A' + ClickedTile->GetGridPosition().Y;
					FString UnitInitial = (SelectedUnit->AttackType == EAttackType::RANGED) ? TEXT("S") : TEXT("B");

					// ORA STAMPA CORRETTAMENTE LE COORDINATE!
					FString MoveMsg = FString::Printf(TEXT("HP: %s %c%d -> %c%d"), *UnitInitial, StartLetter, StartNumber, EndLetter, ClickedTile->GetGridPosition().X);
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