#include "GameField.h"
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
	TileGridPosition = FVector2D(0, 0);
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

void ATile::SetGridPosition(double InX, double InY)
{
	TileGridPosition.Set(InX, InY);
}

FVector2D ATile::GetGridPosition() const
{
	return TileGridPosition;
}