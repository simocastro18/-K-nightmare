#include "StrategyTower.h"

AStrategyTower::AStrategyTower()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentState = ETowerState::Neutral;

	// Creazione della Mesh (se non l'avevi già creata)
	TowerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
	SetRootComponent(TowerMesh);
}

void AStrategyTower::UpdateTowerVisuals(ETowerState NewState)
{
	if (!TowerMesh) return;

	UMaterialInterface* MatToUse = nullptr;

	// Lo stesso identico Switch che avevi nel Blueprint!
	switch (NewState)
	{
	case ETowerState::Neutral:
		MatToUse = NeutralMaterial;
		break;
	case ETowerState::ControlledPlayer:
		MatToUse = PlayerMaterial;
		break;
	case ETowerState::ControlledAI:
		MatToUse = AIMaterial;
		break;
	case ETowerState::Contested:
		MatToUse = ContestedMaterial;
		break;
	}

	if (MatToUse)
	{
		TowerMesh->SetMaterial(0, MatToUse);
	}
}

/*
Vecchio BP
AStrategyTower::AStrategyTower()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentState = ETowerState::Neutral; // Stato Iniziale 
}
*/