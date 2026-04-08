#include "GameField.h"
#include "Tile.h"  
#include "StrategyUnit.h"
#include "StrategyTower.h"
#include "Math/UnrealMathUtility.h"
#include "Kismet/GameplayStatics.h"
//debug
//#include "DrawDebugHelpers.h"

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
	int32 SafeMaxHeight = (MaxHeight <= 0) ? 3 : MaxHeight;
	float SafeNoise = (NoiseScale <= 0.0f) ? 0.1f : NoiseScale;
	int32 Attempts = 0;

	while (!bIsMapValid && Attempts < 50) {
		Attempts++;
		float OffsetX = 0.0f;
		float OffsetY = 0.0f;

		if (bUseRandomSeed || Attempts > 1) {
			RandomSeed = FMath::RandRange(1, 999999);
			OffsetX = (RandomSeed % 1000) * 10.0f;
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

				float SampleX = (X * SafeNoise) + OffsetX;
				float SampleY = (Y * SafeNoise) + OffsetY;
				float NoiseValue = FMath::PerlinNoise2D(FVector2D(SampleX, SampleY));

				float NormalizedNoise = (NoiseValue + 1.0f) / 2.0f;
				NormalizedNoise = FMath::Pow(NormalizedNoise, 1.8f);
				NormalizedNoise = NormalizedNoise * 1.55f;
				NormalizedNoise = NormalizedNoise - 0.10f;
				NormalizedNoise = FMath::Clamp(NormalizedNoise, 0.0f, 1.0f);

				NewCell.Elevation = FMath::RoundToInt(NormalizedNoise * SafeMaxHeight);
				NewCell.Elevation = FMath::Clamp(NewCell.Elevation, 0, SafeMaxHeight);
				NewCell.bIsWalkable = (NewCell.Elevation > 0);

				GridData.Add(NewCell);
			}
		}
		bIsMapValid = IsMapFullyConnected();
	}

	TileMap.Empty();

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

				if (!Cell.bIsWalkable) {
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

	// FASE 1: PIAZZAMENTO TORRI
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
				BestTile->SetTileStatus(-1, ETileStatus::OBSTACLE); // Ostacolo

				// AGGIUNTA: Diciamo alla torre dove si trova esattamente!
				AStrategyTower* StratTower = Cast<AStrategyTower>(NewTower);
				if (StratTower)
				{
					StratTower->GridPosition = BestTile->GetGridPosition();
				}
			}
		}
		};

	SpawnTowerAtBestLocation(FIntPoint(12, 12));
	SpawnTowerAtBestLocation(FIntPoint(12, 5));
	SpawnTowerAtBestLocation(FIntPoint(12, 19));

	// FASE 2: PIAZZAMENTO UNITA' 
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
			else if (T->TileGridPosition.X >= GridSizeX - 3)
			{
				AIZone.Add(T);
			}
		}
	}

// Funzione Lambda aggiornata per assegnare Team, LogID e Yaw per la rotazione!
auto SpawnUnitInZone = [&](TSubclassOf<AActor> LClass, TArray<ATile*>& Zone, ETeam AssignedTeam, FString UnitLogID, float SpawnYaw) {
	if (!LClass || Zone.Num() == 0) return;

	int32 Rnd = FMath::RandRange(0, Zone.Num() - 1);
	ATile* T = Zone[Rnd];

	// Calcoliamo la posizione in altezza
	FVector Loc = T->GetActorLocation() + FVector(0, 0, 100);

	FVector SpawnScale(0.5f, 0.5f, 0.5f);
	FTransform SpawnTransform(FRotator(0.0f, SpawnYaw, 0.0f), Loc, SpawnScale);

	// 2. SPAWN RITARDATO
	AActor* NewUnit = GetWorld()->SpawnActorDeferred<AActor>(LClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (NewUnit)
	{
		T->SetTileStatus(-1, ETileStatus::OCCUPIED);
		T->SetUnitOnTile(NewUnit);

		AStrategyUnit* StrategyUnit = Cast<AStrategyUnit>(NewUnit);
		if (StrategyUnit)
		{
			// Settiamo solo il riferimento alla mappa prima di finire lo spawn
			StrategyUnit->GameFieldRef = this;
		}

		// 3. COMPLETAMO LO SPAWN: Qui Unreal crea fisicamente il modello 3D e la barra della vita!
		UGameplayStatics::FinishSpawningActor(NewUnit, SpawnTransform);

		// 4. ORA CHE ESISTE TUTTO, LO INIZIALIZZIAMO (La HealthBar ora esiste e si colorerà!)
		if (StrategyUnit)
		{
			StrategyUnit->InitializeUnit(UnitLogID, AssignedTeam, SpawnYaw, T);
		}

		Zone.RemoveAt(Rnd);
	}
	}; // <-- Qui finisce la Lambda

// --- SCHIERAMENTO PLAYER (Guarda in alto, Yaw = 0.0f) ---
// --- SCHIERAMENTO PLAYER ---
SpawnUnitInZone(BrawlerClass, PlayerZone, ETeam::Player, TEXT("Brawler_P"), -90.0f);
SpawnUnitInZone(SniperClass, PlayerZone, ETeam::Player, TEXT("Sniper_P"), -90.0f);

// --- SCHIERAMENTO AI ---
SpawnUnitInZone(BrawlerClass, AIZone, ETeam::AI, TEXT("Brawler_AI"), 90.0f); // o 270.0f
SpawnUnitInZone(SniperClass, AIZone, ETeam::AI, TEXT("Sniper_AI"), 90.0f);
}

void AGameField::ClearHighlightedTiles()
{

	// debug
	UE_LOG(LogTemp, Error, TEXT("STO SPEGNENDO LE LUCI ROSSE DEI BERSAGLI!"));


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
							CameFromMap.Add(Neighbor, Current);
							Frontier.Add(Neighbor);
						}
					}
				}
			}
		}
	}

	ClearHighlightedTiles();

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

	while (IsValid(CurrentTile) && SafetyCounter < 1000)
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

	int32 NumElements = Path.Num();
	for (int32 i = 0; i < NumElements / 2; ++i)
	{
		Path.Swap(i, NumElements - 1 - i);
	}

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

	for (int32 dx = -Range; dx <= Range; ++dx)
	{
		for (int32 dy = -Range; dy <= Range; ++dy)
		{
			if (FMath::Abs(dx) + FMath::Abs(dy) <= Range)
			{
				FIntPoint CheckPos(StartPos.X + dx, StartPos.Y + dy);

				if (ATile** FoundTilePtr = TileMap.Find(CheckPos))
				{
					ATile* CheckTile = *FoundTilePtr;

					if (IsValid(CheckTile) && CheckTile != AttackingUnit->CurrentTile)
					{
						// CONTROLLO BERSAGLIO E FUOCO AMICO DEFINITIVO
						if (IsValid(CheckTile->UnitOnTile))
						{
							AStrategyUnit* TargetUnit = Cast<AStrategyUnit>(CheckTile->UnitOnTile);

							// 1. L'unità deve esistere e NON deve essere del mio stesso team
							if (IsValid(TargetUnit) && TargetUnit->UnitTeam != AttackingUnit->UnitTeam)
							{
								// 2. REGOLA DELL'ALTEZZA: Posso attaccare solo se il bersaglio è più in basso o al mio stesso livello
								if (CheckTile->Elevation <= AttackingUnit->CurrentTile->Elevation)
								{
									UE_LOG(LogTemp, Error, TEXT("ACCENDO LUCE ROSSA SU NEMICO: %s in X:%d Y:%d"), *TargetUnit->UnitLogID, CheckTile->GetGridPosition().X, CheckTile->GetGridPosition().Y);
									CheckTile->UpdateAttackHighlight(true);
									AttackableTiles.Add(CheckTile);
								}
								else
								{
									// Log opzionale per capire perché la cella non si è accesa
									UE_LOG(LogTemp, Warning, TEXT("BERSAGLIO %s TROPPO IN ALTO! (Altezza Attaccante: %d, Altezza Bersaglio: %d)"),
										*TargetUnit->UnitLogID, AttackingUnit->CurrentTile->Elevation, CheckTile->Elevation);
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

	TArray<ATile*> OpenSet;
	TSet<ATile*> ClosedSet;

	TMap<ATile*, int32> GScore; // Costo dal punto di partenza
	TMap<ATile*, int32> FScore; // GScore + Euristica (Distanza Stimata)

	// Inizializziamo il punto di partenza
	OpenSet.Add(InStartTile);
	GScore.Add(InStartTile, 0);

	// L'Euristica (Distanza Manhattan: contiamo i passi in orizzontale e verticale)
	int32 DistX = FMath::Abs(InStartTile->GetGridPosition().X - InTargetTile->GetGridPosition().X);
	int32 DistY = FMath::Abs(InStartTile->GetGridPosition().Y - InTargetTile->GetGridPosition().Y);
	FScore.Add(InStartTile, DistX + DistY);

	// Puliamo la mappa globale così il movimento del tuo gioco funzionerà in automatico!
	CameFromMap.Empty();
	CameFromMap.Add(InStartTile, nullptr);

	FIntPoint Directions[4] = { FIntPoint(0, 1), FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(-1, 0) };

	while (OpenSet.Num() > 0)
	{
		// 1. Trova la cella nell'OpenSet con l'FScore più basso
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

		// 2. Se siamo arrivati al bersaglio, ricostruiamo il percorso al contrario
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

		// 3. Spostiamo la cella corrente tra quelle già analizzate
		OpenSet.RemoveAt(BestIndex);
		ClosedSet.Add(Current);

		// 4. Analizziamo i vicini
		for (FIntPoint Dir : Directions)
		{
			FIntPoint NeighborCoord = Current->GetGridPosition() + Dir;
			if (ATile** FoundTilePtr = TileMap.Find(NeighborCoord))
			{
				ATile* Neighbor = *FoundTilePtr;

				// Ignora ostacoli e celle già chiuse
				if (!Neighbor->bIsWalkable || Neighbor->Status == ETileStatus::OBSTACLE || ClosedSet.Contains(Neighbor))
					continue;

				// Ignora i blocchi occupati da altre unità (Tranne se è il bersaglio stesso!)
				if (Neighbor->UnitOnTile != nullptr && Neighbor != InTargetTile)
					continue;

				// Calcolo costo del dislivello (La tua regola dell'Elevation!)
				int32 StepCost = (Neighbor->Elevation > Current->Elevation) ? 2 : 1;
				int32 TentativeG = GScore[Current] + StepCost;

				if (!OpenSet.Contains(Neighbor))
				{
					OpenSet.Add(Neighbor);
				}
				else if (TentativeG >= (GScore.Contains(Neighbor) ? GScore[Neighbor] : 999999))
				{
					continue; // Abbiamo trovato una strada peggiore, ignoriamola
				}

				// Se arriviamo qui, abbiamo trovato la strada migliore per questo vicino!
				CameFromMap.Add(Neighbor, Current);
				GScore.Add(Neighbor, TentativeG);

				int32 HScore = FMath::Abs(Neighbor->GetGridPosition().X - InTargetTile->GetGridPosition().X) +
					FMath::Abs(Neighbor->GetGridPosition().Y - InTargetTile->GetGridPosition().Y);

				FScore.Add(Neighbor, TentativeG + HScore);
			}
		}
	}
	return FinalPath;
}