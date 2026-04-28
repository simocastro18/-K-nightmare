#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tile.generated.h"

// Forward declarations 
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Defines the current occupation status of the tile
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

	// Components

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* TileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* PathMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* AttackMesh;

	// Tile visuals

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Visuals")
	UMaterialInterface* PlayerPathMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Visuals")
	UMaterialInterface* AIPathMaterial;

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterial;

	// Tile data

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	FIntPoint TileGridPosition;

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

	// Tracks if the tile is currently highlighted to maintain visibility after mouse exit
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile State")
	bool bIsCurrentlySelected = false;

	// Tile logic 

	UFUNCTION(BlueprintImplementableEvent, Category = "Tile")
	void OnTileDataInitialized();

	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetTileStatus(int32 NewOwner, ETileStatus NewStatus);

	void SetGridPosition(int32 InX, int32 InY);

	UFUNCTION(BlueprintCallable, Category = "Tile Data")
	FIntPoint GetGridPosition() const;

	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetUnitOnTile(AActor* NewUnit);

	// Visual highlights 

	UFUNCTION(BlueprintCallable, Category = "Tile Visuals")
	void OnSelectionChanged(bool bIsSelected, bool bIsAI = false);

	UFUNCTION(BlueprintCallable, Category = "Tile Visuals")
	void UpdateAttackHighlight(bool bIsHighlighted);

	UFUNCTION(BlueprintCallable, Category = "Tile Visuals")
	void UpdateTileColor();

protected:
	virtual void BeginPlay() override;

	// Hardware input events for mouse hover states
	virtual void NotifyActorBeginCursorOver() override;
	virtual void NotifyActorEndCursorOver() override;
};