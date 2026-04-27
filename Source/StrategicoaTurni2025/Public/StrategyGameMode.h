#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StrategyTower.h"
#include "StrategyUnit.h" // Required for the ETeam enum
#include "StrategyGameMode.generated.h"

// Used to prevent circular dependencies between header files
class AGameField;
class UUserWidget;

// Defines the pathfinding algorithm utilized by the AI during the match.
UENUM(BlueprintType)
enum class EAIAlgorithm : uint8
{
	AStar  UMETA(DisplayName = "A*"),
	Greedy UMETA(DisplayName = "Greedy")
};

// Configuration struct passed from the main menu to initialize the match settings.
USTRUCT(BlueprintType)
struct FGameConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float NoiseScale = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int32 GridSizeX = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int32 GridSizeY = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	EAIAlgorithm SelectedAIAlgorithm = EAIAlgorithm::AStar;
};

// Represents the current phase of the core game loop.
UENUM(BlueprintType)
enum class ETurnState : uint8
{
	Deployment UMETA(DisplayName = "Deployment Phase"),
	PlayerTurn UMETA(DisplayName = "Player Turn"),
	AITurn     UMETA(DisplayName = "AI Turn")
};

// Delegate used to broadcast game events and combat actions to the UI Log Widget
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameLogAdded, const FString&, LogMessage);

// Define the Delegate type (No parameters needed, the UI will fetch data directly)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUIUpdateRequired);

UCLASS()
class STRATEGICOATURNI2025_API AStrategyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> WinWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> LoseWidgetClass;

	// Halts gameplay and displays the appropriate victory or defeat screen.
	void HandleGameOver(ETeam Winner);

	// Reloads the current level to start a new match.
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void RestartGame();

	// Initializes the map and the game rules based on the user's menu selection.
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void StartGameWithConfig(FGameConfig Config);

	// Reference to the grid generator and spatial manager
	UPROPERTY(BlueprintReadOnly, Category = "Game Flow")
	AGameField* MapGenerator;

	// Concludes the current player's or AI's turn and passes control to the opponent.
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void EndTurn();

	ETurnState GetCurrentTurnState() const { return CurrentTurnState; }

	// Verifies if the active team has any units left that can perform an action.
	void CheckRemainingMoves();

	// Analyzes tower capture zones and updates dominance counters for win conditions.
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void EvaluateTowers();

	// Triggers the execution sequence for the enemy units.
	UFUNCTION(BlueprintCallable, Category = "AI")
	void ProcessAITurn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	int32 PlayerDominanceTurns = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	int32 AIDominanceTurns = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	int32 PlayerTowerCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	int32 AITowerCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	bool bIsGameOver = false;

	UPROPERTY(BlueprintAssignable, Category = "UI Log")
	FOnGameLogAdded OnGameLogAdded;

	// Formats and pushes a new string to the UI Combat Log.
	UFUNCTION(BlueprintCallable, Category = "UI Log")
	void AddGameLog(const FString& Message);


	UPROPERTY(BlueprintReadOnly, Category = "Game State")
	int32 CurrentTurnNumber = 1;

	// Converts raw Grid (X,Y) coordinates into a readable format (e.g., "A5").
	UFUNCTION(BlueprintPure, Category = "Grid Logic")
	static FString GetCellCoordinateName(int32 X, int32 Y);

	// Retrieves the alphabet letter corresponding to the Y axis index.
	UFUNCTION(BlueprintPure, Category = "Grid Logic")
	static FString GetGridLetter(int32 Index);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deployment")
	int32 PlayerUnitsPlaced = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deployment")
	int32 AIUnitsPlaced = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deployment")
	bool bPlayerDeploysNext = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deployment")
	ETeam FirstTurnWinner;

	// Executes a 50/50 RNG check to determine which team deploys the first unit.
	UFUNCTION(BlueprintCallable, Category = "Deployment")
	void StartCoinFlipAndDeployment();

	// Manages the alternating logic for placing units on the grid.
	UFUNCTION(BlueprintCallable, Category = "Deployment")
	void AdvanceDeployment();

	// Global setting determining the active pathfinding heuristic for enemy units
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	EAIAlgorithm ActiveAIAlgorithm = EAIAlgorithm::AStar;

	// Expose the delegate to Blueprints so the UI Widget can bind to it
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnUIUpdateRequired OnUIUpdateRequired;

	// Helper function to trigger the UI update broadcast
	void TriggerUIUpdate() { OnUIUpdateRequired.Broadcast(); }

	UPROPERTY(BlueprintReadOnly, Category = "Tower UI")
	ETowerState StateTowerWest;

	UPROPERTY(BlueprintReadOnly, Category = "Tower UI")
	ETowerState StateTowerMid;

	UPROPERTY(BlueprintReadOnly, Category = "Tower UI")
	ETowerState StateTowerEast;

	UPROPERTY()
	AStrategyTower* RefTowerWest;

	UPROPERTY()
	AStrategyTower* RefTowerMid;

	UPROPERTY()
	AStrategyTower* RefTowerEast;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	ETurnState CurrentTurnState = ETurnState::PlayerTurn;

};