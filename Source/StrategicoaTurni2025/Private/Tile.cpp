#include "Tile.h" // FONDAMENTALE: permette al GameField di vedere le funzioni di ATile
#include "Math/UnrealMathUtility.h"
#include "Engine/World.h" // Per GetWorld() e SpawnActor

// Costruttore: inizializziamo i componenti
ATile::ATile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Creiamo la radice della scena
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Creiamo la mesh statica e la attacchiamo alla radice
	TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	TileMesh->SetupAttachment(SceneRoot);

	// Valori di default
	Status = ETileStatus::EMPTY;
	PlayerOwner = -1;
	Elevation = 0;
	bIsWalkable = false;
	TileGridPosition = FIntPoint(0, 0); // AGGIORNATO
	UnitOnTile = nullptr; // AGGIUNTO PER SICUREZZA
}

void ATile::BeginPlay()
{
	Super::BeginPlay();
}

void ATile::SetTileStatus(int32 NewOwner, ETileStatus NewStatus)
{
	PlayerOwner = NewOwner;
	Status = NewStatus;
}

void ATile::SetGridPosition(int32 InX, int32 InY) // AGGIORNATO
{
	TileGridPosition.X = InX;
	TileGridPosition.Y = InY;
}

FIntPoint ATile::GetGridPosition() const // AGGIORNATO
{
	return TileGridPosition;
}

void ATile::SetUnitOnTile(AActor* NewUnit)
{
	UnitOnTile = NewUnit;
}

