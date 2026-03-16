#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
// RIMOSSO: #include "StrategyUnit.h" -> Era questo a causare la dipendenza circolare!
#include "Tile.generated.h"

// Enum per lo stato della cella (Spazi invisibili rimossi)
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* TileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	FIntPoint TileGridPosition;

	UFUNCTION(BlueprintImplementableEvent, Category = "Tile")
	void OnTileDataInitialized();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	int32 Elevation;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Tile Data")
	AActor* UnitOnTile;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	bool bIsWalkable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	ETileStatus Status;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	int32 PlayerOwner;

	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetTileStatus(int32 NewOwner, ETileStatus NewStatus);

	void SetGridPosition(int32 InX, int32 InY);

	UFUNCTION(BlueprintCallable, Category = "Tile Data")
	FIntPoint GetGridPosition() const;

	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetUnitOnTile(AActor* NewUnit);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tile Visuals")
	void OnSelectionChanged(bool bIsSelected);

	UFUNCTION(BlueprintImplementableEvent, Category = "Tile Visuals")
	void UpdateAttackHighlight(bool bIsHighlighted);

protected:
	virtual void BeginPlay() override;
};