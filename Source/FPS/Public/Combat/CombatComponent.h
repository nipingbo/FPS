// Raymond Learn UE Project

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "CombatComponent.generated.h"

class AWeapon;
class UWeaponData;
class UMaterialInstanceDynamic;
class UAnimMontage;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FReticleChanged, UMaterialInstanceDynamic*, ReticleDynMatInst, const FReticleParams&, ReticleParams, bool, bCurrentTargetingPlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAmmoCounterChanged, UMaterialInstanceDynamic*, AmmoCounterDynMatInst, int32, RoundsCurrent, int32, RoundsMax);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRoundFired, int32, RoundsCurrent, int32, RoundsMax, int32, RoundsInReserve);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAimingStatusChanged, bool, bIsAiming);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTargetingPlayerStatusChanged, bool, bTargeting);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCurrentReserveAmmoChanged, int32, RoundsInReserve, int32, RoundsInWeapon, UMaterialInterface*, WeaponIconMaterial);

// 用于按武器类型和槽位从 WeaponData 中选取要播放的蒙太奇
UENUM()
enum class ECombatMontageSlot : uint8
{
	Equip,
	Reload,
	Fire,
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UFUNCTION(BlueprintPure, Category = "FPS|Combat")
	static UCombatComponent* FindCombatComponent(const AActor* Actor) { return ( IsValid(Actor) ? Actor->FindComponentByClass<UCombatComponent>() : nullptr); }
	
	void Initiate_CycleWeapon();
	void Initiate_FireWeapon_Pressed();
	void Initiate_FireWeapon_Released();
	void Initiate_ReloadWeapon();
	void Initiate_Aim_Pressed();
	void Initiate_Aim_Released();
	
	void Notify_CycleWeapon();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon")
	TObjectPtr<UWeaponData> WeaponData;
	
	void Equip(AWeapon* Weapon);
	
	void EquipWeapon(AWeapon* Weapon);
	UFUNCTION(Server, Reliable)
	void Server_EquipWeapon(AWeapon* Weapon);
	UFUNCTION()
	void BlendOut_CycleWeapon(UAnimMontage* Montage, bool bInterrupted);
	
	void SpawnInventory();
	void DestroyInventory();
	
	void InitializeWeaponWidgets() const;
	
	UPROPERTY(BlueprintAssignable)
	FReticleChanged OnReticleChanged;
	
	UPROPERTY(BlueprintAssignable)
	FAmmoCounterChanged OnAmmoCounterChanged;
	
	UPROPERTY(BlueprintAssignable)
	FRoundFired OnRoundFired;
	
	UPROPERTY(BlueprintAssignable)
	FAimingStatusChanged OnAimingStatusChanged;
	
	UPROPERTY(BlueprintAssignable)
	FTargetingPlayerStatusChanged OnTargetingPlayerStatusChanged;
	
	UPROPERTY(BlueprintAssignable)
	FCurrentReserveAmmoChanged OnCurrentReserveAmmoChanged;
	
	UPROPERTY(BlueprintReadOnly, Replicated)
	bool bAiming;
	
	UPROPERTY(Transient, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<AWeapon> CurrentWeapon;
	
	UPROPERTY(ReplicatedUsing = OnRep_CurrentReserveAmmo)
	int32 CurrentReserveAmmo;
protected:
	
private:
	TMap<FGameplayTag, int32> ReserveAmmo;
	bool bHitPlayer;
	bool bHitPlayerLastFrame = false;
	bool bTriggerPressed = false;
	FTimerHandle FireTimer;
	float LastFireTime = -1.f;

	void FireTimerFinished();
	
	UFUNCTION()
	void OnRep_CurrentWeapon(AWeapon* LastWeapon);
	
	void SetCurrentWeapon(AWeapon* NewWeapon, AWeapon* LastWeapon);
	
	UPROPERTY(Transient, Replicated)
	TArray<AWeapon*> Inventory;
	
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
	TArray<TSubclassOf<AWeapon>> DefaultWeaponClasses;
	
	AWeapon* SpawnWeapon(TSubclassOf<AWeapon> WeaponClass) const;
	
	UFUNCTION(Server, Reliable)
	void Server_Aim(bool bPressed);
	
	// 不接受客户端传来的 Hit 数据，服务端收到信号后自己做 Trace，
	// 防止客户端伪造命中位置或目标。
	UFUNCTION(Server, Reliable)
	void Server_FireWeapon();

	// 服务端拒绝开枪时通知客户端，让客户端重置预测状态
	UFUNCTION(Client, Reliable)
	void Client_FireRejected(int32 AuthAmmo);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FireWeapon(const FHitResult& Hit);
	
	UFUNCTION()
	void OnRep_CurrentReserveAmmo();
	
	void Local_Aim(bool bPressed);
	void Local_FireWeapon();
	
	int32 AdvanceWeaponIndex();
	int32 Local_WeaponIndex;
	/* Cycle Weapon */
	void Local_CycleWeapon(int32 WeaponIndex);
	UFUNCTION(Server, Reliable)
	void Server_CycleWeapon(int32 WeaponIndex);
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_CycleWeapon(int32 WeaponIndex);
	
	/* Reload Weapon*/
	void Local_ReloadWeapon();
	UFUNCTION(Server, Reliable)
	void Server_ReloadWeapon();
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ReloadWeapon(int32 NewWeaponAmmo, int32 NewCarriedAmmo);

	/* 蒙太奇播放辅助 */
	bool PlayMontageOnMesh(const USkeletalMeshComponent* Mesh, UAnimMontage* Montage) const;
	const USkeletalMeshComponent* GetViewMesh(bool bIsLocal) const;
	UAnimMontage* GetBodyMontage(const FGameplayTag& WeaponType, ECombatMontageSlot Slot, bool bIsLocal) const;
	UAnimMontage* GetWeaponMontage(const FGameplayTag& WeaponType, ECombatMontageSlot Slot) const;
};