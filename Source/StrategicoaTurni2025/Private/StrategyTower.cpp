#include "StrategyTower.h"
#include "StrategyGameMode.h"

AStrategyTower::AStrategyTower()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentState = ETowerState::Neutral;

	// Initialize the physical mesh component
	
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TowerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
	TowerMesh->SetupAttachment(SceneRoot); 
}

void AStrategyTower::BeginPlay()
{
	Super::BeginPlay();

	// Apply the default neutral visual state at the start of the match
	UpdateTowerVisuals(CurrentState);
}

void AStrategyTower::UpdateTowerVisuals(ETowerState NewState)
{
	UMaterialInterface* MatToUse = nullptr;
	FString TowerMsg = TEXT("");

	// Extract grid coordinates to format the UI log message
	char ColLetter = 'A' + GridPosition.Y;
	int32 RowNum = GridPosition.X;

	// Determine the correct material and log message based on the new state
	switch (NewState)
	{
	case ETowerState::Neutral:
		MatToUse = NeutralMaterial;
		break;
	case ETowerState::ControlledPlayer:
		MatToUse = PlayerMaterial;
		TowerMsg = FString::Printf(TEXT("TOW: HP captured Tower %c%d!"), ColLetter, RowNum);
		break;
	case ETowerState::ControlledAI:
		MatToUse = AIMaterial;
		TowerMsg = FString::Printf(TEXT("TOW: AI captured Tower %c%d!"), ColLetter, RowNum);
		break;
	case ETowerState::Contested:
		MatToUse = ContestedMaterial;
		TowerMsg = FString::Printf(TEXT("TOW: Tower %c%d is Contested!"), ColLetter, RowNum);
		break;
	}

	// Apply the selected material to all material slots on the mesh
	if (MatToUse != nullptr && TowerMesh != nullptr)
	{	
			TowerMesh->SetMaterial(0, MatToUse);
	}

	// Broadcast the state change to the GameMode for the UI Combat Log
	if (!TowerMsg.IsEmpty())
	{
		AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
		if (GM)
		{
			GM->AddGameLog(TowerMsg);
		}
	}
}