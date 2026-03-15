#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

//al posto di #include "Tile.h"
class ATile;
//#include "Tile.h" // Importante per far riconoscere la classe ATile
#include "StrategyPlayerController.generated.h"



UCLASS()
class STRATEGICOATURNI2025_API AStrategyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// La funzione principale chiamata dal click sulla cella
	UFUNCTION(BlueprintCallable, Category = "Grid Logic")
	void HandleTileClick(ATile* ClickedTile);

	// Memoria per l'ottimizzazione O(1) dell'illuminazione
	UPROPERTY(BlueprintReadWrite, Category = "Grid Logic")
	ATile* CurrentSelectedTile;

	// Memoria per l'unità selezionata (se ne hai cliccata una)
	UPROPERTY(BlueprintReadWrite, Category = "Grid Logic")
	AActor* SelectedUnit;

protected:
	virtual void BeginPlay() override;
};