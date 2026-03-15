#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StrategyUnit.h"
#include "Tile.generated.h"

// Enum per lo stato della cella (vuota, occupata da unità o ostacolo/acqua)
UENUM(BlueprintType)
enum class ETileStatus : uint8
{
	EMPTY      UMETA(DisplayName = "Empty"),
	OCCUPIED   UMETA(DisplayName = "Occupied"),
	OBSTACLE   UMETA(DisplayName = "Obstacle")
};

UCLASS()
class STRATEGICOATURNI2025_API ATile : public AActor
{
	GENERATED_BODY()

public:
	ATile();

	// Componenti base per la visualizzazione
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* TileMesh;

	// Dati della cella (Coordinate in griglia) - CAMBIATO IN FIntPoint
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	FIntPoint TileGridPosition;

	//ricalcolo posizione per blueprint
	UFUNCTION(BlueprintImplementableEvent, Category = "Tile")
	void OnTileDataInitialized();

	// Elevazione (0-4) passata dal GameField
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	int32 Elevation;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Tile Data")
	AActor* UnitOnTile;

	// Se è camminabile (Terra) o no (Acqua)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	bool bIsWalkable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	ETileStatus Status;

	// ID del giocatore che occupa la cella (-1 se nessuno)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	int32 PlayerOwner;

	// Funzioni Setter e Getter (Stile professore)
	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetTileStatus(int32 NewOwner, ETileStatus NewStatus);

	// CAMBIATO IN int32
	void SetGridPosition(int32 InX, int32 InY);

	// CAMBIATO IN FIntPoint
	UFUNCTION(BlueprintCallable, Category = "Tile Data")
	FIntPoint GetGridPosition() const;

	// Funzione per modificare l'unità sopra
	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetUnitOnTile(AActor* NewUnit);

	// Evento implementato nel Blueprint per accendere/spegnere la luce
	UFUNCTION(BlueprintImplementableEvent, Category = "Tile Visuals")
	void OnSelectionChanged(bool bIsSelected);

protected:
	virtual void BeginPlay() override;
};