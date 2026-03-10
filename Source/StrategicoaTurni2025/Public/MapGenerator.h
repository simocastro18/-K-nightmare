#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapGenerator.generated.h"

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
class STRATEGICOATURNI2025_API AMapGenerator : public AActor
{
    GENERATED_BODY()

public:
    // Costruttore di default
    AMapGenerator();

protected:
    virtual void BeginPlay() override;

public:
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
};