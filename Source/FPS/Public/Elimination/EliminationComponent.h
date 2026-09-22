// Raymond Learn UE Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EliminationComponent.generated.h"

class AShooterPlayerState;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FPS_API UEliminationComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UEliminationComponent();

	UFUNCTION()
	void OnRoundReported(AActor* Attacker, AActor* Victim, bool bHit, bool bHeadShot, bool bLethal);

private:
	
	AShooterPlayerState* GetPlayerStateFromActor(AActor* Actor);
	void ProcessHitOrMiss(bool bHit, AShooterPlayerState* AttackerPS);
	void ProcessElimination(bool bHit, AShooterPlayerState* AttackerPS, AShooterPlayerState* VictimPS);

};
