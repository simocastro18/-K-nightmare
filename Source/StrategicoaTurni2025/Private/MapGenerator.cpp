#include "MapGenerator.h"
#include "Math/UnrealMathUtility.h"

AMapGenerator::AMapGenerator()
{
    // Disattiviamo il Tick, non ci serve aggiornare la mappa ogni singolo frame
    PrimaryActorTick.bCanEverTick = false;
}

void AMapGenerator::BeginPlay()
{
    Super::BeginPlay();
    // Non chiamiamo GenerateGridData() qui, perché le specifiche chiedono 
    // di configurarla tramite un widget all'avvio della partita.
    // Richiamiamo la funzione che genera i cubi appena premiamo Play!
    GenerateGridData();
    
}

void AMapGenerator::GenerateGridData()
{
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
        
            // 1. Normalizziamo tra 0 e 1
            float NormalizedNoise = (NoiseValue + 1.0f) / 2.0f;

            // 2. POTENZA: Usiamo 1.5 invece di 2.2. 
            // Renderà le pianure ampie ma lascerà spazio a colline gialle e arancioni.
            NormalizedNoise = FMath::Pow(NormalizedNoise, 1.5f);

            // 3. OFFSET ACQUA: Usiamo -0.08 invece di -0.15.
            // Questo creerà piccoli laghi e insenature senza sommergere mezza mappa.
            NormalizedNoise = NormalizedNoise - 0.08f;

            // 4. Protezione obbligatoria
            NormalizedNoise = FMath::Clamp(NormalizedNoise, 0.0f, 1.0f);


            // Quantizziamo in gradini discreti da 0 a MaxHeight (es. 5 livelli)
            NewCell.Elevation = FMath::RoundToInt(NormalizedNoise * MaxHeight);

            // Per sicurezza, ci assicuriamo che il valore non esca dai limiti [0, 4]
            NewCell.Elevation = FMath::Clamp(NewCell.Elevation, 0, MaxHeight);

            // Il livello 0 è acqua ed è un ostacolo non calpestabile
            NewCell.bIsWalkable = (NewCell.Elevation > 0);

            // Aggiungiamo la cella finita al nostro Array
            GridData.Add(NewCell);

            // --- SPAWN ---

            // Calcoliamo la posizione 3D del cubo nel mondo. 
            // Moltiplichiamo X e Y per la grandezza del cubo. 
            // L'altezza (Z) la alziamo in base all'Elevation.
            FVector SpawnLocation = FVector(X * CellSize, Y * CellSize, NewCell.Elevation * (CellSize / 2.0f));

            // Il cubo non ha rotazione
            FRotator SpawnRotation = FRotator::ZeroRotator;

            // Se abbiamo assegnato il Blueprint dall'editor, spawniamo il cubo!
            if (CellBlueprint != nullptr)
            {
                GetWorld()->SpawnActor<AActor>(CellBlueprint, SpawnLocation, SpawnRotation);
            }

        }
    }
}