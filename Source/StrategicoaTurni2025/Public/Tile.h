#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	// Dati della cella (Coordinate in griglia)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	FVector2D TileGridPosition;

	// Elevazione (0-4) passata dal GameField
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	int32 Elevation;

	// Se è camminabile (Terra) o no (Acqua)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	bool bIsWalkable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	ETileStatus Status;

	// ID del giocatore che occupa la cella (-1 se nessuno)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	int32 PlayerOwner;

	// Funzioni Setter e Getter (Stile professore)
	void SetTileStatus(int32 NewOwner, ETileStatus NewStatus);
	void SetGridPosition(double InX, double InY);

	UFUNCTION(BlueprintCallable, Category = "Tile Data")
	FVector2D GetGridPosition() const;

protected:
	virtual void BeginPlay() override;

};