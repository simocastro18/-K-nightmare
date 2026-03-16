#include "StrategyTower.h"

AStrategyTower::AStrategyTower()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentState = ETowerState::Neutral; // Stato Iniziale 
}