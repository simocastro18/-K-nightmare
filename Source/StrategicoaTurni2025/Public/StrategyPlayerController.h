#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "GameFramework/PlayerController.h"

class ATile;
class AStrategyUnit; // Diciamo al compilatore che esiste questa classe

#include "StrategyPlayerController.generated.h"

UCLASS()
class STRATEGICOATURNI2025_API AStrategyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Grid Logic")
	void HandleTileClick(ATile* ClickedTile);

	UPROPERTY(BlueprintReadWrite, Category = "Grid Logic")
	ATile* CurrentSelectedTile;

	// CAMBIATO: Ora è specifico per le nostre unità!
	UPROPERTY(BlueprintReadWrite, Category = "Grid Logic")
	AStrategyUnit* SelectedUnit;

	// Riferimento al GameField (ci servirà a breve per far calcolare i percorsi matematici)
	UPROPERTY(BlueprintReadWrite, Category = "Grid Logic")
	class AGameField* GameFieldRef;

	// La funzione esposta al Blueprint che fa partire il raggio
	UFUNCTION(BlueprintCallable, Category = "Grid Logic")
	void ExecuteClick();

public:
	// Il nostro Contesto di Mapping (L'interruttore generale)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputMappingContext* DefaultMappingContext;

	// La nostra Azione di Click
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputAction* ClickAction;

protected:
	virtual void BeginPlay() override;
	// Funzione nativa di Unreal per legare i tasti alle funzioni
	virtual void SetupInputComponent() override;
};