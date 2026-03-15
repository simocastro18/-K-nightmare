#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StrategyUnit.generated.h"

// Enum per il tipo di attacco, come richiesto dal PDF
UENUM(BlueprintType)
enum class EAttackType : uint8
{
	MELEE   UMETA(DisplayName = "Corto Raggio"),
	RANGED  UMETA(DisplayName = "A Distanza")
};

UENUM(BlueprintType)
enum class ETeam : uint8
{
	Player  UMETA(DisplayName = "Player Team"),
	AI      UMETA(DisplayName = "AI Team")
};

UCLASS()
class STRATEGICOATURNI2025_API AStrategyUnit : public AActor
{
	GENERATED_BODY()

public:
	AStrategyUnit();

	// --- COMPONENTI VISIVI ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* UnitMesh;

	// --- STATISTICHE BASE (Esposte per i Blueprint figli) ---

	// Identificativo per il Log ('S' per Sniper, 'B' per Brawler)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats")
	FString UnitLogID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats")
	ETeam UnitTeam = ETeam::Player;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Unit Stats")
	bool bHasMovedThisTurn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats")
	int32 MovementRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats")
	EAttackType AttackType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats")
	int32 AttackRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats")
	int32 MinDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats")
	int32 MaxDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit Stats")
	int32 MaxHealth;

	// --- VARIABILI DI STATO (Modificate a Runtime) ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit State")
	int32 CurrentHealth;

	// ID del giocatore (es. 0 per Umano, 1 per AI)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit State")
	int32 PlayerOwner;

	// Cella su cui si trova attualmente l'unità
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit State")
	class ATile* CurrentTile;

	// Booleana per sapere se l'unità ha già agito in questo turno
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit State")
	bool bHasActedThisTurn;

	// --- FUNZIONI DI COMBATTIMENTO E GESTIONE ---

	// Inizializza i PS e le statistiche all'inizio o al respawn
	UFUNCTION(BlueprintCallable, Category = "Unit Actions")
	void InitializeUnit(int32 InOwnerID, ATile* StartingTile);

	// Funzione per subire danni
	UFUNCTION(BlueprintCallable, Category = "Unit Actions")
	void ReceiveDamage(int32 DamageAmount);

	// Genera un danno randomico tra Min e Max
	UFUNCTION(BlueprintCallable, Category = "Unit Actions")
	int32 CalculateDamageToDeal();

protected:
	virtual void BeginPlay() override;
};