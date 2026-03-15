#include "StrategyGameMode.h"
#include "StrategyPlayerController.h"
#include "StrategyUnit.h"
#include "GameField.h"
#include "Kismet/GameplayStatics.h" // Per trovare il GameField nel mondo

void AStrategyGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Troviamo automaticamente l'oggetto BP_GameField che hai trascinato nella mappa
	AActor* FoundField = UGameplayStatics::GetActorOfClass(GetWorld(), AGameField::StaticClass());
	if (FoundField)
	{
		MapGenerator = Cast<AGameField>(FoundField);
	}
}

void AStrategyGameMode::StartGameWithConfig(float NoiseScale, int32 GridSizeX, int32 GridSizeY)
{
	if (MapGenerator)
	{	
		// Protezione contro i valori a zero o assurdi
		int32 SafeX = (GridSizeX <= 0) ? 25 : GridSizeX;
		int32 SafeY = (GridSizeY <= 0) ? 25 : GridSizeY;
		float SafeNoise = (NoiseScale <= 0.0f) ? 0.1f : NoiseScale;

		AStrategyPlayerController* PC = Cast<AStrategyPlayerController>(GetWorld()->GetFirstPlayerController());
		if (PC)
		{
			// Consegniamo la mappa direttamente nelle mani del Player Controller
			PC->GameFieldRef = MapGenerator;
		}

		// 1. Applichiamo i dati ricevuti dall'utente
		MapGenerator->NoiseScale = NoiseScale;
		MapGenerator->GridSizeX = GridSizeX;
		MapGenerator->GridSizeY = GridSizeY;

		// 2. Generiamo la griglia (Sincrono: il codice si ferma finché non ha finito!)
		MapGenerator->GenerateGridData();

		// 3. Spawnamo truppe e torri sicuri al 100% che la griglia esista
		MapGenerator->SpawnInitialEntities();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ERRORE CRITICO: AGameField non trovato nel livello!"));
	}
}

void AStrategyGameMode::CheckRemainingMoves()
{
	// 1. Troviamo tutte le unità nella mappa
	TArray<AActor*> AllUnits;
	// Se ti dà errore qui, manca #include "Kismet/GameplayStatics.h"
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyUnit::StaticClass(), AllUnits);

	bool bAnyUnitCanMove = false;

	for (AActor* Actor : AllUnits)
	{
		// Se ti dà errore su AStrategyUnit, manca #include "StrategyUnit.h"
		AStrategyUnit* Unit = Cast<AStrategyUnit>(Actor);

		// Verifichiamo se l'unità appartiene al team del turno attuale
		ETeam CurrentActiveTeam = (CurrentTurnState == ETurnState::PlayerTurn) ? ETeam::Player : ETeam::AI;

		if (Unit && Unit->UnitTeam == CurrentActiveTeam)
		{
			if (!Unit->bHasMovedThisTurn)
			{
				bAnyUnitCanMove = true;
				break; // Ne basta una che può muovere per non finire il turno
			}
		}
	}

	// 2. Se nessuno può più muovere, cambiamo turno automaticamente
	if (!bAnyUnitCanMove && AllUnits.Num() > 0) // Controlliamo che ci sia almeno un'unità
	{
		UE_LOG(LogTemp, Warning, TEXT("Tutte le unità hanno mosso o agito. Cambio Turno!"));
		EndTurn();
	}
}

void AStrategyGameMode::EndTurn()
{
	if (CurrentTurnState == ETurnState::PlayerTurn)
	{
		CurrentTurnState = ETurnState::AITurn;
		UE_LOG(LogTemp, Warning, TEXT("=== TURNO DELL'INTELLIGENZA ARTIFICIALE ==="));

		// TODO: Qui chiameremo la funzione che fa pensare l'AI
	}
	else
	{
		CurrentTurnState = ETurnState::PlayerTurn;
		UE_LOG(LogTemp, Warning, TEXT("=== TURNO DEL GIOCATORE ==="));
	}

	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStrategyUnit::StaticClass(), AllUnits);
	for (AActor* Actor : AllUnits)
	{
		if (AStrategyUnit* Unit = Cast<AStrategyUnit>(Actor))
		{
			Unit->bHasMovedThisTurn = false;
		}
	}
	// Nota: Dovremmo fare un ciclo su tutte le unità per resettare bHasMovedThisTurn
}