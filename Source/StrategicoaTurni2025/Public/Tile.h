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

	// NUOVO: Le mesh per i percorsi e gli attacchi
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* PathMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* AttackMesh;

	// --- NUOVE VARIABILI PER I MATERIALI DEL PERCORSO ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Visuals")
	UMaterialInterface* PlayerPathMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Visuals")
	UMaterialInterface* AIPathMaterial;

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

	// NUOVO: Ci serve per sapere se la cella deve rimanere accesa dopo che il mouse se ne va
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile State")
	bool bIsCurrentlySelected = false;

	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetTileStatus(int32 NewOwner, ETileStatus NewStatus);

	void SetGridPosition(int32 InX, int32 InY);

	UFUNCTION(BlueprintCallable, Category = "Tile Data")
	FIntPoint GetGridPosition() const;

	UFUNCTION(BlueprintCallable, Category = "Tile")
	void SetUnitOnTile(AActor* NewUnit);

	// MODIFICATO: Non sono più BlueprintImplementableEvent, ora sono normali funzioni C++
	UFUNCTION(BlueprintCallable, Category = "Tile Visuals")
	void OnSelectionChanged(bool bIsSelected, bool bIsAI = false);

	UFUNCTION(BlueprintCallable, Category = "Tile Visuals")
	void UpdateAttackHighlight(bool bIsHighlighted);

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterial;

	UFUNCTION(BlueprintCallable, Category = "Tile Visuals")
	void UpdateTileColor();

protected:
	virtual void BeginPlay() override;

	// NUOVO: Sostituiscono i nodi del Mouse nel Blueprint
	//virtual void NotifyActorOnClicked(FKey ButtonPressed) override;
	virtual void NotifyActorBeginCursorOver() override;
	virtual void NotifyActorEndCursorOver() override;
};