#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StrategyUnit.generated.h"

// Forward declarations 
class ATile;
class AGameField;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

// Defines the combat capabilities of the unit
UENUM(BlueprintType)
enum class EAttackType : uint8
{
	MELEE   UMETA(DisplayName = "Melee"),
	RANGED  UMETA(DisplayName = "Ranged")
};

// Defines the faction the unit belongs to
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

	// Faction materials

	UPROPERTY(EditDefaultsOnly, Category = "Unit Visuals")
	UMaterialInterface* PlayerMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Unit Visuals")
	UMaterialInterface* AIMaterial;

	// Visual components

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* UnitMesh;

	// Base statistics

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

	// Runtime state variables 

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit State")
	int32 CurrentHealth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit State")
	int32 PlayerOwner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit State")
	ATile* CurrentTile;

	// Combat and lifecycle function

	UFUNCTION(BlueprintCallable, Category = "Unit Actions")
	void ReceiveDamage(int32 DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Unit")
	void InitializeUnit(const FString& InUnitLogID, ETeam InUnitTeam, float InInitialYaw, class ATile* StartingTile);

	UFUNCTION(BlueprintCallable, Category = "Unit Actions")
	int32 CalculateDamageToDeal();

	UFUNCTION(BlueprintCallable, Category = "Unit Actions")
	void AttackTarget(AStrategyUnit* TargetUnit);

	// AI system

	// Main entry point for the AI unit to evaluate the board and act
	UFUNCTION(BlueprintCallable, Category = "AI")
	void ExecuteAITurn();

	// Turn logic flags
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn Logic")
	bool bHasMoved = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn Logic")
	bool bHasAttacked = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn Logic")
	bool bIsTurnFinished = false;

	// Movement system

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bIsMoving = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MoveSpeed = 600.0f;

	TArray<FVector> PathToFollow;
	int32 CurrentPathIndex = 0;

	void StartMoving(TArray<FVector> NewPath);
	virtual void Tick(float DeltaTime) override;

	// Direct reference to the map logic generator
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References")
	AGameField* GameFieldRef;

	// Internal AI data

	ATile* AIBestTargetTile = nullptr;
	FTimerHandle AIThinkTimerHandle;

	// Executed via timer to simulate AI "thinking" before moving
	UFUNCTION()
	void ExecuteAIMovement();

	// Respawn system

	// Caches the deployment tile to allow unit recovery upon death
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
	ATile* OriginalSpawnTile;

	// Handles the "fake death" visual and resets the unit to the OriginalSpawnTile
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RespawnUnit();

	// Blueprint UI events 

	// Hook to instantiate the health bar widget dynamically
	UFUNCTION(BlueprintImplementableEvent, Category = "Unit UI")
	void OnSetupHealthBar(ETeam Team);

	// Hook to update the health bar fill percentage
	UFUNCTION(BlueprintImplementableEvent, Category = "Unit UI")
	void OnHealthChanged(float NewCurrentHealth, float NewMaxHealth);

	// Hook to spawn a floating text widget representing damage taken
	UFUNCTION(BlueprintImplementableEvent, Category = "Unit UI")
	void OnShowFloatingDamage(float DamageAmount, FVector SpawnLocation);

protected:
	virtual void BeginPlay() override;
};