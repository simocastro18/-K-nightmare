#include "StrategyTower.h"
#include "StrategyGameMode.h"

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
	FString TowerMsg = TEXT("");

	// Ricaviamo la lettera e il numero della torre (usando la tua variabile GridPosition)
	char ColLetter = 'A' + GridPosition.Y;
	int32 RowNum = GridPosition.X;

	// Scegliamo il materiale e prepariamo il messaggio in base allo stato
	switch (NewState)
	{
	case ETowerState::Neutral:
		MatToUse = NeutralMaterial;
		// Nessun log per quando è neutrale (inizio partita)
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

	// 1. Aggiorniamo i Colori
	if (MatToUse != nullptr && TowerMesh != nullptr)
	{
		int32 NumMaterials = TowerMesh->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; ++i)
		{
			TowerMesh->SetMaterial(i, MatToUse);
		}
	}

	// 2. SPARIAMO IL LOG
	if (!TowerMsg.IsEmpty())
	{
		// ---> AGGIUNGI QUESTA RIGA <---
		UE_LOG(LogTemp, Warning, TEXT("TENTATIVO DI INVIO LOG TORRE: %s"), *TowerMsg);
		AStrategyGameMode* GM = Cast<AStrategyGameMode>(GetWorld()->GetAuthGameMode());
		if (GM)
		{
			GM->AddGameLog(TowerMsg);
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