// Raymond Learn UE Project

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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
	
	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
	
	UFUNCTION()
	void OnWeaponFirstReplicated(AWeapon* Weapon);
};

