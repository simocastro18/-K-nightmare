#include "Tile.h" // FONDAMENTALE: permette al GameField di vedere le funzioni di ATile
#include "Math/UnrealMathUtility.h"
#include "Engine/World.h" // Per GetWorld() e SpawnActor
#include "Kismet/GameplayStatics.h"
#include "StrategyPlayerController.h"

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

	PathMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PathMesh"));
	PathMesh->SetupAttachment(SceneRoot);
	PathMesh->SetVisibility(false);
	// NUOVO: Disattiva completamente le collisioni!
	PathMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	AttackMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AttackMesh"));
	AttackMesh->SetupAttachment(SceneRoot);
	AttackMesh->SetVisibility(false);
	// NUOVO: Disattiva completamente le collisioni!
	AttackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Status = ETileStatus::EMPTY;
	PlayerOwner = -1;
	Elevation = 0;
	bIsWalkable = false;
	TileGridPosition = FIntPoint(0, 0);
	UnitOnTile = nullptr;
}

void ATile::BeginPlay()
{
	Super::BeginPlay();
	//per debug
	/*if (AttackMesh)
	{
		AttackMesh->SetVisibility(true);
	}
	*/ 
}

// 2. LOGICA DEL MOUSE - HOVER E CLICK
void ATile::NotifyActorBeginCursorOver()
{
	Super::NotifyActorBeginCursorOver();
	// Quando il mouse passa sopra, accendi la luce azzurra (Player)
	if (!bIsCurrentlySelected && PathMesh)
	{
		// Applica il materiale del Player prima di accendere
		if (PlayerPathMaterial)
		{
			PathMesh->SetMaterial(0, PlayerPathMaterial);
		}
		PathMesh->SetVisibility(true);
	}
}

void ATile::NotifyActorEndCursorOver()
{
	Super::NotifyActorEndCursorOver();
	// Quando il mouse esce, spegnila
	if (!bIsCurrentlySelected && PathMesh)
	{
		PathMesh->SetVisibility(false);
	}
}
/*
void ATile::NotifyActorOnClicked(FKey ButtonPressed)
{
	Super::NotifyActorOnClicked(ButtonPressed);

	// [DA COMPLETARE] Qui dentro chiami il tuo Controller come facevi nel Blueprint (Screenshot 1).
	// Esempio:
	// AStrategyPlayerController* PC = Cast<AStrategyPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	// if (PC) { PC->ProcessTileClick(this); }
}
*/
void ATile::OnSelectionChanged(bool bIsSelected, bool bIsAI)
{
	bIsCurrentlySelected = bIsSelected;
	if (PathMesh)
	{
		if (bIsSelected)
		{
			// Cambia il materiale in base a chi sta calcolando il percorso
			if (bIsAI && AIPathMaterial)
			{
				PathMesh->SetMaterial(0, AIPathMaterial);
			}
			else if (!bIsAI && PlayerPathMaterial)
			{
				PathMesh->SetMaterial(0, PlayerPathMaterial);
			}
		}
		PathMesh->SetVisibility(bIsSelected);
	}
}


void ATile::UpdateAttackHighlight(bool bIsHighlighted)
{
	if (AttackMesh)
	{
		AttackMesh->SetVisibility(bIsHighlighted);
	}
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

void ATile::UpdateTileColor()
{
	if (!TileMesh) return;

	// Creiamo il materiale dinamico solo la prima volta
	if (!DynamicMaterial)
	{
		DynamicMaterial = TileMesh->CreateDynamicMaterialInstance(0);
	}

	if (DynamicMaterial)
	{
		FLinearColor TileColor;

		// Assegnazione colori in base all'Elevation (come nel tuo Blueprint)
		switch (Elevation)
		{
		case 0: TileColor = FLinearColor::Blue; break;       // Ostacolo / Acqua
		case 1: TileColor = FLinearColor::Green; break;      // Erba
		case 2: TileColor = FLinearColor(0.8f, 0.8f, 0.0f); break; // Giallo
		case 3: TileColor = FLinearColor(1.0f, 0.3f, 0.0f); break;// Arancione
		case 4: TileColor = FLinearColor::Red; break;      // Rosso
		default: TileColor = FLinearColor::Black; break;
		}

		// Inviamo il colore al Materiale
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), TileColor);
	}
}
