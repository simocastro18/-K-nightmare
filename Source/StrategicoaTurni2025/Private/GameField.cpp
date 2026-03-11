#include "GameField.h"
#include "Tile.h"  
#include "Math/UnrealMathUtility.h"

AGameField::AGameField()
{
    // Disattiviamo il Tick, non ci serve aggiornare la mappa ogni singolo frame
    PrimaryActorTick.bCanEverTick = false;
}

void AGameField::BeginPlay()
{
    Super::BeginPlay();

    // Non chiamiamo GenerateGridData() qui, perché le specifiche chiedono 
    // di configurarla tramite un widget all'avvio della partita.
    // Richiamiamo la funzione che genera i cubi appena premiamo Play!
    //GenerateGridData();
    
}

void AGameField::GenerateGridData()
{
    bool bIsMapValid = false;

    // --- FASE 1: GENERAZIONE E CONTROLLO ---
    // Continua a generare finché non trova una mappa senza isole irraggiungibili!
    while (!bIsMapValid) {
        if (bUseRandomSeed) {
            RandomSeed = FMath::RandRange(1, 999999);
        }
        // Svuotiamo l'array nel caso venisse generata una nuova mappa
        GridData.Empty();

        // Doppio ciclo per creare la griglia X e Y
        for (int32 Y = 0; Y < GridSizeY; ++Y)
        {
            for (int32 X = 0; X < GridSizeX; ++X)
            {
                FGridCell NewCell;
                NewCell.X = X;
                NewCell.Y = Y;

                // Prepariamo le coordinate per il Perlin Noise, aggiungendo il Seed
                float SampleX = (X + RandomSeed) * NoiseScale;
                float SampleY = (Y + RandomSeed) * NoiseScale;

                // FMath::PerlinNoise2D restituisce un valore tra circa -1.0 e 1.0. 
                // Lo normalizziamo per averlo tra 0.0 e 1.0.
                float NoiseValue = FMath::PerlinNoise2D(FVector2D(SampleX, SampleY));

                // 1. Normalizziamo tra 0 e 1( rapporto terra acqua)
                float NormalizedNoise = (NoiseValue + 1.0f) / 2.0f;

                // 2. POTENZA: Usiamo 1.5 invece di 2.2. (curvatura del terreno)
                // Renderà le pianure ampie ma lascerà spazio a colline gialle e arancioni.
                NormalizedNoise = FMath::Pow(NormalizedNoise, 1.8f);

                // 3. OFFSET ACQUA: Usiamo -0.08 invece di -0.15.
                // Questo creerà piccoli laghi e insenature senza sommergere mezza mappa.
                NormalizedNoise = NormalizedNoise - 0.14f;

                // 4. Protezione obbligatoria
                NormalizedNoise = FMath::Clamp(NormalizedNoise, 0.0f, 1.0f);

                NormalizedNoise = NormalizedNoise * 1.55f;

                // Quantizziamo in gradini discreti da 0 a MaxHeight (es. 5 livelli)
                NewCell.Elevation = FMath::RoundToInt(NormalizedNoise * MaxHeight);

                // Per sicurezza, ci assicuriamo che il valore non esca dai limiti [0, 4]
                NewCell.Elevation = FMath::Clamp(NewCell.Elevation, 0, MaxHeight);

                // Il livello 0 è acqua ed è un ostacolo non calpestabile
                NewCell.bIsWalkable = (NewCell.Elevation > 0);

                // Aggiungiamo la cella finita al nostro Array
                GridData.Add(NewCell);


            }
        }
        bIsMapValid = IsMapFullyConnected();
    }

    // --- SPAWN DEGLI ATTORI INTELLIGENTI ---
    // Svuotiamo la mappa precedente se esiste
    TileMap.Empty();

    for (const FGridCell& Cell : GridData)
    {
        // Calcoliamo la posizione (Z leggermente alzata per l'elevazione)
        FVector SpawnLocation = FVector(Cell.X * CellSize, Cell.Y * CellSize, Cell.Elevation * (CellSize / 2.0f));

        if (TileClass != nullptr)
        {
            // Spawnamo la nostra classe ATile invece di un Actor generico
            ATile* NewTile = GetWorld()->SpawnActor<ATile>(TileClass, SpawnLocation, FRotator::ZeroRotator);

            if (NewTile)
            {
                // Diamo alla cella la sua identità
                NewTile->SetGridPosition(Cell.X, Cell.Y);
                NewTile->Elevation = Cell.Elevation;
                NewTile->bIsWalkable = Cell.bIsWalkable;

                // Se è acqua, impostiamo lo status come ostacolo
                if (!Cell.bIsWalkable) {
                    NewTile->SetTileStatus(-1, ETileStatus::OBSTACLE);
                }

                // REGISTRIAMO NELLA MAPPA (Stile professore)
                TileMap.Add(FVector2D(Cell.X, Cell.Y), NewTile);
            }
        }
    }


}
//logica per evitare blocchi non raggiungibili
bool AGameField::IsMapFullyConnected()
{
    int32 TotalWalkableCells = 0;
    int32 StartX = -1;
    int32 StartY = -1;

    // 1. Contiamo quanta terra calpestabile c'è in totale
    for (const FGridCell& Cell : GridData)
    {
        if (Cell.bIsWalkable)
        {
            TotalWalkableCells++;
            // Salviamo le coordinate del primo pezzo di terra che troviamo per buttare il secchiello
            if (StartX == -1)
            {
                StartX = Cell.X;
                StartY = Cell.Y;
            }
        }
    }

    // Se la mappa è un oceano totale (0 terra), scartiamola a prescindere
    if (TotalWalkableCells == 0) return false;

    // 2. Prepariamo il secchiello
    TArray<bool> Visited;
    Visited.Init(false, GridSizeX * GridSizeY); // Una lista per ricordarci dove siamo già passati

    TArray<FIntPoint> Queue; // La coda delle celle da esplorare
    int32 QueueIndex = 0;

    Queue.Add(FIntPoint(StartX, StartY));
    Visited[StartY * GridSizeX + StartX] = true;

    int32 ReachedCells = 0;

    // 3. L'Inondazione (Espansione a macchia d'olio)
    while (QueueIndex < Queue.Num())
    {
        FIntPoint Current = Queue[QueueIndex];
        QueueIndex++;
        ReachedCells++;

        // Controlliamo i 4 vicini (Su, Giù, Sinistra, Destra)
        FIntPoint Neighbors[4] = {
            FIntPoint(Current.X, Current.Y + 1),
            FIntPoint(Current.X, Current.Y - 1),
            FIntPoint(Current.X - 1, Current.Y),
            FIntPoint(Current.X + 1, Current.Y)
        };

        for (FIntPoint N : Neighbors)
        {
            // Se il vicino è dentro i confini della griglia...
            if (N.X >= 0 && N.X < GridSizeX && N.Y >= 0 && N.Y < GridSizeY)
            {
                int32 Index = N.Y * GridSizeX + N.X;

                // Se non ci siamo ancora passati E se è un pezzo di terra calpestabile...
                if (!Visited[Index] && GridData[Index].bIsWalkable)
                {
                    Visited[Index] = true; // Lo marchiamo
                    Queue.Add(N);          // Lo aggiungiamo alla coda per esplorare i suoi vicini
                }
            }
        }
    }

    // 4. Il Verdetto finale
    // Se le celle bagnate dal secchiello sono uguali al totale della terra, la mappa è perfetta!
    return ReachedCells == TotalWalkableCells;
}

void AGameField::SpawnInitialEntities()
{
    // PROTEZIONE 1: Se non ci sono celle, esci subito
    if (TileMap.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Spawn fallito: TileMap vuota!"));
        return;
    }

    TArray<ATile*> WalkableTiles;
    for (auto& Elem : TileMap)
    {
        // PROTEZIONE 2: Verifica che il puntatore alla cella sia valido
        if (Elem.Value && Elem.Value->bIsWalkable)
        {
            WalkableTiles.Add(Elem.Value);
        }
    }

    if (WalkableTiles.Num() == 0) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    auto SpawnAtRandom = [&](TSubclassOf<AActor> ClassToSpawn, ETileStatus NewStatus) {
        // PROTEZIONE 3: Se non hai assegnato il Blueprint nello slot, esci senza crashare
        if (!ClassToSpawn)
        {
            UE_LOG(LogTemp, Warning, TEXT("Uno slot unita' e' vuoto nel Blueprint!"));
            return;
        }

        int32 RandomIndex = FMath::RandRange(0, WalkableTiles.Num() - 1);
        ATile* TargetTile = WalkableTiles[RandomIndex];

        if (TargetTile)
        {
            FVector Location = TargetTile->GetActorLocation() + FVector(0, 0, 100);
            GetWorld()->SpawnActor<AActor>(ClassToSpawn, Location, FRotator::ZeroRotator, SpawnParams);
            TargetTile->SetTileStatus(-1, NewStatus);
            WalkableTiles.RemoveAt(RandomIndex);
        }
        };

    SpawnAtRandom(BrawlerClass, ETileStatus::OCCUPIED);
    SpawnAtRandom(SniperClass, ETileStatus::OCCUPIED);

    for (int32 i = 0; i < 3; i++)
    {
        SpawnAtRandom(TowerClass, ETileStatus::OBSTACLE);
    }
}