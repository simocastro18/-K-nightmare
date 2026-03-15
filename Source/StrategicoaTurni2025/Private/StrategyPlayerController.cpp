#include "StrategyPlayerController.h"
#include "Tile.h"

void AStrategyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Mostriamo il cursore del mouse all'avvio
	bShowMouseCursor = true;

	// Assicuriamoci che i puntatori partano puliti
	CurrentSelectedTile = nullptr;
	SelectedUnit = nullptr;
}

void AStrategyPlayerController::HandleTileClick(ATile* ClickedTile)
{
	if (!ClickedTile) return;

	// 1. Gestione Visiva (Accensione/Spegnimento O(1))
	if (CurrentSelectedTile != nullptr && CurrentSelectedTile != ClickedTile)
	{
		// Spegniamo la cella precedente
		CurrentSelectedTile->OnSelectionChanged(false);
	}

	// Accendiamo la nuova cella
	ClickedTile->OnSelectionChanged(true);

	// Aggiorniamo la memoria
	CurrentSelectedTile = ClickedTile;

	// 2. Logica di Selezione/Movimento (Da implementare in seguito)
	UE_LOG(LogTemp, Warning, TEXT("Click su Tile: X=%d, Y=%d"), ClickedTile->GetGridPosition().X, ClickedTile->GetGridPosition().Y);
}