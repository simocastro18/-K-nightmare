#include "GameField.h"
#include "Tile.h"  
#include "StrategyUnit.h"
#include "StrategyTower.h"
#include "StrategyGameMode.h"
#include "Math/UnrealMathUtility.h"
#include "Kismet/GameplayStatics.h"

AGameField::AGameField()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGameField::BeginPlay()
{
	Super::BeginPlay();
	
	// Graphic Optimization: Force shadows off to ensure consistent performance on any hardware
	// Shadows ruins this gameplay
	if (GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("sg.ShadowQuality 0"));
		GEngine->Exec(GetWorld(), TEXT("r.ShadowQuality 0"));
	}
}

void AGameField::GenerateGridData()
{
	bool bIsMapValid = false;
	int32 SafeMaxHeight = (MaxHeight <= 0) ? 3 : MaxHeight;
	float SafeNoise = (NoiseScale <= 0.0f) ? 0.1f : NoiseScale;
	int32 Attempts = 0;

	// Loop until a fully connected map is generated (max 50 attempts to avoid infinite loops)
	while (!bIsMapValid && Attempts < 50)
	{
		Attempts++;
		float OffsetX = 0.0f;
		float OffsetY = 0.0f;

		// Use a random seed for procedural generation if requested, or if previous attempts failed
		if (bUseRandomSeed || Attempts > 1)
		{
			RandomSeed = FMath::RandRange(1, 999999);
			OffsetX = (RandomSeed % 1000) * 10.0f;
			OffsetY = ((RandomSeed * 2) % 1000) * 10.0f;
		}

		GridData.Empty();

		// Algorithm: 2D Perlin Noise for natural terrain generation
		for (int32 Y = 0; Y < GridSizeY; ++Y)
		{
			for (int32 X = 0; X < GridSizeX; ++X)
			{
				FGridCell NewCell;
				NewCell.X = X;
				NewCell.Y = Y;

				float SampleX = (X * SafeNoise) + OffsetX;
				float SampleY = (Y * SafeNoise) + OffsetY;
				float NoiseValue = FMath::PerlinNoise2D(FVector2D(SampleX, SampleY));

				// Normalize the noise from [-1, 1] to [0, 1] and apply clamping/exponentiation for better island shapes
				float NormalizedNoise = (NoiseValue + 1.0f) / 2.0f;
				NormalizedNoise = FMath::Pow(NormalizedNoise, 1.8f);
				NormalizedNoise = NormalizedNoise * 1.55f;
				NormalizedNoise = NormalizedNoise - 0.10f;
				NormalizedNoise = FMath::Clamp(NormalizedNoise, 0.0f, 1.0f);

				NewCell.Elevation = FMath::RoundToInt(NormalizedNoise * SafeMaxHeight);
				NewCell.Elevation = FMath::Clamp(NewCell.Elevation, 0, SafeMaxHeight);
				NewCell.bIsWalkable = (NewCell.Elevation > 0); // Elevation 0 is water (obstacle)

				GridData.Add(NewCell);
			}
		}

		// Verify if the generated layout contains isolated inaccessible areas
		bIsMapValid = IsMapFullyConnected();
	}

	TileMap.Empty();

	// Spawn the physical tiles based on the generated logical grid
	for (const FGridCell& Cell : GridData)
	{
		FVector SpawnLocation = FVector(Cell.X * CellSize, Cell.Y * CellSize, 0.0f);

		if (TileClass != nullptr)
		{
			ATile* NewTile = GetWorld()->SpawnActor<ATile>(TileClass, SpawnLocation, FRotator::ZeroRotator);

			if (NewTile)
			{
				NewTile->SetGridPosition(Cell.X, Cell.Y);
				NewTile->Elevation = Cell.Elevation;
				NewTile->bIsWalkable = Cell.bIsWalkable;

				NewTile->UpdateTileColor();

				if (!Cell.bIsWalkable)
				{
					NewTile->SetTileStatus(-1, ETileStatus::OBSTACLE);
				}

				TileMap.Add(FIntPoint(Cell.X, Cell.Y), NewTile);
				NewTile->OnTileDataInitialized();
			}
		}
	}
}

bool AGameField::IsMapFullyConnected()
{
	int32 TotalWalkableCells = 0;
	int32 StartX = -1;
	int32 StartY = -1;

	// Count total walkable cells and find a starting point for the flood fill
	for (const FGridCell& Cell : GridData)
	{
		if (Cell.bIsWalkable)
		{
			TotalWalkableCells++;
			if (StartX == -1)
			{
				StartX = Cell.X;
				StartY = Cell.Y;
			}
		}
	}

	if (TotalWalkableCells == 0) return false;

	// Algorithm: Breadth-First Search (BFS)
	// Flood Fill to check map connectivity
	TArray<bool> Visited;
	Visited.Init(false, GridSizeX * GridSizeY);

	TArray<FIntPoint> Queue;
	int32 QueueIndex = 0;

	Queue.Add(FIntPoint(StartX, StartY));
	Visited[StartY * GridSizeX + StartX] = true;

	int32 ReachedCells = 0;

	while (QueueIndex < Queue.Num())
	{
		FIntPoint Current = Queue[QueueIndex];
		QueueIndex++;
		ReachedCells++;

		FIntPoint Neighbors[4] = {
			FIntPoint(Current.X, Current.Y + 1),
			FIntPoint(Current.X, Current.Y - 1),
			FIntPoint(Current.X - 1, Current.Y),
			FIntPoint(Current.X + 1, Current.Y)
		};

		for (FIntPoint N : Neighbors)
		{
			if (N.X >= 0 && N.X < GridSizeX && N.Y >= 0 && N.Y < GridSizeY)
			{
				int32 Index = N.Y * GridSizeX + N.X;

				if (!Visited[Index] && GridData[Index].bIsWalkable)
				{
					Visited[Index] = true;
					Queue.Add(N);
				}
			}
		}
	}

	// The map is fully connected if the BFS reached all walkable cells
	return ReachedCells == TotalWalkableCells;
}

void AGameField::SpawnInitialEntities()
{
	if (TileMap.Num() == 0) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Lambda function for adaptive tower placement (finds the closest valid tile to the ideal coordinate)
	auto SpawnTowerAtBestLocation = [&](FIntPoint TargetCoord) {
		ATile* BestTile = nullptr;
		float MinDistance = 999999.0f;

		for (auto& Elem : TileMap)
		{
			ATile* T = Elem.Value;
			if (T && T->IsValidLowLevel() && T->bIsWalkable && T->Status == ETileStatus::EMPTY)
			{
				float Dist = FVector2D::Distance(
					FVector2D(T->TileGridPosition.X, T->TileGridPosition.Y),
					FVector2D(TargetCoord.X, TargetCoord.Y)
				);

				if (Dist < MinDistance)
				{
					MinDistance = Dist;
					BestTile = T;
				}
			}
		}

		if (BestTile && TowerClass)
		{
			FVector Loc = BestTile->GetActorLocation() + FVector(0, 0, 100);
			AActor* NewTower = GetWorld()->SpawnActor<AActor>(TowerClass, Loc, FRotator::ZeroRotator, SpawnParams);
			if (NewTower)
			{
				BestTile->SetTileStatus(-1, ETileStatus::OBSTACLE);

				AStrategyTower* StratTower = Cast<AStrategyTower>(NewTower);
				if (StratTower)
				{
					StratTower->GridPosition = BestTile->GetGridPosition();
				}
			}
		}
		};

	// Ideal target coordinates for towers (Center, Left, Right)
	SpawnTowerAtBestLocation(FIntPoint(12, 12));
	SpawnTowerAtBestLocation(FIntPoint(12, 5));
	SpawnTowerAtBestLocation(FIntPoint(12, 19));
}

void AGameField::SpawnSingleAIUnit(int32 UnitIndex)
{
	TArray<ATile*> AIZone;

	// AI valid deployment zone (Top 3 rows of the grid)
	for (auto& Elem : TileMap)
	{
		ATile* T = Elem.Value;
		if (T && T->IsValidLowLevel() && T->bIsWalkable && T->Status == ETileStatus::EMPTY)
		{
			if (T->GetGridPosition().X >= GridSizeX - 3)
			{
				AIZone.Add(T);
			}
		}
	}

	if (AIZone.Num() > 0)
	{
		int32 Rnd = FMath::RandRange(0, AIZone.Num() - 1);
		ATile* T = AIZone[Rnd];

		// Alternate spawning: Brawler (Index 0), Sniper (Index 1)
		TSubclassOf<AActor> ClassToSpawn = (UnitIndex == 0) ? BrawlerClass : SniperClass;
		FString LogID = (UnitIndex == 0) ? TEXT("Brawler_AI") : TEXT("Sniper_AI");

		FVector Loc = T->GetActorLocation() + FVector(0, 0, 100);
		FTransform SpawnTransform(FRotator(0.0f, 90.0f, 0.0f), Loc, FVector(0.5f, 0.5f, 0.5f));

		AActor* NewUnit = GetWorld()->SpawnActorDeferred<AActor>(ClassToSpawn, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (NewUnit)
		{
			T->SetTileStatus(-1, ETileStatus::OCCUPIED);
			T->SetUnitOnTile(NewUnit);

			AStrategyUnit* StrategyUnit = Cast<AStrategyUnit>(NewUnit);
			if (StrategyUnit) { StrategyUnit->GameFieldRef = this; }

			UGameplayStatics::FinishSpawningActor(NewUnit, SpawnTransform);

			if (StrategyUnit) { StrategyUnit->InitializeUnit(LogID, ETeam::AI, 90.0f, T); }

			// Send deployment action to the Game Mode combat log
			AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
			if (GM)
			{
				FString UnitInitial = (UnitIndex == 0) ? TEXT("B") : TEXT("S");
				char ColLetter = 'A' + T->GetGridPosition().Y;
				int32 RowNum = T->GetGridPosition().X;

				GM->AddGameLog(FString::Printf(TEXT("AI: Placed %s in %c%d"), *UnitInitial, ColLetter, RowNum));
			}
		}
	}
}

void AGameField::ClearHighlightedTiles()
{
	for (ATile* Tile : HighlightedTiles)
	{
		if (IsValid(Tile))
		{
			Tile->OnSelectionChanged(false);
		}
	}
	HighlightedTiles.Empty();
}

void AGameField::HighlightReachableTiles(AStrategyUnit* SelectedUnit)
{
	ClearHighlightedTiles();

	if (!IsValid(SelectedUnit)) return;
	if (!IsValid(SelectedUnit->CurrentTile)) return;

	ATile* StartTile = SelectedUnit->CurrentTile;
	int32 MaxRange = SelectedUnit->MovementRange;

	if (MaxRange <= 0) return;

	// Algorithm: Dijkstra's Pathfinding (Cost-based grid exploration)
	TMap<ATile*, int32> CostSoFar;
	TArray<ATile*> Frontier;

	CostSoFar.Add(StartTile, 0);
	Frontier.Add(StartTile);

	FIntPoint Directions[4] = {
		FIntPoint(0, 1), FIntPoint(0, -1),
		FIntPoint(1, 0), FIntPoint(-1, 0)
	};

	CameFromMap.Empty();
	CameFromMap.Add(StartTile, nullptr);

	while (Frontier.Num() > 0)
	{
		// Extract the tile with the lowest accumulated cost
		int32 BestIndex = 0;
		for (int32 i = 1; i < Frontier.Num(); ++i)
		{
			if (CostSoFar[Frontier[i]] < CostSoFar[Frontier[BestIndex]])
			{
				BestIndex = i;
			}
		}

		ATile* Current = Frontier[BestIndex];
		Frontier.RemoveAt(BestIndex);

		if (!IsValid(Current)) continue;

		int32 CurrentCost = CostSoFar[Current];

		// Check all 4 adjacent neighbors
		for (FIntPoint Dir : Directions)
		{
			FIntPoint NeighborCoord = Current->GetGridPosition() + Dir;

			if (ATile** FoundTilePtr = TileMap.Find(NeighborCoord))
			{
				ATile* Neighbor = *FoundTilePtr;

				// Proceed only if walkable, not an obstacle, and unoccupied
				if (IsValid(Neighbor) && Neighbor->bIsWalkable && Neighbor->Status != ETileStatus::OBSTACLE && !IsValid(Neighbor->UnitOnTile))
				{
					// Movement cost rule
					int32 ElevationDiff = Neighbor->Elevation - Current->Elevation;
					int32 StepCost = 1; // Base cost for flat terrain or moving downhill

					if (ElevationDiff > 0)
					{
						// Uphill penalty: costs twice the elevation difference
						StepCost = ElevationDiff * 2;
					}

					int32 NewCost = CurrentCost + StepCost;

					if (NewCost <= MaxRange)
					{
						if (!CostSoFar.Contains(Neighbor) || NewCost < CostSoFar[Neighbor])
						{
							CostSoFar.Add(Neighbor, NewCost);
							CameFromMap.Add(Neighbor, Current);
							Frontier.Add(Neighbor);
						}
					}
				}
			}
		}
	}

	ClearHighlightedTiles();

	// Visually highlight all tiles processed within the allowed movement range
	for (auto& Elem : CostSoFar)
	{
		ATile* ReachableTile = Elem.Key;
		if (IsValid(ReachableTile) && ReachableTile != StartTile)
		{
			ReachableTile->OnSelectionChanged(true);
			HighlightedTiles.Add(ReachableTile);
		}
	}
}

TArray<FVector> AGameField::GetPathToTile(ATile* DestinationTile)
{
	TArray<FVector> Path;

	if (!IsValid(DestinationTile) || !CameFromMap.Contains(DestinationTile))
	{
		return Path;
	}

	ATile* CurrentTile = DestinationTile;
	int32 SafetyCounter = 0;

	// Semantic safety limit: the path can never be longer than the total number of tiles on the map
	int32 MaxSafeSteps = GridSizeX * GridSizeY;

	// Backtrack from destination to start using the CameFromMap
	while (IsValid(CurrentTile) && SafetyCounter < MaxSafeSteps)
	{
		Path.Add(CurrentTile->GetActorLocation());

		ATile** ParentPtr = CameFromMap.Find(CurrentTile);

		if (ParentPtr && *ParentPtr != nullptr)
		{
			CurrentTile = *ParentPtr;
		}
		else
		{
			break;
		}

		SafetyCounter++;
	}

	// Reverse array to obtain Start->Destination order
	int32 NumElements = Path.Num();
	for (int32 i = 0; i < NumElements / 2; ++i)
	{
		Path.Swap(i, NumElements - 1 - i);
	}

	// Remove the starting tile from the path execution
	if (Path.Num() > 0)
	{
		Path.RemoveAt(0);
	}

	return Path;
}

void AGameField::ClearAttackableTiles()
{
	for (ATile* Tile : AttackableTiles)
	{
		if (IsValid(Tile))
		{
			Tile->UpdateAttackHighlight(false);
		}
	}
	AttackableTiles.Empty();
}

void AGameField::HighlightAttackableTiles(AStrategyUnit* AttackingUnit)
{
	ClearAttackableTiles();

	if (!IsValid(AttackingUnit) || !IsValid(AttackingUnit->CurrentTile)) return;

	FIntPoint StartPos = AttackingUnit->CurrentTile->GetGridPosition();
	int32 Range = AttackingUnit->AttackRange;

	// Loop through a bounding box defined by the attack range
	for (int32 dx = -Range; dx <= Range; ++dx)
	{
		for (int32 dy = -Range; dy <= Range; ++dy)
		{
			// Manhattan distance check to form a diamond shape attack area
			if (FMath::Abs(dx) + FMath::Abs(dy) <= Range)
			{
				FIntPoint CheckPos(StartPos.X + dx, StartPos.Y + dy);

				if (ATile** FoundTilePtr = TileMap.Find(CheckPos))
				{
					ATile* CheckTile = *FoundTilePtr;

					if (IsValid(CheckTile) && CheckTile != AttackingUnit->CurrentTile)
					{
						// Target Identification and Friendly Fire check
						if (IsValid(CheckTile->UnitOnTile))
						{
							AStrategyUnit* TargetUnit = Cast<AStrategyUnit>(CheckTile->UnitOnTile);

							if (IsValid(TargetUnit) && TargetUnit->UnitTeam != AttackingUnit->UnitTeam)
							{
								// Sniper logic: Check Line of Sight against mountains
								if (AttackingUnit->AttackType == EAttackType::RANGED)
								{
									if (!HasLineOfSight(AttackingUnit->CurrentTile, CheckTile))
									{
										continue; // Visual is blocked, skip target
									}
								}

								// Elevation rule: Can only attack enemies on the same level or lower
								if (CheckTile->Elevation <= AttackingUnit->CurrentTile->Elevation)
								{
									CheckTile->UpdateAttackHighlight(true);
									AttackableTiles.Add(CheckTile);
								}
							}
						}
					}
				}
			}
		}
	}
}

TArray<ATile*> AGameField::FindPathAStar(ATile* InStartTile, ATile* InTargetTile)
{
	TArray<ATile*> FinalPath;
	if (!InStartTile || !InTargetTile) return FinalPath;

	// Algorithm: A* (A-Star)
	// Combines the guaranteed shortest path of Dijkstra with a distance heuristic
	TArray<ATile*> OpenSet;
	TSet<ATile*> ClosedSet;

	TMap<ATile*, int32> GScore; // Cost from start
	TMap<ATile*, int32> FScore; // GScore + Heuristic (Estimated distance to target)

	OpenSet.Add(InStartTile);
	GScore.Add(InStartTile, 0);

	// Heuristic: Manhattan Distance (Horizontal + Vertical steps)
	int32 DistX = FMath::Abs(InStartTile->GetGridPosition().X - InTargetTile->GetGridPosition().X);
	int32 DistY = FMath::Abs(InStartTile->GetGridPosition().Y - InTargetTile->GetGridPosition().Y);
	FScore.Add(InStartTile, DistX + DistY);

	CameFromMap.Empty();
	CameFromMap.Add(InStartTile, nullptr);

	FIntPoint Directions[4] = { FIntPoint(0, 1), FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(-1, 0) };

	while (OpenSet.Num() > 0)
	{
		// 1. Find the tile in OpenSet with the lowest FScore
		int32 BestIndex = 0;
		int32 LowestF = FScore.Contains(OpenSet[0]) ? FScore[OpenSet[0]] : 999999;

		for (int32 i = 1; i < OpenSet.Num(); ++i)
		{
			int32 TestF = FScore.Contains(OpenSet[i]) ? FScore[OpenSet[i]] : 999999;
			if (TestF < LowestF)
			{
				LowestF = TestF;
				BestIndex = i;
			}
		}

		ATile* Current = OpenSet[BestIndex];

		// 2. If target reached, reconstruct path backwards
		if (Current == InTargetTile)
		{
			ATile* Trace = Current;
			while (Trace != InStartTile)
			{
				FinalPath.Insert(Trace, 0);
				Trace = CameFromMap[Trace];
			}
			return FinalPath;
		}

		OpenSet.RemoveAt(BestIndex);
		ClosedSet.Add(Current);

		// 3. Analyze neighbors
		for (FIntPoint Dir : Directions)
		{
			FIntPoint NeighborCoord = Current->GetGridPosition() + Dir;
			if (ATile** FoundTilePtr = TileMap.Find(NeighborCoord))
			{
				ATile* Neighbor = *FoundTilePtr;

				if (!Neighbor->bIsWalkable || Neighbor->Status == ETileStatus::OBSTACLE || ClosedSet.Contains(Neighbor))
					continue;

				// Ignore tiles occupied by other units (unless it is the final target itself)
				if (Neighbor->UnitOnTile != nullptr && Neighbor != InTargetTile)
					continue;

				// AI Movement cost rule
				int32 ElevationDiff = Neighbor->Elevation - Current->Elevation;
				int32 StepCost = 1; // Base cost

				if (ElevationDiff > 0)
				{
					// Uphill penalty: costs twice the elevation difference
					StepCost = ElevationDiff * 2;
				}

				int32 TentativeG = GScore[Current] + StepCost;

				if (!OpenSet.Contains(Neighbor))
				{
					OpenSet.Add(Neighbor);
				}
				else if (TentativeG >= (GScore.Contains(Neighbor) ? GScore[Neighbor] : 999999))
				{
					continue; // Found a worse path, ignore
				}

				// Record the best path found so far
				CameFromMap.Add(Neighbor, Current);
				GScore.Add(Neighbor, TentativeG);

				int32 HScore = FMath::Abs(Neighbor->GetGridPosition().X - InTargetTile->GetGridPosition().X) +	FMath::Abs(Neighbor->GetGridPosition().Y - InTargetTile->GetGridPosition().Y);
				FScore.Add(Neighbor, TentativeG + HScore);
			}
		}
	}
	return FinalPath;
}

void AGameField::HighlightDeploymentZone()
{
	ClearHighlightedTiles();

	// Player valid deployment zone (Bottom 3 rows: X <= 2)
	for (auto& Elem : TileMap)
	{
		ATile* T = Elem.Value;
		if (T && T->IsValidLowLevel() && T->bIsWalkable && T->Status == ETileStatus::EMPTY && T->GetGridPosition().X <= 2)
		{
			T->OnSelectionChanged(true);
			HighlightedTiles.Add(T);
		}
	}
}

TArray<ATile*> AGameField::FindPathGreedy(ATile* InStartTile, ATile* InTargetTile)
{
	TArray<ATile*> FinalPath;
	if (!InStartTile || !InTargetTile) return FinalPath;

	// Algorithm: Greedy Best-First Search
	// Extremely fast heuristic algorithm that ignores terrain cost (G) and solely relies on distance (H)
	TArray<ATile*> OpenSet;
	TSet<ATile*> ClosedSet;

	OpenSet.Add(InStartTile);

	CameFromMap.Empty();
	CameFromMap.Add(InStartTile, nullptr);

	FIntPoint Directions[4] = { FIntPoint(0, 1), FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(-1, 0) };

	while (OpenSet.Num() > 0)
	{
		int32 BestIndex = 0;
		int32 LowestH = 999999;

		// Select the node with the absolute lowest HScore (closest to target)
		for (int32 i = 0; i < OpenSet.Num(); ++i)
		{
			int32 HScore = FMath::Abs(OpenSet[i]->GetGridPosition().X - InTargetTile->GetGridPosition().X) +
				FMath::Abs(OpenSet[i]->GetGridPosition().Y - InTargetTile->GetGridPosition().Y);

			if (HScore < LowestH)
			{
				LowestH = HScore;
				BestIndex = i;
			}
		}

		ATile* Current = OpenSet[BestIndex];

		if (Current == InTargetTile)
		{
			ATile* Trace = Current;
			while (Trace != InStartTile)
			{
				FinalPath.Insert(Trace, 0);
				Trace = CameFromMap[Trace];
			}
			return FinalPath;
		}

		OpenSet.RemoveAt(BestIndex);
		ClosedSet.Add(Current);

		for (FIntPoint Dir : Directions)
		{
			FIntPoint NeighborCoord = Current->GetGridPosition() + Dir;
			if (ATile** FoundTilePtr = TileMap.Find(NeighborCoord))
			{
				ATile* Neighbor = *FoundTilePtr;

				if (!Neighbor->bIsWalkable || Neighbor->Status == ETileStatus::OBSTACLE || ClosedSet.Contains(Neighbor))
					continue;

				if (Neighbor->UnitOnTile != nullptr && Neighbor != InTargetTile)
					continue;

				if (!ClosedSet.Contains(Neighbor) && !OpenSet.Contains(Neighbor))
				{
					OpenSet.Add(Neighbor);
					CameFromMap.Add(Neighbor, Current);
				}
			}
		}
	}
	return FinalPath;
}

bool AGameField::HasLineOfSight(ATile* InStartTile, ATile* InTargetTile)
{
	if (!InStartTile || !InTargetTile) return false;

	// ALGORITHM: Bresenham's Line Algorithm
	// Purpose: To find all the tiles crossed by a straight line between Point A and Point B,
	// using only integer arithmetic (no floating-point math) for maximum performance.

	// 1. Define the starting coordinates (X0, Y0) and the target coordinates (X1, Y1)
	int32 X0 = InStartTile->GetGridPosition().X;
	int32 Y0 = InStartTile->GetGridPosition().Y;
	int32 X1 = InTargetTile->GetGridPosition().X;
	int32 Y1 = InTargetTile->GetGridPosition().Y;

	// 2. Calculate the total absolute distance to cover on both axes
	int32 DX = FMath::Abs(X1 - X0);
	int32 DY = FMath::Abs(Y1 - Y0);

	// 3. Determine the direction of our "steps" (1 if moving forward/up, -1 if moving backward/down)
	int32 SX = (X0 < X1) ? 1 : -1;
	int32 SY = (Y0 < Y1) ? 1 : -1;

	// 4. The Error variable: this acts as an accumulator tracking the line's slope
	int32 Error = DX - DY;

	int32 StartElevation = InStartTile->Elevation;

	// 5. Start the loop: we walk tile by tile along the theoretical line
	while (true)
	{
		// Exclude the collision check for the very first tile (where the attacker is) 
		// and the very last one (where the target is)
		if ((X0 != InStartTile->GetGridPosition().X || Y0 != InStartTile->GetGridPosition().Y) &&
			(X0 != InTargetTile->GetGridPosition().X || Y0 != InTargetTile->GetGridPosition().Y))
		{
			// TRACKING IN ACTION! (X0, Y0) represents the tile where the laser currently is.
			FIntPoint CurrentPoint(X0, Y0);
			if (ATile** IntersectTilePtr = TileMap.Find(CurrentPoint))
			{
				ATile* IntersectTile = *IntersectTilePtr;

				// Blocking rule: The laser is stopped if the current tile is higher than the attacker's 
				// starting elevation, or if it's considered a physical Obstacle (e.g., Towers!)
				if (IntersectTile->Elevation > StartElevation || IntersectTile->Status == ETileStatus::OBSTACLE)
				{
					return false; // Obstacle found. Line of Sight is broken!
				}
			}
		}

		// If we reached the exact target coordinates, stop the loop.
		if (X0 == X1 && Y0 == Y1) break;

		// 6. Calculate the next mathematical step
		int32 E2 = 2 * Error;

		// Should we take a horizontal step?
		if (E2 > -DY) { Error -= DY; X0 += SX; }

		// Should we take a vertical step?
		if (E2 < DX) { Error += DX; Y0 += SY; }

		// (If both conditions are met simultaneously, we step diagonally!)
	}

	// If the loop finishes without ever hitting an obstacle, the line of sight is perfectly clear!
	return true;
}