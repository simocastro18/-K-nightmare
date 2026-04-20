#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "GameFramework/PlayerController.h"
#include "StrategyPlayerController.generated.h"

// FORWARD DECLARATIONS 
class ATile;
class AStrategyUnit;
class AGameField;
class UInputMappingContext;
class UInputAction;

UCLASS()
class STRATEGICOATURNI2025_API AStrategyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	// Grid Interaction 

	// Processes the logical consequence of clicking a specific tile on the grid
	UFUNCTION(BlueprintCallable, Category = "Grid Logic")
	void HandleTileClick(ATile* ClickedTile);

	// Triggers a raycast from the mouse cursor to interact with the 3D world
	UFUNCTION(BlueprintCallable, Category = "Grid Logic")
	void ExecuteClick();

	// Currently selected tile by the player
	UPROPERTY(BlueprintReadWrite, Category = "Grid Logic")
	ATile* CurrentSelectedTile;

	// Currently selected unit by the player, used to execute movement or attack commands
	UPROPERTY(BlueprintReadWrite, Category = "Grid Logic")
	AStrategyUnit* SelectedUnit;

	// Reference to the main grid manager for pathfinding and attack range calculations
	UPROPERTY(BlueprintReadWrite, Category = "Grid Logic")
	AGameField* GameFieldRef;

	// Input System (Enhanced Input)

	// The base mapping context containing the control scheme
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	// The specific input action bound to the primary mouse click
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* ClickAction;

	// Deployment Phase 

	// The blueprint class of the unit the player is currently holding to place on the grid
	UPROPERTY(BlueprintReadWrite, Category = "Deployment")
	TSubclassOf<AActor> ClassToSpawn;

	// Tracks if the player has successfully deployed their Brawler unit
	UPROPERTY(BlueprintReadWrite, Category = "Deployment")
	bool bBrawlerPlaced = false;

	// Tracks if the player has successfully deployed their Sniper unit
	UPROPERTY(BlueprintReadWrite, Category = "Deployment")
	bool bSniperPlaced = false;

protected:
	virtual void BeginPlay() override;

	// Binds specific Input Actions to C++ functions at runtime
	virtual void SetupInputComponent() override;
};