// Raymond Learn UE Project

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "Weapon.generated.h"

class UMaterialInstanceDynamic;
enum EPhysicalSurface : int;

UENUM(BlueprintType)
enum class EFireType : uint8
{
	Auto UMETA(DisplayName = "Automatic"),
	SemiAuto UMETA(DisplayName = "Semi Automatic"),
};
UCLASS()
class FPS_API AWeapon : public AActor
{
	GENERATED_BODY()

public:
	AWeapon();
	virtual void OnRep_Instigator() override;
	USkeletalMeshComponent* GetMesh1P() const;
	USkeletalMeshComponent* GetMesh3P() const;

	void AttachToOwningPawn() const;
	void WeaponTrace(FHitResult& OutHit, float TraceLength) const;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|WeaponType")
	FGameplayTag WeaponType;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Aiming")
	float AimFieldOfView;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Trace")
	float TraceRadius;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Trace")
	float TraceLength;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FireType")
	EFireType FireType;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|FireType")
	float FireTime;
	
	void Local_Fire(const FVector& ImpactPoint, const FVector& ImpactNormal, TEnumAsByte<EPhysicalSurface> ImpactSurfaceType, bool bIsFirstPerson);
	bool Auth_Fire();
	// 服务端拒绝开枪时调用，用权威 Ammo 强制覆盖本地预测值并清空 Sequence
	void ResetPrediction(int32 AuthAmmo);
	
	int32 GetMagCapacity() const { return MagCapacity; }
	int32 GetAmmo() const { return Ammo; };
	int32 GetStartingCarriedAmmo() const { return StartingCarriedAmmo; };
	UMaterialInstanceDynamic* GetReticleDynamicMaterialInstance();
	UMaterialInstanceDynamic* GetAmmoCounterDynamicMaterialInstance();
	
protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintImplementableEvent)
	void FireEffects(const FVector& ImpactPoint, const FVector& ImpactNormal, EPhysicalSurface ImpactSurfaceType, bool bIsFirstPerson);
private:
	// Weapon Mesh 1st person view
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon", meta = (AllowPrivateAccess = true))
	TObjectPtr<USkeletalMeshComponent> Mesh1P;
	
	// Weapon Mesh 3rd person view
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon", meta = (AllowPrivateAccess = true))
	TObjectPtr<USkeletalMeshComponent> Mesh3P;
	
	UPROPERTY(EditDefaultsOnly, Category="FPS|Ammo")
	int32 MagCapacity;

	UPROPERTY(EditDefaultsOnly, ReplicatedUsing=OnRep_Ammo, Category = "FPS|Ammo")
	int32 Ammo;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Ammo")
	int32 StartingCarriedAmmo;

	int32 Sequence;
	
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
	TObjectPtr<UMaterialInterface> ReticleMaterial;
	
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon")
	TObjectPtr<UMaterialInterface> AmmoCounterMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynMatInst_Reticle;
	
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynMatInst_AmmoCounter;
	
	UFUNCTION()
	void OnRep_Ammo();
	
	
	void SetMeshVisibilities(APawn* OwningPawn) const;
};
