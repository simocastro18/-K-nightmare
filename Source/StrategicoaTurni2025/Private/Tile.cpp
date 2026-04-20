#include "Tile.h" 
#include "Math/UnrealMathUtility.h"
#include "Engine/World.h" 
#include "Kismet/GameplayStatics.h"
#include "StrategyPlayerController.h"

ATile::ATile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Initialize the root scene component
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Initialize and attach the main physical mesh
	TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	TileMesh->SetupAttachment(SceneRoot);

	// Initialize the visual highlight mesh for pathing and disable collisions
	PathMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PathMesh"));
	PathMesh->SetupAttachment(SceneRoot);
	PathMesh->SetVisibility(false);
	PathMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Initialize the visual highlight mesh for combat targeting and disable collisions
	AttackMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AttackMesh"));
	AttackMesh->SetupAttachment(SceneRoot);
	AttackMesh->SetVisibility(false);
	AttackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Default tile properties
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
}

void ATile::NotifyActorBeginCursorOver()
{
	Super::NotifyActorBeginCursorOver();

	// Enable player highlight path on mouse hover if not locked by active selection
	if (!bIsCurrentlySelected && PathMesh)
	{
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

	// Disable highlight on mouse exit if not locked by active selection
	if (!bIsCurrentlySelected && PathMesh)
	{
		PathMesh->SetVisibility(false);
	}
}

void ATile::OnSelectionChanged(bool bIsSelected, bool bIsAI)
{
	bIsCurrentlySelected = bIsSelected;

	if (PathMesh)
	{
		if (bIsSelected)
		{
			// Apply the appropriate material depending on which entity is evaluating the path
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

void ATile::SetGridPosition(int32 InX, int32 InY)
{
	TileGridPosition.X = InX;
	TileGridPosition.Y = InY;
}

FIntPoint ATile::GetGridPosition() const
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

	// Instantiate the dynamic material only once to optimize performance
	if (!DynamicMaterial)
	{
		DynamicMaterial = TileMesh->CreateDynamicMaterialInstance(0);
	}

	if (DynamicMaterial)
	{
		FLinearColor TileColor;

		// Assign tile color based on its procedural elevation level
		switch (Elevation)
		{
		case 0: TileColor = FLinearColor::Blue; break;               // Level 0: Water (Obstacle)
		case 1: TileColor = FLinearColor::Green; break;              // Level 1: Flat Ground
		case 2: TileColor = FLinearColor(0.8f, 0.8f, 0.0f); break;   // Level 2: Low Elevation (Yellow)
		case 3: TileColor = FLinearColor(1.0f, 0.3f, 0.0f); break;   // Level 3: Medium Elevation (Orange)
		case 4: TileColor = FLinearColor::Red; break;                // Level 4: High Mountain (Red)
		default: TileColor = FLinearColor::Black; break;
		}

		// Push the calculated color to the shader parameter
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), TileColor);
	}
}