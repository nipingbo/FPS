// Raymond Learn UE Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"


class AWeapon;
class UWeaponData;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Initiate_CycleWeapon();
	void Initiate_FireWeapon_Pressed();
	void Initiate_FireWeapon_Released();
	void Initiate_ReloadWeapon();
	void Initiate_Aim_Pressed();
	void Initiate_Aim_Released();
	
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
	TObjectPtr<UWeaponData> WeaponData;
	
	void SpawnInventory();
	void DestroyInventory();
protected:

private:
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
	TSubclassOf<AWeapon> DefaultWeaponClass;
	
	AWeapon* SpawnWeapon(TSubclassOf<AWeapon> WeaponClass) const;
};