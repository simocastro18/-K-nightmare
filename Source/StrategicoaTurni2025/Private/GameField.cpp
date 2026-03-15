#include "GameField.h"
#include "Tile.h"  
#include "StrategyUnit.h"
#include "Math/UnrealMathUtility.h"

AGameField::AGameField()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGameField::BeginPlay()
{
	Super::BeginPlay();
}

void AGameField::GenerateGridData()
{
	bool bIsMapValid = false;
	// PARACADUTE: Se le variabili sono a 0, diamo valori base di emergenza
	int32 SafeMaxHeight = (MaxHeight <= 0) ? 3 : MaxHeight;
	float SafeNoise = (NoiseScale <= 0.0f) ? 0.1f : NoiseScale;

	int32 Attempts = 0;

	while (!bIsMapValid && Attempts < 50) {
		Attempts++;
		float OffsetX = 0.0f;
		float OffsetY = 0.0f;

		if (bUseRandomSeed || Attempts > 1) {
			RandomSeed = FMath::RandRange(1, 999999);
			// Calcoliamo l'offset FUORI dal loop tenendolo piccolo per non far impazzire il Perlin
			OffsetX = (RandomSeed % 1000) * 10.0f;
			// Moltiplichiamo per 2 la Y così le coordinate non sono simmetriche
			OffsetY = ((RandomSeed * 2) % 1000) * 10.0f;
		}

		GridData.Empty();

		for (int32 Y = 0; Y < GridSizeY; ++Y)
		{
			for (int32 X = 0; X < GridSizeX; ++X)
			{
				FGridCell NewCell;
				NewCell.X = X;
				NewCell.Y = Y;

				// IL SEGRETO: Moltiplichiamo PRIMA per il noise, e POI aggiungiamo l'offset
				float SampleX = (X * SafeNoise) + OffsetX;
				float SampleY = (Y * SafeNoise) + OffsetY;

				float NoiseValue = FMath::PerlinNoise2D(FVector2D(SampleX, SampleY));

				// Portiamo il valore da (-1 a 1) a (0 a 1)
				float NormalizedNoise = (NoiseValue + 1.0f) / 2.0f;

				// Rendiamo i dislivelli più belli da vedere
				NormalizedNoise = FMath::Pow(NormalizedNoise, 1.8f);

				// 4. Stretch verso l'alto ? più celle raggiungono livelli 3-4 (arancione/rosso)
				NormalizedNoise = NormalizedNoise * 1.55f;

				NormalizedNoise = NormalizedNoise - 0.10f;

				// 5. Clamp obbligatorio
				NormalizedNoise = FMath::Clamp(NormalizedNoise, 0.0f, 1.0f);

				// Moltiplichiamo per l'altezza massima (SafeMaxHeight)
				NewCell.Elevation = FMath::RoundToInt(NormalizedNoise * SafeMaxHeight);
				NewCell.Elevation = FMath::Clamp(NewCell.Elevation, 0, SafeMaxHeight);

				// Se Elevation > 0 è terraferma (Walkable), altrimenti è Acqua
				NewCell.bIsWalkable = (NewCell.Elevation > 0);

				GridData.Add(NewCell);
			}
		}
		bIsMapValid = IsMapFullyConnected();
	}

	TileMap.Empty();

	for (const FGridCell& Cell : GridData)
	{
		FVector SpawnLocation = FVector(Cell.X * CellSize, Cell.Y * CellSize, Cell.Elevation * (CellSize / 2.0f));

		if (TileClass != nullptr)
		{
			ATile* NewTile = GetWorld()->SpawnActor<ATile>(TileClass, SpawnLocation, FRotator::ZeroRotator);

			if (NewTile)
			{
				NewTile->SetGridPosition(Cell.X, Cell.Y);
				NewTile->Elevation = Cell.Elevation;
				NewTile->bIsWalkable = Cell.bIsWalkable;

				if (!Cell.bIsWalkable) {
					NewTile->SetTileStatus(-1, ETileStatus::OBSTACLE);
				}

				// REGISTRIAMO NELLA MAPPA (Usando FIntPoint)
				TileMap.Add(FIntPoint(Cell.X, Cell.Y), NewTile);
				// AVVISIAMO IL BLUEPRINT CHE I DATI SONO PRONTI!
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

	return ReachedCells == TotalWalkableCells;
}

void AGameField::SpawnInitialEntities()
{
	if (TileMap.Num() == 0) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// ==========================================
	// FASE 1: PIAZZAMENTO TORRI (Orizzontali)
	// ==========================================

	auto SpawnTowerAtBestLocation = [&](FIntPoint TargetCoord) {
		ATile* BestTile = nullptr;
		float MinDistance = 999999.0f;

		for (auto& Elem : TileMap)
		{
			ATile* T = Elem.Value;
			if (T && T->IsValidLowLevel() && T->bIsWalkable && T->Status == ETileStatus::EMPTY)
			{
				// Qui convertiamo temporaneamente in float per usare la distanza 2D
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
				// Non setto UnitOnTile perché le torri sono ostacoli statici, non unità mobili
			}
		}
		};

	SpawnTowerAtBestLocation(FIntPoint(12, 12));
	SpawnTowerAtBestLocation(FIntPoint(12, 5));
	SpawnTowerAtBestLocation(FIntPoint(12, 19));


	// ==========================================
	// FASE 2: PIAZZAMENTO UNITA' (Alto / Basso)
	// ==========================================

	TArray<ATile*> PlayerZone;
	TArray<ATile*> AIZone;

	for (auto& Elem : TileMap)
	{
		ATile* T = Elem.Value;
		if (T && T->IsValidLowLevel() && T->bIsWalkable && T->Status == ETileStatus::EMPTY)
		{
			if (T->TileGridPosition.X <= 2)
			{
				PlayerZone.Add(T);
			}
			else if (T->TileGridPosition.X >= 22)
			{
				AIZone.Add(T);
			}
		}
	}

	auto SpawnUnitInZone = [&](TSubclassOf<AActor> LClass, TArray<ATile*>& Zone) {
		if (!LClass || Zone.Num() == 0) return;

		int32 Rnd = FMath::RandRange(0, Zone.Num() - 1);
		ATile* T = Zone[Rnd];

		FVector Loc = T->GetActorLocation() + FVector(0, 0, 100);
		AActor* NewUnit = GetWorld()->SpawnActor<AActor>(LClass, Loc, FRotator::ZeroRotator, SpawnParams);
		if (NewUnit)
		{
			T->SetTileStatus(-1, ETileStatus::OCCUPIED);
			T->SetUnitOnTile(NewUnit); // FONDAMENTALE: leghiamo l'unità alla cella
			AStrategyUnit* StrategyUnit = Cast<AStrategyUnit>(NewUnit);
			if (StrategyUnit)
			{
				StrategyUnit->InitializeUnit(0, T); // 0 = Umano (o metti la logica per l'AI), T = Cella
			}
			Zone.RemoveAt(Rnd);
		}
		};

	SpawnUnitInZone(BrawlerClass, PlayerZone);
	SpawnUnitInZone(SniperClass, PlayerZone);

	SpawnUnitInZone(BrawlerClass, AIZone);
	SpawnUnitInZone(SniperClass, AIZone);
}

void AGameField::ClearHighlightedTiles()
{
	// Cicla su tutte le celle che avevamo acceso e le spegne
	for (ATile* Tile : HighlightedTiles)
	{
		if (IsValid(Tile))
		{
			Tile->OnSelectionChanged(false); // Spegne la singola cella
		}
	}
	// Svuota la memoria
	HighlightedTiles.Empty();
}

void AGameField::HighlightReachableTiles(AStrategyUnit* SelectedUnit)
{
	ClearHighlightedTiles();

	// 1. CONTROLLI DI SICUREZZA CON LOG
	if (!IsValid(SelectedUnit))
	{
		UE_LOG(LogTemp, Error, TEXT("DIJKSTRA: SelectedUnit nullo o distrutto!"));
		return;
	}
	if (!IsValid(SelectedUnit->CurrentTile))
	{
		UE_LOG(LogTemp, Error, TEXT("DIJKSTRA: L'unita' %s non sa su quale cella si trova (CurrentTile nullo)! Controlla lo SpawnInitialEntities."), *SelectedUnit->UnitLogID);
		return;
	}

	ATile* StartTile = SelectedUnit->CurrentTile;
	int32 MaxRange = SelectedUnit->MovementRange;

	// LOG DI PARTENZA
	UE_LOG(LogTemp, Warning, TEXT("=== INIZIO DIJKSTRA ==="));
	UE_LOG(LogTemp, Warning, TEXT("Unita: %s | Partenza: X:%d Y:%d | Punti Movimento: %d"),
		*SelectedUnit->UnitLogID, StartTile->GetGridPosition().X, StartTile->GetGridPosition().Y, MaxRange);

	if (MaxRange <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("DIJKSTRA BLOCCATO: Il Movimento dell'unita e' %d. Modifica i Class Defaults nel Blueprint!"), MaxRange);
		return;
	}

	TMap<ATile*, int32> CostSoFar;
	TArray<ATile*> Frontier;

	CostSoFar.Add(StartTile, 0);
	Frontier.Add(StartTile);

	FIntPoint Directions[4] = {
		FIntPoint(0, 1), FIntPoint(0, -1),
		FIntPoint(1, 0), FIntPoint(-1, 0)
	};

	int32 CelleValideTrovate = 0; // Contatore per il debug

	while (Frontier.Num() > 0)
	{
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

		for (FIntPoint Dir : Directions)
		{
			FIntPoint NeighborCoord = Current->GetGridPosition() + Dir;

			if (ATile** FoundTilePtr = TileMap.Find(NeighborCoord))
			{
				ATile* Neighbor = *FoundTilePtr;

				if (IsValid(Neighbor) && Neighbor->bIsWalkable && Neighbor->Status != ETileStatus::OBSTACLE && !IsValid(Neighbor->UnitOnTile))
				{
					int32 StepCost = (Neighbor->Elevation > Current->Elevation) ? 2 : 1;
					int32 NewCost = CurrentCost + StepCost;

					if (NewCost <= MaxRange)
					{
						if (!CostSoFar.Contains(Neighbor) || NewCost < CostSoFar[Neighbor])
						{
							CostSoFar.Add(Neighbor, NewCost);
							Frontier.Add(Neighbor);
							CelleValideTrovate++;

							// LOG DI OGNI PASSO
							UE_LOG(LogTemp, Display, TEXT(" -> Espansione su X:%d Y:%d | Costo Totale: %d"), NeighborCoord.X, NeighborCoord.Y, NewCost);
						}
					}
				}
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("=== FINE DIJKSTRA: Trovate %d celle raggiungibili ==="), CelleValideTrovate);

	// PULIZIA DI SICUREZZA
	ClearHighlightedTiles();

	// ACCENDIAMO LE CELLE
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