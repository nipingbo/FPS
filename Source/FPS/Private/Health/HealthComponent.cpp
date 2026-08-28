// Raymond Learn UE Project


#include "Health/HealthComponent.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
	DeathState = EDeathState::NotDead;
	SetIsReplicatedByDefault(true);
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UHealthComponent, DeathState);
	DOREPLIFETIME_CONDITION(UHealthComponent, Health, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UHealthComponent, MaxHealth, COND_OwnerOnly);
}

float UHealthComponent::GetHealthNormalized() const
{
	return (MaxHealth > 0.0f) ? (Health / MaxHealth) : 0.0f;
}

bool UHealthComponent::ChangeHealthByAmount(float Amount, AActor* Instigator)
{
	float OldValue = Health;
	Health = FMath::Clamp(Health + Amount, 0.0f, MaxHealth);
	//Broadcast OnHealthChanged
	OnHealthChanged.Broadcast(this, OldValue, Health, Instigator);
	//Check if lethal -> Start Death
	//return bLethal
	return false;
}

void UHealthComponent::ChangeMaxHealthByAmount(float Amount, AActor* Instigator)
{
	float OldValue = MaxHealth;
	MaxHealth += Amount;
	//broadcast OnHealthChanged
	OnMaxHealthChanged.Broadcast(this, OldValue, MaxHealth, Instigator);
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	
}

void UHealthComponent::OnRep_DeathState(EDeathState OldDeathState)
{
}

void UHealthComponent::OnRep_Health(float OldValue) const
{
	//broadcast OnHealthChanged
	OnHealthChanged.Broadcast(this, OldValue, Health, nullptr);
}

void UHealthComponent::OnRep_MaxHealth(float OldValue)
{
	//broadcast OnHealthChanged
	OnMaxHealthChanged.Broadcast(this, OldValue, MaxHealth, nullptr);
}

