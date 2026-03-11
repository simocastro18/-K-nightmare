#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameField.generated.h"

// Struttura che definisce i dati della singola cella della griglia
USTRUCT(BlueprintType)
struct FGridCell
{
    GENERATED_BODY()

    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Data")
    int32 X = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Data")
    int32 Y = 0;

    // Livello di elevazione da 0 a 4
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Data")
    int32 Elevation = 0;

    // True se ci si può camminare, False se è livello 0 (acqua)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Data")
    bool bIsWalkable = false;
};

UCLASS()
class STRATEGICOATURNI2025_API AGameField : public AActor
{
    GENERATED_BODY()

public:
    // Costruttore di default
    AGameField();

    // Blueprint delle unità da assegnare nell'editor
    UPROPERTY(EditAnywhere, Category = "Game Logic|Units")
    TSubclassOf<AActor> BrawlerClass;

    UPROPERTY(EditAnywhere, Category = "Game Logic|Units")
    TSubclassOf<AActor> SniperClass;

    UPROPERTY(EditAnywhere, Category = "Game Logic|Units")
    TSubclassOf<AActor> TowerClass;

    // Funzione per spawnare le unità iniziali
    UFUNCTION(BlueprintCallable, Category = "Game Logic")
    void SpawnInitialEntities();

protected:
    virtual void BeginPlay() override;

public:

    // Mappa fissa oppure no
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
    bool bUseRandomSeed = true;

    // Parametri esposti ai Blueprint per essere configurati dal Widget
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
    int32 GridSizeX = 25;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
    int32 GridSizeY = 25;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
    float NoiseScale = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
    int32 MaxHeight = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
    int32 RandomSeed = 0;

    // Lo "slot" per inserire il nostro cubetto Blueprint
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
    TSubclassOf<AActor> CellBlueprint;

    // La dimensione del cubo in Unreal (di default un cubo base è 100x100)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
    float CellSize = 100.0f;

    // L'array che contiene tutte le 625 celle generate
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Data")
    TArray<FGridCell> GridData;

    // Funzione richiamabile dai Blueprint per generare la griglia
    UFUNCTION(BlueprintCallable, Category = "Map Generation")
    

    void GenerateGridData();

    // La TMap fondamentale per il Best Path (A*)
    // FVector2D sarà (X, Y), ATile* sarà il puntatore al cubetto spawnato
    UPROPERTY(Transient)
    TMap<FVector2D, class ATile*> TileMap;

    // Cambiamo AActor in ATile per sicurezza di tipo
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings")
    TSubclassOf<class ATile> TileClass;

    // Variabili per il Pathfinding (A*)
    UPROPERTY(BlueprintReadWrite, Category = "Pathfinding")
    class ATile* SelectedUnitTile;

    UPROPERTY(BlueprintReadWrite, Category = "Pathfinding")
    class ATile* TargetTile;

    private:
        // La funzione "Secchiello" che controlla se le celle sono tutte collegate
        bool IsMapFullyConnected();
};