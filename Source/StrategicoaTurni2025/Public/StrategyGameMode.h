#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameField.h" // Per poter usare AGameField
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
	// La funzione esposta al Widget Blueprint
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void StartGameWithConfig(float NoiseScale, int32 GridSizeX, int32 GridSizeY);

	// Il riferimento al nostro GameField nel livello
	UPROPERTY(BlueprintReadOnly, Category = "Game Flow")
	AGameField* MapGenerator;

	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void EndTurn();

	// Getter per il PlayerController
	ETurnState GetCurrentTurnState() const { return CurrentTurnState; }
	void CheckRemainingMoves(); 

protected:
	virtual void BeginPlay() override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	ETurnState CurrentTurnState = ETurnState::PlayerTurn;

};