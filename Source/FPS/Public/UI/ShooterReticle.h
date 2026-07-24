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
	float BaseCornerScaleFactor;
	float BaseShapeCutFactor;
	float CornerScaleFactor_RoundFired;
	float ShapeCutFactor_RoundFired;
	float CornerScaleFactor_Aiming;
	float ShapeCutFactor_Aiming;
	float CornerScaleFactor_TargetingPlayer;
	bool bAiming;
	bool bTargetingPlayer;
	
	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
	
	UFUNCTION()
	void OnWeaponFirstReplicated(AWeapon* Weapon);
	
	UFUNCTION()
	void OnReticleChanged(UMaterialInstanceDynamic* ReticleDynMatInst, const FReticleParams& ReticleParams, bool bCurrentTargetingPlayer);
	
	UFUNCTION()
	void OnAmmoCounterChanged(UMaterialInstanceDynamic* AmmoCounterDynMatInst, int32 RoundsCurrent, int32 RoundsMax);

	UFUNCTION()
	void OnRoundFired(int32 RoundsCurrent, int32 RoundsMax);
	
	UFUNCTION()
	void OnAimingStatusChanged(bool bIsAiming);
	
	UFUNCTION()
	void OnTargetingPlayerStatusChanged(bool bTargeting);
};

