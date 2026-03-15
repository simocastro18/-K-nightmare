#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameField.h" // Per poter usare AGameField
#include "StrategyGameMode.generated.h"

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

protected:
	virtual void BeginPlay() override;
};