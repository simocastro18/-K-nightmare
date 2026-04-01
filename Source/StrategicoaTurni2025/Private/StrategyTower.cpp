#include "StrategyTower.h"

AStrategyTower::AStrategyTower()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentState = ETowerState::Neutral;

	// Creazione della Mesh (se non l'avevi già creata)
	TowerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
	SetRootComponent(TowerMesh);
}

void AStrategyTower::BeginPlay()
{
	Super::BeginPlay();

	// Appena la partita inizia, applica il colore dello stato attuale (Neutrale)
	UpdateTowerVisuals(CurrentState);
}

void AStrategyTower::UpdateTowerVisuals(ETowerState NewState)
{
	UMaterialInterface* MatToUse = nullptr;

	// Scegliamo il materiale in base allo stato
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

	// Se abbiamo trovato un materiale E la mesh esiste
	if (MatToUse != nullptr && TowerMesh != nullptr)
	{
		// Contiamo quanti "pezzi" ha la tua torre e li coloriamo tutti
		int32 NumMaterials = TowerMesh->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; ++i)
		{
			TowerMesh->SetMaterial(i, MatToUse);
		}
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