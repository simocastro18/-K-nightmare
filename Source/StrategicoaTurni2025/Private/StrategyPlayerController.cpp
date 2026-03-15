#include "StrategyPlayerController.h"
#include "Tile.h"
#include "StrategyUnit.h" // Includiamo la nostra nuova classe
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"// DA CONTROLLARE 
#include "GameField.h"
#include "StrategyGameMode.h"

void AStrategyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	CurrentSelectedTile = nullptr;
	SelectedUnit = nullptr;

	// Troviamo il GameField all'avvio e ce lo salviamo in memoria O(1)
	AActor* FoundField = UGameplayStatics::GetActorOfClass(GetWorld(), AGameField::StaticClass());
	if (FoundField)
	{
		GameFieldRef = Cast<AGameField>(FoundField);
	}
	// --- NOVITA': Attiviamo l'Enhanced Input ---
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0); // Lo usiamo come interruttore
		}
	}

}

void AStrategyPlayerController::HandleTileClick(ATile* ClickedTile)
{
	if (!IsValid(ClickedTile)) return;

	if (!ClickedTile->bIsWalkable || ClickedTile->Status == ETileStatus::OBSTACLE)
	{
		UE_LOG(LogTemp, Warning, TEXT("DEBUG: Click su Ostacolo/Acqua! Ignoro il click."));

		// Spengo il cursore visivo se era acceso su un'altra cella
		if (IsValid(CurrentSelectedTile))
		{
			CurrentSelectedTile->OnSelectionChanged(false);
			CurrentSelectedTile = nullptr;
		}

		// Deseleziono lo Sniper e spengo tutte le luci di Dijkstra
		SelectedUnit = nullptr;
		if (IsValid(GameFieldRef))
		{
			GameFieldRef->ClearHighlightedTiles();
		}

		return; // Muro di gomma: usciamo subito dalla funzione!
	}

	if (CurrentSelectedTile == ClickedTile)
	{
		// Se riclicco ESATTAMENTE la stessa cella: spengo tutto e resetto
		ClickedTile->OnSelectionChanged(false);
		CurrentSelectedTile = nullptr;
		SelectedUnit = nullptr;

		if (IsValid(GameFieldRef))
		{
			GameFieldRef->ClearHighlightedTiles();
		}

		UE_LOG(LogTemp, Warning, TEXT("DEBUG: Cella deselezionata cliccandola di nuovo."));
		return; // IMPORTANTE: Esco dalla funzione, non faccio altro!
	}
	else
	{
		// Se clicco una cella NUOVA: spengo la vecchia e accendo questa
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

	// 3. SE CLICCO SU UN'UNITA'
	if (IsValid(ClickedTile->UnitOnTile))
	{
		AStrategyUnit* ClickedUnit = Cast<AStrategyUnit>(ClickedTile->UnitOnTile);
		if (ClickedUnit)
		{
			// DEBUG TEAM
			UE_LOG(LogTemp, Warning, TEXT("DEBUG: Cliccata unita %s | Team: %d"), *ClickedUnit->UnitLogID, (int32)ClickedUnit->UnitTeam);

			// Se clicco un'unita' NEMICA (Team 1 se AI è 1)
			if (ClickedUnit->UnitTeam != ETeam::Player)
			{
				UE_LOG(LogTemp, Error, TEXT("DEBUG: Click su nemico! Ignoro selezione."));
				return;
			}

			// Se clicco un'unita' che ha GIA' MOSSO
			if (ClickedUnit->bHasMovedThisTurn)
			{
				UE_LOG(LogTemp, Error, TEXT("DEBUG: Unita' stanca!"));
				return;
			}

			// SELEZIONE VALIDA
			if (SelectedUnit == ClickedUnit)
			{
				// Deseleziona se clicco la stessa
				SelectedUnit = nullptr;
				if (IsValid(GameFieldRef)) GameFieldRef->ClearHighlightedTiles();
			}
			else
			{
				// Pulisco il vecchio percorso e mostro il nuovo
				if (IsValid(GameFieldRef)) GameFieldRef->ClearHighlightedTiles();
				SelectedUnit = ClickedUnit;

				if (IsValid(GameFieldRef))
				{
					UE_LOG(LogTemp, Warning, TEXT("DEBUG: Lancio Dijkstra per %s"), *SelectedUnit->UnitLogID);
					GameFieldRef->HighlightReachableTiles(SelectedUnit);
				}
				else {
					UE_LOG(LogTemp, Error, TEXT("DEBUG: GameFieldRef nullo! Dijkstra non puo' partire."));
				}
			}
		}
	}
	// 4. SE CLICCO SUL VUOTO
	else
	{
		if (IsValid(SelectedUnit))
		{
			// CONTROLLO: La cella cliccata fa parte del percorso illuminato?
			if (IsValid(GameFieldRef) && GameFieldRef->HighlightedTiles.Contains(ClickedTile))
			{
				UE_LOG(LogTemp, Warning, TEXT("Teletrasporto in corso su X:%d Y:%d"), ClickedTile->GetGridPosition().X, ClickedTile->GetGridPosition().Y);

				// --- 1. TROVIAMO E LIBERIAMO LA VECCHIA CELLA ---
				// Cerchiamo in tutta la TileMap quale cella ha attualmente questa unità
				ATile* OldTile = nullptr;
				for (auto& Pair : GameFieldRef->TileMap)
				{
					if (Pair.Value->UnitOnTile == SelectedUnit)
					{
						OldTile = Pair.Value;
						break; // Trovata! Fermiamo la ricerca
					}
				}

				if (OldTile)
				{
					OldTile->UnitOnTile = nullptr; // Liberiamo la cella di partenza!
				}

				// --- 2. OCCUPIAMO LA NUOVA CELLA ---
				ClickedTile->UnitOnTile = SelectedUnit;

				// --- 3. SPOSTAMENTO FISICO ---
				FVector NewLocation = ClickedTile->GetActorLocation();
				NewLocation.Z += 50.0f; // Alziamo la Z per non farlo sprofondare nella cella. Regola questo 50 a tuo piacimento!
				SelectedUnit->SetActorLocation(NewLocation);

				// --- 4. GESTIONE TURNO E PULIZIA ---
				SelectedUnit->bHasMovedThisTurn = true; // L'unità ha fatto la sua mossa
				SelectedUnit = nullptr;                 // Deselezioniamo l'unità
				GameFieldRef->ClearHighlightedTiles();  // Spegniamo le luci azzurre

				// --- 5. CONTROLLO FINE TURNO AUTOMATICA ---
				// esiste gia' AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
				if (GM)
				{
					GM->CheckRemainingMoves();
				}
			}
			else
			{
				// Se clicco su una cella non illuminata (troppo lontana)
				UE_LOG(LogTemp, Warning, TEXT("DEBUG: Click fuori dal percorso. Deseleziono."));
				SelectedUnit = nullptr;
				if (IsValid(GameFieldRef)) GameFieldRef->ClearHighlightedTiles();
			}
		}
	}
}

void AStrategyPlayerController::ExecuteClick()
{
	FHitResult HitResult;

	// Spara un raggio dal mouse verso lo schermo usando il canale Visibility
	if (GetHitResultUnderCursor(ECC_Visibility, true, HitResult))
	{
		AActor* HitActor = HitResult.GetActor();
		if (!HitActor) return;

		// CASO A: Abbiamo colpito una Cella (Tile)?
		ATile* HitTile = Cast<ATile>(HitActor);
		if (HitTile)
		{
			HandleTileClick(HitTile);
			return; // Usciamo, abbiamo finito
		}

		// CASO B: Abbiamo colpito la mesh di un'Unità? (Il famoso "Piano B")
		AStrategyUnit* HitUnit = Cast<AStrategyUnit>(HitActor);
		if (HitUnit && HitUnit->CurrentTile)
		{
			// Abbiamo colpito la truppa, quindi passiamo la sua cella alla logica
			HandleTileClick(HitUnit->CurrentTile);
			return;
		}
	}
}

void AStrategyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Leghiamo il nostro ClickAction alla nostra funzione ExecuteClick!
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		if (ClickAction)
		{
			EnhancedInputComponent->BindAction(ClickAction, ETriggerEvent::Started, this, &AStrategyPlayerController::ExecuteClick);
		}
	}
}