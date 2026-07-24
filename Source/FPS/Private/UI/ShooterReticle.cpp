// Raymond Learn UE Project


#include "UI/ShooterReticle.h"

#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Weapon/Weapon.h"

namespace Ammo
{
	const FName Rounds_Current = FName("Rounds_Current");
	const FName Rounds_Max = FName("Rounds_Max");
}

namespace Reticle
{
	const FName RoundedCornerScale = FName("RoundedCornerScale");
	const FName ShapeCutThickness = FName("ShapeCutThickness");
	const FName Inner_RGBA = FName("Inner_RGBA");
}

void UShooterReticle::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	GetOwningPlayer()->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);
	
	Image_Reticle->SetOpacity(0.f);
	Image_AmmoCounter->SetOpacity(0.f);
	CornerScaleFactor_RoundFired = 0.f;
	ShapeCutFactor_RoundFired = 0.f;
	CornerScaleFactor_Aiming = 0.f;
	ShapeCutFactor_Aiming = 0.f;
	bAiming = false;
	bTargetingPlayer = false;
	
	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(GetOwningPlayer()->GetPawn());
	if (!ShooterCharacter) return;
	
	OnPossessedPawnChanged(nullptr, ShooterCharacter);
	
	// HasAuthority() covers Listen Server host (weapon spawned locally, never goes through replication path).
	// HasWeaponFirstReplicated() covers clients where replication already completed before Widget init.
	// Only bind the delegate when neither condition is met (weapon not yet replicated to this client).
	if (ShooterCharacter->HasWeaponFirstReplicated() || ShooterCharacter->HasAuthority())
	{
		AWeapon* Weapon = IPlayerInterface::Execute_GetCurrentWeapon(ShooterCharacter);
		if (IsValid(Weapon))
		{
			OnReticleChanged(Weapon->GetReticleDynamicMaterialInstance(), Weapon->ReticleParams, false);
			OnAmmoCounterChanged(Weapon->GetAmmoCounterDynamicMaterialInstance(), Weapon->GetAmmo(), Weapon->GetMagCapacity());
		}
	}
	else
	{
		ShooterCharacter->OnWeaponFirstReplicated.AddDynamic(this, &ThisClass::OnWeaponFirstReplicated);
	}
}

void UShooterReticle::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	CornerScaleFactor_RoundFired = FMath::FInterpTo(CornerScaleFactor_RoundFired, 0.f, InDeltaTime, CurrentReticleParams.RoundFiredInterpSpeed);
	ShapeCutFactor_RoundFired = FMath::FInterpTo(ShapeCutFactor_RoundFired, 0.f, InDeltaTime, CurrentReticleParams.RoundFiredInterpSpeed);
	CornerScaleFactor_Aiming = FMath::FInterpTo(CornerScaleFactor_Aiming, bAiming ? CurrentReticleParams.ScaleFactor_Aiming : CurrentReticleParams.ScaleFactor_NotAiming, InDeltaTime, CurrentReticleParams.AimingInterpSpeed);
	ShapeCutFactor_Aiming = FMath::FInterpTo(ShapeCutFactor_Aiming, bAiming ? CurrentReticleParams.ShapeCutFactor_Aiming : CurrentReticleParams.ShapeCutFactor_NotAiming, InDeltaTime, CurrentReticleParams.AimingInterpSpeed);
	CornerScaleFactor_TargetingPlayer = FMath::FInterpTo(CornerScaleFactor_TargetingPlayer, bTargetingPlayer ? CurrentReticleParams.ScaleFactor_Targeting : CurrentReticleParams.ScaleFactor_NotTargeting, InDeltaTime, CurrentReticleParams.TargetingPlayerInterpSpeed);
	
	BaseCornerScaleFactor = CornerScaleFactor_RoundFired + CornerScaleFactor_Aiming + CornerScaleFactor_TargetingPlayer;
	BaseShapeCutFactor = ShapeCutFactor_RoundFired + ShapeCutFactor_Aiming;
	
	if (CurrentReticle_DynMatInst.IsValid())
	{
		CurrentReticle_DynMatInst->SetScalarParameterValue(Reticle::RoundedCornerScale, BaseCornerScaleFactor);
		CurrentReticle_DynMatInst->SetScalarParameterValue(Reticle::ShapeCutThickness, BaseShapeCutFactor);
	}
}

void UShooterReticle::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	//Unbind from delegates on the old pawn's combat component
	//bind to delegates on the new pawn's combat component
	UCombatComponent* OldPawnCombat = UCombatComponent::FindCombatComponent(OldPawn);
	if (OldPawnCombat)
	{
		OldPawnCombat->OnReticleChanged.RemoveDynamic(this, &ThisClass::OnReticleChanged);
		OldPawnCombat->OnAmmoCounterChanged.RemoveDynamic(this, &ThisClass::OnAmmoCounterChanged);
		OldPawnCombat->OnRoundFired.RemoveDynamic(this, &ThisClass::OnRoundFired);
		OldPawnCombat->OnAimingStatusChanged.RemoveDynamic(this, &ThisClass::OnAimingStatusChanged);
		OldPawnCombat->OnTargetingPlayerStatusChanged.RemoveDynamic(this, &ThisClass::OnTargetingPlayerStatusChanged);
	}
	UCombatComponent* NewPawnCombat = UCombatComponent::FindCombatComponent(NewPawn);
	if (NewPawnCombat)
	{
		Image_Reticle->SetOpacity(1.f);
		Image_AmmoCounter->SetOpacity(1.f);
		NewPawnCombat->OnReticleChanged.AddDynamic(this, &ThisClass::OnReticleChanged);
		NewPawnCombat->OnAmmoCounterChanged.AddDynamic(this, &ThisClass::OnAmmoCounterChanged);
		NewPawnCombat->OnRoundFired.AddDynamic(this, &ThisClass::OnRoundFired);
		NewPawnCombat->OnAimingStatusChanged.AddDynamic(this, &ThisClass::OnAimingStatusChanged);
		NewPawnCombat->OnTargetingPlayerStatusChanged.AddDynamic(this, &ThisClass::OnTargetingPlayerStatusChanged);
	}
		
}

void UShooterReticle::OnWeaponFirstReplicated(AWeapon* Weapon)
{
	//get dynamic material instance from the weapon
	OnReticleChanged(Weapon->GetReticleDynamicMaterialInstance(), Weapon->ReticleParams, false);
	OnAmmoCounterChanged(Weapon->GetAmmoCounterDynamicMaterialInstance(), Weapon->GetAmmo(), Weapon->GetMagCapacity());
}

void UShooterReticle::OnReticleChanged(UMaterialInstanceDynamic* ReticleDynMatInst, const FReticleParams& ReticleParams, bool bCurrentTargetingPlayer)
{
	CurrentReticle_DynMatInst = ReticleDynMatInst;
	CurrentReticleParams = ReticleParams;
	FSlateBrush Brush;
	Brush.SetResourceObject(ReticleDynMatInst);
	if (IsValid(Image_Reticle))
	{
		Image_Reticle->SetBrush(Brush);
	}
	OnTargetingPlayerStatusChanged(bCurrentTargetingPlayer);
}

void UShooterReticle::OnAmmoCounterChanged(UMaterialInstanceDynamic* AmmoCounterDynMatInst, int32 RoundsCurrent,
	int32 RoundsMax)
{
	CurrentAmmoCounter_DynMatInst = AmmoCounterDynMatInst;
	if (!CurrentAmmoCounter_DynMatInst.IsValid()) return;
	CurrentAmmoCounter_DynMatInst->SetScalarParameterValue(Ammo::Rounds_Current, RoundsCurrent);
	CurrentAmmoCounter_DynMatInst->SetScalarParameterValue(Ammo::Rounds_Max, RoundsMax);
	FSlateBrush Brush;
	Brush.SetResourceObject(AmmoCounterDynMatInst);
	if (IsValid(Image_AmmoCounter))
	{
		Image_AmmoCounter->SetBrush(Brush);
	}
}

void UShooterReticle::OnRoundFired(int32 RoundsCurrent, int32 RoundsMax)
{
	CornerScaleFactor_RoundFired += CurrentReticleParams.ScaleFactor_RoundFired;
	ShapeCutFactor_RoundFired += CurrentReticleParams.ShapeCutFactor_RoundFired;
	if (CurrentAmmoCounter_DynMatInst.IsValid())
	{
		CurrentAmmoCounter_DynMatInst->SetScalarParameterValue(Ammo::Rounds_Current, RoundsCurrent);
		CurrentAmmoCounter_DynMatInst->SetScalarParameterValue(Ammo::Rounds_Max, RoundsMax);
	}
}

void UShooterReticle::OnAimingStatusChanged(bool bIsAiming)
{
	bAiming =  bIsAiming;
}

void UShooterReticle::OnTargetingPlayerStatusChanged(bool bTargeting)
{
	bTargetingPlayer = bTargeting;
	if (CurrentReticle_DynMatInst.IsValid())
	{
		FLinearColor ReticleColor = bTargetingPlayer ? FLinearColor::Red : FLinearColor::White;
		CurrentReticle_DynMatInst->SetVectorParameterValue(Reticle::Inner_RGBA, ReticleColor);
	}
}
