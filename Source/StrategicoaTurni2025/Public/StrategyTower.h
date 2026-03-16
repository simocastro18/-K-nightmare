#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StrategyTower.generated.h"

// La Macchina a Stati descritta nel PDF [cite: 113, 116, 120]
UENUM(BlueprintType)
enum class ETowerState : uint8
{
	Neutral           UMETA(DisplayName = "Neutrale"),
	ControlledPlayer  UMETA(DisplayName = "Controllata Player"),
	ControlledAI      UMETA(DisplayName = "Controllata AI"),
	Contested         UMETA(DisplayName = "Contesa")
};

UCLASS(Blueprintable)
class STRATEGICOATURNI2025_API AStrategyTower : public AActor
{
	GENERATED_BODY()

public:
	AStrategyTower();

	// Memorizziamo lo stato e la posizione
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tower")
	ETowerState CurrentState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tower")
	FIntPoint GridPosition;

	// Questo evento chiamerà il Blueprint per cambiare il colore visivo! 
	UFUNCTION(BlueprintImplementableEvent, Category = "Tower")
	void OnStateChanged(ETowerState NewState);
};