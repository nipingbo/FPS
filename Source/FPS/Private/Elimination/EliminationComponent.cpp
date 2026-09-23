// Raymond Learn UE Project

#include "Elimination/EliminationComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Player/ShooterPlayerState.h"
#include "ShooterTypes/ShooterTypes.h"

// Sets default values for this component's properties
UEliminationComponent::UEliminationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SequentialElimInterval = 10.f;
	LastElimTime = 0.f;
	SequentialElims = 0;
	Streak = 0;
	ElimsNeededForStreak = 5;
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
	
void UEliminationComponent::ProcessElimination(bool bHeadShot, AShooterPlayerState* AttackerPS, AShooterPlayerState* VictimPS)
{
	AttackerPS->AddScoredElim();
	VictimPS->AddDefeat();

	ESpecialElimType SpecialElimType{};

	// process head shot
	ProcessHeadShot(bHeadShot, SpecialElimType, AttackerPS);
	// process sequential eliminations
	ProcessSequentialEliminations(SpecialElimType, AttackerPS);
	// process streaks
	ProcessStreaks(SpecialElimType, AttackerPS, VictimPS);
	// handle first blood
	// update leader status

	// if (has special elim types)
		// tell the client which special elims we got
	// else
		// tell the client we got a regular elim
}

void UEliminationComponent::ProcessHeadShot(bool bHeadShot, ESpecialElimType& OutElimType, AShooterPlayerState* AttackerPS)
{
	if (bHeadShot)
	{
		OutElimType |= ESpecialElimType::Headshot;
		AttackerPS->AddHeadShotElim();
	}
}

void UEliminationComponent::ProcessSequentialEliminations(ESpecialElimType& OutElimType, AShooterPlayerState* AttackerPS)
{
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastElimTime <= SequentialElimInterval)
	{
		++SequentialElims;
	}
	else
	{
		SequentialElims = 1;
	}
	LastElimTime = CurrentTime;
	if (SequentialElims > 1)
	{
		OutElimType |= ESpecialElimType::Sequential;
		AttackerPS->AddSequentialElim(SequentialElims);
	}
}

void UEliminationComponent::ProcessStreaks(ESpecialElimType& OutElimType, AShooterPlayerState* AttackerPS, AShooterPlayerState* VictimPS)
{
	++Streak;
	if (Streak >= ElimsNeededForStreak)
	{
		OutElimType |= ESpecialElimType::Streak;
		AttackerPS->SetOnStreak(true);
		AttackerPS->UpdateHighestStreak(Streak);
	}
	if (VictimPS->IsOnStreak())
	{
		OutElimType |= ESpecialElimType::Showstopper;
		AttackerPS->AddShowStopperElim();
		VictimPS->SetOnStreak(false);
	}
	if (AttackerPS->GetLastAttacker() == VictimPS)
	{
		OutElimType |= ESpecialElimType::Revenge;
		AttackerPS->AddRevengeElim();
		AttackerPS->SetLastAttacker(nullptr);
	}
	VictimPS->SetLastAttacker(AttackerPS);

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