#include "StrategyGameMode.h"
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