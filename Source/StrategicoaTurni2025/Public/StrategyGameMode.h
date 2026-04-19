#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StrategyUnit.h" // Ci serve solo questo per l'Enum ETeam
#include "StrategyGameMode.generated.h"

// --- FORWARD DECLARATIONS (Il trucco per evitare dipendenze circolari) ---
class AGameField;
class UUserWidget;

// --- NUOVO ENUM PER LA SCELTA DELL'ALGORITMO ---
UENUM(BlueprintType)
enum class EAIAlgorithm : uint8
{
	AStar  UMETA(DisplayName = "A*"),
	Greedy UMETA(DisplayName = "Greedy")
};

// --- STRUTTURA CONFIGURAZIONE ---
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

// --- STATI DEL TURNO ---
UENUM(BlueprintType)
enum class ETurnState : uint8
{
	Deployment UMETA(DisplayName = "Fase di Schieramento"),
	PlayerTurn UMETA(DisplayName = "Turno Giocatore"),
	AITurn     UMETA(DisplayName = "Turno AI")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameLogAdded, const FString&, LogMessage);

UCLASS()
class STRATEGICOATURNI2025_API AStrategyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> WinWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> LoseWidgetClass;

	void HandleGameOver(ETeam Winner);

	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void RestartGame();

	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void StartGameWithConfig(FGameConfig Config);

	// Usiamo il puntatore alla Forward Declaration
	UPROPERTY(BlueprintReadOnly, Category = "Game Flow")
	AGameField* MapGenerator;

	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void EndTurn();

	ETurnState GetCurrentTurnState() const { return CurrentTurnState; }

	void CheckRemainingMoves();

	UFUNCTION(BlueprintCallable, Category = "Game Flow")

	void EvaluateTowers();

	UFUNCTION(BlueprintCallable, Category = "AI")
	void ProcessAITurn();

	// --- SISTEMA DI VITTORIA ---
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

	UFUNCTION(BlueprintCallable, Category = "UI Log")
	void AddGameLog(const FString& Message);

	// --- STATO DELLA PARTITA ---
	UPROPERTY(BlueprintReadOnly, Category = "Game State")
	int32 PlayerTowers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Game State")
	int32 AITowers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Game State")
	int32 CurrentTurnNumber = 1;

	// --- HELPER GRIGLIA ---
	UFUNCTION(BlueprintPure, Category = "Grid Logic")
	static FString GetCellCoordinateName(int32 X, int32 Y);

	UFUNCTION(BlueprintPure, Category = "Grid Logic")
	static FString GetGridLetter(int32 Index);

	// --- VARIABILI SCHIERAMENTO ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deployment")
	int32 PlayerUnitsPlaced = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deployment")
	int32 AIUnitsPlaced = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deployment")
	bool bPlayerDeploysNext = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deployment")
	ETeam FirstTurnWinner;

	UFUNCTION(BlueprintCallable, Category = "Deployment")
	void StartCoinFlipAndDeployment();

	UFUNCTION(BlueprintCallable, Category = "Deployment")
	void AdvanceDeployment();

	// --- ALGORITMO IA GLOBALE ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	EAIAlgorithm ActiveAIAlgorithm = EAIAlgorithm::AStar;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	ETurnState CurrentTurnState = ETurnState::PlayerTurn;
};