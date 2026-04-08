#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StrategyUnit.generated.h"

// Diciamo al compilatore in anticipo che esiste una classe ATile
class ATile;

// Enum per il tipo di attacco, come richiesto dal PDF (Spazi invisibili rimossi)
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

	// --- MATERIALI DELLE FAZIONI ---
	UPROPERTY(EditDefaultsOnly, Category = "Unit Visuals")
	class UMaterialInterface* PlayerMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Unit Visuals")
	class UMaterialInterface* AIMaterial;

	// --- COMPONENTI VISIVI ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* UnitMesh;

	// --- STATISTICHE BASE ---
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

	// --- VARIABILI DI STATO ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit State")
	int32 CurrentHealth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit State")
	int32 PlayerOwner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit State")
	ATile* CurrentTile;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit State")
	bool bHasActedThisTurn;

	// --- FUNZIONI DI COMBATTIMENTO E GESTIONE ---

	UFUNCTION(BlueprintCallable, Category = "Unit Actions")
	void ReceiveDamage(int32 DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Unit")
	void InitializeUnit(const FString& InUnitLogID, ETeam InUnitTeam, float InInitialYaw, class ATile* StartingTile);

	UFUNCTION(BlueprintCallable, Category = "Unit Actions")
	int32 CalculateDamageToDeal();

	UFUNCTION(BlueprintCallable, Category = "Unit Actions")
	void AttackTarget(AStrategyUnit* TargetUnit);

	// --- NUOVO: INTELLIGENZA ARTIFICIALE ---
	UFUNCTION(BlueprintCallable, Category = "AI")
	void ExecuteAITurn();

	// --- VARIABILI LOGICA TURNI ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn Logic")
	bool bHasMoved = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn Logic")
	bool bHasAttacked = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn Logic")
	bool bIsTurnFinished = false;

	// --- VARIABILI PER IL MOVIMENTO ---
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bIsMoving = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 HealthPoints = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MoveSpeed = 600.0f;
	
	TArray<FVector> PathToFollow;
	int32 CurrentPathIndex = 0;

	void StartMoving(TArray<FVector> NewPath);
	virtual void Tick(float DeltaTime) override;

	// Reference diretta al campo di gioco
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References")
	class AGameField* GameFieldRef;

	// --- VARIABILI IA ---
	ATile* AIBestTargetTile = nullptr;
	FTimerHandle AIThinkTimerHandle;

	UFUNCTION()
	void ExecuteAIMovement(); // La funzione che partirà DOPO il delay

	// --- SISTEMA DI RESPAWN ---

	// Memorizza la cella iniziale per il respawn
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
	ATile* OriginalSpawnTile;

	// Funzione che gestisce la "falsa morte" e il ritorno alla base
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RespawnUnit();
	/*
	// Aggiungi questo: creerà un nodo Evento rosso nel Blueprint!
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void SetupHealthBarUI();

	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void ShowFloatingDamage(int32 DamageReceived);
	*/

	// EVENTI UI DA IMPLEMENTARE IN BLUEPRINT
	UFUNCTION(BlueprintImplementableEvent, Category = "Unit UI")
	void OnSetupHealthBar(ETeam Team);

	UFUNCTION(BlueprintImplementableEvent, Category = "Unit UI")
	void OnHealthChanged(float NewCurrentHealth, float NewMaxHealth); // <--- NOMI CAMBIATI

	UFUNCTION(BlueprintImplementableEvent, Category = "Unit UI")
	void OnShowFloatingDamage(float DamageAmount, FVector SpawnLocation);

protected:
	virtual void BeginPlay() override;
};