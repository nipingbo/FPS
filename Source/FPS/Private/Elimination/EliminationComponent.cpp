// Raymond Learn UE Project

#include "Elimination/EliminationComponent.h"
#include "GameFramework/Pawn.h"
#include "Player/ShooterPlayerState.h"
#include "ShooterTypes/ShooterTypes.h"

// Sets default values for this component's properties
UEliminationComponent::UEliminationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}

void UEliminationComponent::OnRoundReported(AActor* Attacker, AActor* Victim, bool bHit, bool bHeadShot, bool bLethal)
{
	AShooterPlayerState* AttackerPS = GetPlayerStateFromActor(Attacker);

	ProcessHitOrMiss(bHit, AttackerPS);

	if (!bHit) return;

	AShooterPlayerState* VictimPS = GetPlayerStateFromActor(Victim);
	if (!IsValid(VictimPS)) return;

	if (bLethal)
	{
		ProcessElimination(bHeadShot, AttackerPS, VictimPS);
	}
}


void UEliminationComponent::ProcessHitOrMiss(bool bHit, AShooterPlayerState* AttackerPS)
{
	if (bHit)
	{
		AttackerPS->AddHit();
	}
	else
	{
		AttackerPS->AddMiss();
	}
}

void UEliminationComponent::ProcessHeadShot(bool bHit, ESpecialElimType& OutElimType, AShooterPlayerState* AttackerPS)
{
	if (bHit)
	{
		OutElimType |= ESpecialElimType::Headshot;
		AttackerPS->AddHeadShotElim();
	}
}
	
void UEliminationComponent::ProcessElimination(bool bHit, AShooterPlayerState* AttackerPS, AShooterPlayerState* VictimPS)
{
	AttackerPS->AddScoredElim();
	VictimPS->AddDefeat();

	ESpecialElimType SpecialElimType{};

	// process head shot
	ProcessHeadShot(bHit, SpecialElimType, AttackerPS);
	// process sequential eliminations
	// process streaks
	// handle first blood
	// update leader status

	// if (has special elim types)
		// tell the client which special elims we got
	// else
		// tell the client we got a regular elim
}

AShooterPlayerState* UEliminationComponent::GetPlayerStateFromActor(AActor* Actor)
{
	APawn* Pawn = Cast<APawn>(Actor);
	if (IsValid(Pawn))
	{
		return Pawn->GetPlayerState<AShooterPlayerState>();
	}

	return nullptr;
}