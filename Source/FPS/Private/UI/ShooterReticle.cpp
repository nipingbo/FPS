// Raymond Learn UE Project


#include "UI/ShooterReticle.h"

#include "Character/ShooterCharacter.h"
#include "Kismet/GameplayStatics.h"

void UShooterReticle::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	GetOwningPlayer()->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);
	
	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(GetOwningPlayer()->GetPawn());
	if (!ShooterCharacter) return;
	
	OnPossessedPawnChanged(nullptr, ShooterCharacter);
	
	if (ShooterCharacter->HasWeaponFirstReplicated())
	{
		//get dynamic material instance from the weapon
	}
	else
	{
		ShooterCharacter->OnWeaponFirstReplicated.AddDynamic(this, &ThisClass::OnWeaponFirstReplicated);
	}
}

void UShooterReticle::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

void UShooterReticle::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	//Unbind from delegates on the old pawn's combat component
	//bind to delegates on the new pawn's combat component
}

void UShooterReticle::OnWeaponFirstReplicated(AWeapon* Weapon)
{
	//get dynamic material instance from the weapon
}
