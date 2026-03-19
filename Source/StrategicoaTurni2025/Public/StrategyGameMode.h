#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameField.h" 
#include "StrategyUnit.h"
#include "StrategyGameMode.generated.h"

UENUM(BlueprintType)
enum class ETurnState : uint8
{
	PlayerTurn  UMETA(DisplayName = "Turno Giocatore"),
	AITurn      UMETA(DisplayName = "Turno AI")
};

UCLASS()
class STRATEGICOATURNI2025_API AStrategyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void StartGameWithConfig(float NoiseScale, int32 GridSizeX, int32 GridSizeY);

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow")
	AGameField* MapGenerator;

	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void EndTurn();

	ETurnState GetCurrentTurnState() const { return CurrentTurnState; }

	void CheckRemainingMoves();

	// Valuta la macchina a stati delle torri
	void EvaluateTowers();

	// NUOVO: La funzione che gestisce i nemici uno alla volta
	UFUNCTION(BlueprintCallable, Category = "AI")
	void ProcessAITurn();

	// --- SISTEMA DI VITTORIA ---

	// Da quanti turni consecutivi il Player ha il dominio (>= 2 torri)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	int32 PlayerDominanceTurns = 0;

	// Da quanti turni consecutivi l'AI ha il dominio (>= 2 torri)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	int32 AIDominanceTurns = 0;

	// Quante torri ha attualmente il Player
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	int32 PlayerTowerCount = 0;

	// Quante torri ha attualmente l'AI
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	int32 AITowerCount = 0;

	// --- UI DEL GAME OVER ---

	UFUNCTION(BlueprintImplementableEvent, Category = "Game Flow")
	void OnGameOver(ETeam Winner);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	ETurnState CurrentTurnState = ETurnState::PlayerTurn;
};