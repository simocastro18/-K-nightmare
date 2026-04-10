#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameField.h" 
#include "StrategyUnit.h"
#include "StrategyGameMode.generated.h"

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
};

UENUM(BlueprintType)
enum class ETurnState : uint8
{
	Deployment    UMETA(DisplayName = "Fase di Schieramento"), // <--- DEVE ESSERCI QUESTO
	PlayerTurn    UMETA(DisplayName = "Turno Giocatore"),
	AITurn        UMETA(DisplayName = "Turno AI")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameLogAdded, const FString&, LogMessage);

UCLASS()
class STRATEGICOATURNI2025_API AStrategyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	// Riferimenti ai Blueprint dei Widget (da assegnare nell'editor)
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UUserWidget> WinWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<class UUserWidget> LoseWidgetClass;

	// Sostituiamo l'evento Blueprint con una funzione C++
	void HandleGameOver(ETeam Winner);

	// Funzione per il Rematch (carica il livello corrente)
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void RestartGame();

	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void StartGameWithConfig(FGameConfig Config);

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow")
	AGameField* MapGenerator;

	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void EndTurn();

	ETurnState GetCurrentTurnState() const { return CurrentTurnState; }

	void CheckRemainingMoves();

	// Lancia la moneta e avvia il primo turno ufficiale
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void StartFirstTurn();

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

	// IL KILL SWITCH
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	bool bIsGameOver = false;

	// --- UI DEL GAME OVER ---
	/*
	UFUNCTION(BlueprintImplementableEvent, Category = "Game Flow")
	void OnGameOver(ETeam Winner);
	*/

	// Il megafono che avvisa la UI
	UPROPERTY(BlueprintAssignable, Category = "UI Log")
	FOnGameLogAdded OnGameLogAdded;

	// La funzione che chiameremo in C++ per aggiungere un testo
	UFUNCTION(BlueprintCallable, Category = "UI Log")
	void AddGameLog(const FString& Message);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow")
	ETurnState CurrentTurnState = ETurnState::PlayerTurn;
};