#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameField.generated.h"

/*
   Struct representing the raw data for a single grid cell.
   Used during the procedural generation phase before physical tiles are spawned.
 */
USTRUCT(BlueprintType)
struct FGridCell
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Data")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Data")
	int32 Y = 0;

	// Elevation level from 0 (Water) to 4 (High Mountain)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Data")
	int32 Elevation = 0;

	// Movement flag: false for level 0 (Water), true for levels 1-4
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Data")
	bool bIsWalkable = false;
};

UCLASS()
class STRATEGICOATURNI2025_API AGameField : public AActor
{
	GENERATED_BODY()

public:
	AGameField();

	// Unit and Object Classes

	UPROPERTY(EditAnywhere, Category = "Game Logic|Units")
	TSubclassOf<AActor> BrawlerClass;

	UPROPERTY(EditAnywhere, Category = "Game Logic|Units")
	TSubclassOf<AActor> SniperClass;

	UPROPERTY(EditAnywhere, Category = "Game Logic|Units")
	TSubclassOf<AActor> TowerClass;

	// Handles the procedural generation of the grid and the initial placement of towers
	UFUNCTION(BlueprintCallable, Category = "Game Logic")
	void SpawnInitialEntities();

	// Map Generation Settings

	// Determines if the Perlin Noise generator should use a random seed or a fixed one for debugging
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	bool bUseRandomSeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	int32 GridSizeX = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	int32 GridSizeY = 25;

	// Scale factor for the Perlin Noise frequency
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	float NoiseScale = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	int32 MaxHeight = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	int32 RandomSeed = 0;

	// The specific Blueprint class used to instantiate the physical tiles on the map
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	TSubclassOf<class ATile> TileClass;

	// Distance between the centers of two adjacent tiles
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
	float CellSize = 100.0f;

	// Data Structures

	// Array containing the logical data for each cell before spawning
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Data")
	TArray<FGridCell> GridData;

	// Core spatial dictionary linking grid coordinates to their corresponding ATile actor
	UPROPERTY(Transient)
	TMap<FIntPoint, class ATile*> TileMap;

	// Logic to generate heights and properties for the entire grid
	UFUNCTION(BlueprintCallable, Category = "Map Generation")
	void GenerateGridData();

	// Pathfinding and Movement 

	UPROPERTY(BlueprintReadWrite, Category = "Pathfinding")
	class ATile* TargetTile;

	// List of tiles currently highlighted for movement
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pathfinding")
	TArray<class ATile*> HighlightedTiles;

	// Internal map used by Dijkstra/A* to reconstruct the path
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pathfinding")
	TMap<class ATile*, class ATile*> CameFromMap;

	// Calculates the world-space path coordinates to a destination tile
	UFUNCTION(BlueprintCallable, Category = "Pathfinding")
	TArray<FVector> GetPathToTile(class ATile* DestinationTile);

	// Calculates and visually highlights all reachable tiles for a selected unit
	UFUNCTION(BlueprintCallable, Category = "Pathfinding")
	void HighlightReachableTiles(class AStrategyUnit* SelectedUnit);

	// Turns off all movement highlights on the grid
	UFUNCTION(BlueprintCallable, Category = "Pathfinding")
	void ClearHighlightedTiles();

	// Highlights valid deployment rows for player and AI at the start of the match
	UFUNCTION(BlueprintCallable, Category = "Deployment")
	void HighlightDeploymentZone();

	// Combat System

	// List of tiles containing enemies within attack range
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TArray<class ATile*> AttackableTiles;

	// Identifies and highlights all enemy units that the current unit can attack
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void HighlightAttackableTiles(class AStrategyUnit* AttackingUnit);

	// Resets all attack-related visual highlights
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ClearAttackableTiles();

	// Uses Bresenham's line algorithm to check if a clear line of sight exists between two tiles
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool HasLineOfSight(class ATile* InStartTile, class ATile* InTargetTile);

	// AI Algorithms

	// Implements A* algorithm to find the optimal path to a target
	UFUNCTION(BlueprintCallable, Category = "AI Pathfinding")
	TArray<class ATile*> FindPathAStar(class ATile* StartTile, class ATile* InTargetTile);

	// Implements Greedy Best-First Search for fast, heuristic-based movement
	UFUNCTION(BlueprintCallable, Category = "AI Pathfinding")
	TArray<class ATile*> FindPathGreedy(class ATile* StartTile, class ATile* InTargetTile);

	// Handles spawning of a specific AI unit based on the deployment sequence
	UFUNCTION(BlueprintCallable, Category = "Game Logic")
	void SpawnSingleAIUnit(int32 UnitIndex);


protected:
	virtual void BeginPlay() override;

private:
	// Validates if every walkable tile on the map is reachable from any other walkable tile
	bool IsMapFullyConnected();
};