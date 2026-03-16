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

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	ETurnState CurrentTurnState = ETurnState::PlayerTurn;
};