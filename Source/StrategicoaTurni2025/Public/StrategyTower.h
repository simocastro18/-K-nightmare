#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StrategyTower.generated.h"

// FORWARD DECLARATIONS
class UStaticMeshComponent;
class UMaterialInterface;

// Defines the finite state machine for tower control logic
UENUM(BlueprintType)
enum class ETowerState : uint8
{
	Neutral          UMETA(DisplayName = "Neutral"),
	ControlledPlayer UMETA(DisplayName = "Controlled Player"),
	ControlledAI     UMETA(DisplayName = "Controlled AI"),
	Contested        UMETA(DisplayName = "Contested")
};

UCLASS(Blueprintable)
class STRATEGICOATURNI2025_API AStrategyTower : public AActor
{
	GENERATED_BODY()

public:
	AStrategyTower();

	// The physical 3D representation of the tower
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* TowerMesh;

	// STATE MATERIALS

	UPROPERTY(EditDefaultsOnly, Category = "Tower Visuals")
	UMaterialInterface* NeutralMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Tower Visuals")
	UMaterialInterface* PlayerMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Tower Visuals")
	UMaterialInterface* AIMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Tower Visuals")
	UMaterialInterface* ContestedMaterial;

	// Updates the tower's material based on the current control state
	UFUNCTION(BlueprintCallable, Category = "Tower")
	void UpdateTowerVisuals(ETowerState NewState);

	// The current ownership state evaluated at the end of each turn
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tower")
	ETowerState CurrentState;

	// The 2D logical coordinate of the tower on the grid
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tower")
	FIntPoint GridPosition;

protected:
	virtual void BeginPlay() override;
};