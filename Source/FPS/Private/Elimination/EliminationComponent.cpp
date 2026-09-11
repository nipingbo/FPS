// Raymond Learn UE Project

#include "Elimination/EliminationComponent.h"

// Sets default values for this component's properties
UEliminationComponent::UEliminationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}

void UEliminationComponent::OnRoundReported(AActor* Attacker, AActor* Victim, bool bHit, bool bHeadShot, bool bLethal)
{
	
}