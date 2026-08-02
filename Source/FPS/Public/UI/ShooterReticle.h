// Raymond Learn UE Project

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterTypes/ShooterTypes.h"
#include "ShooterReticle.generated.h"

class UImage;

UCLASS()
class FPS_API UShooterReticle : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	// Image for the reticle
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Reticle;
	// Image for the ammo
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_AmmoCounter;
	
private:
	TWeakObjectPtr<UMaterialInstanceDynamic> CurrentReticle_DynMatInst;
	TWeakObjectPtr<UMaterialInstanceDynamic> CurrentAmmoCounter_DynMatInst;
	FReticleParams CurrentReticleParams;
	float BaseCornerScaleFactor = 0.f;
	float BaseShapeCutFactor = 0.f;
	float CornerScaleFactor_RoundFired = 0.f;
	float ShapeCutFactor_RoundFired = 0.f;
	float CornerScaleFactor_Aiming = 0.f;
	float ShapeCutFactor_Aiming = 0.f;
	float CornerScaleFactor_TargetingPlayer = 0.f;
	bool bAiming = false;
	bool bTargetingPlayer = false;
	
	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
	
	UFUNCTION()
	void OnWeaponFirstReplicated(AWeapon* Weapon);
	
	UFUNCTION()
	void OnReticleChanged(UMaterialInstanceDynamic* ReticleDynMatInst, const FReticleParams& ReticleParams, bool bCurrentTargetingPlayer);
	
	UFUNCTION()
	void OnAmmoCounterChanged(UMaterialInstanceDynamic* AmmoCounterDynMatInst, int32 RoundsCurrent, int32 RoundsMax);

	UFUNCTION()
	void OnRoundFired(int32 RoundsCurrent, int32 RoundsMax, int32 RoundsInReserve);
	
	UFUNCTION()
	void OnAimingStatusChanged(bool bIsAiming);
	
	UFUNCTION()
	void OnTargetingPlayerStatusChanged(bool bTargeting);
};

