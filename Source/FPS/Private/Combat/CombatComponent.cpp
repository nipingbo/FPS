// Raymond Learn UE Project


#include "Combat/CombatComponent.h"

#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/WeaponData.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Weapon/Weapon.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAiming = false;
	bTriggerPressed = false;
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatComponent, Inventory);
	DOREPLIFETIME(UCombatComponent, CurrentWeapon);
	DOREPLIFETIME_CONDITION(UCombatComponent, bAiming, COND_SkipOwner);
}

void UCombatComponent::Initiate_CycleWeapon()
{
	GEngine->AddOnScreenDebugMessage(
		-1, 
		5.f, 
		FColor::Cyan, 
		TEXT("Initiate_CycleWeapon"), 
		false);
}

void UCombatComponent::Initiate_FireWeapon_Pressed()
{
	bTriggerPressed = true;
	if (CurrentWeapon->GetAmmo() > 0)
	{
		Local_FireWeapon();
	}
}

void UCombatComponent::Local_FireWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	ensure(IsValid(WeaponData));
	//play the fire weapon montage for the first person mesh
	UAnimMontage* Montage1P = WeaponData->FirstPersonMontages.FindChecked(CurrentWeapon->WeaponType).FireMontage;
	USkeletalMeshComponent* Mesh1P = IPlayerInterface::Execute_GetMesh1P(GetOwner());
	if (IsValid(Mesh1P) && IsValid(Montage1P))
	{
		Mesh1P->GetAnimInstance()->Montage_Play(Montage1P);
	}
	// 本地做 Trace 只用于立即播放本地特效（枪口火焰、弹孔贴花等），
	// 不把结果上传服务端，避免外挂伪造命中数据。
	FHitResult Hit;
	CurrentWeapon->WeaponTrace(Hit, CurrentWeapon->TraceLength);
	EPhysicalSurface ImpactSurfaceType = Hit.PhysMaterial.IsValid(false)? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType1;
	CurrentWeapon->Local_Fire(Hit.ImpactPoint, Hit.ImpactNormal, ImpactSurfaceType, true);

	GetWorld()->GetTimerManager().SetTimer(FireTimer, this, &ThisClass::FireTimerFinished, CurrentWeapon->FireTime);
	Server_FireWeapon();
}

void UCombatComponent::FireTimerFinished()
{
	if (!IsValid(CurrentWeapon)) return;
	if (bTriggerPressed && CurrentWeapon->FireType == EFireType::Auto && CurrentWeapon->GetAmmo() > 0)
	{
		Local_FireWeapon();
	}
}


// 服务端不使用客户端的 Hit 数据，而是用服务端自己的视角重新做 Trace，
// 保证命中判定在服务端权威数据上进行，客户端无法伪造结果。
void UCombatComponent::Server_FireWeapon_Implementation()
{
	if (!IsValid(CurrentWeapon)) return;
	//只有本地操控的client才需要更新自己的Ammo，别的client，包括server不用。
	if (GetNetMode() != NM_ListenServer || !Cast<APawn>(GetOwner())->IsLocallyControlled())
	{
		CurrentWeapon->Auth_Fire();
	}
	FHitResult Hit;
	CurrentWeapon->WeaponTrace(Hit, CurrentWeapon->TraceLength);
	Multicast_FireWeapon(Hit, CurrentWeapon->GetAmmo());
}

void UCombatComponent::Multicast_FireWeapon_Implementation(const FHitResult& Hit, int32 AuthAmmo)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsValid(Pawn)) return;

	if (Pawn->IsLocallyControlled())
	{
		//把武器当前的Ammo传过去，再更新
		CurrentWeapon->Rep_Fire(AuthAmmo);
	}
	else
	{
		if (!IsValid(CurrentWeapon)) return;
		ensure(IsValid(WeaponData));
		
		EPhysicalSurface ImpactSurfaceType = Hit.PhysMaterial.IsValid(false) ? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType1;
		CurrentWeapon->Local_Fire(Hit.ImpactPoint, Hit.ImpactNormal, ImpactSurfaceType, false);
		
		//play the fire weapon montage for the first person mesh
		UAnimMontage* Montage3P = WeaponData->ThirdPersonMontages.FindChecked(CurrentWeapon->WeaponType).FireMontage;
		if (const USkeletalMeshComponent* Mesh3P = IPlayerInterface::Execute_GetMesh3P(GetOwner()); IsValid(Mesh3P) && IsValid(Montage3P))
		{
			Mesh3P->GetAnimInstance()->Montage_Play(Montage3P);
		}
	}
}


void UCombatComponent::Initiate_FireWeapon_Released()
{
	bTriggerPressed = false;
}

void UCombatComponent::Initiate_ReloadWeapon()
{
	GEngine->AddOnScreenDebugMessage(
		-1, 
		5.f, 
		FColor::Cyan, 
		TEXT("Initiate_ReloadWeapon"), 
		false);
}

void UCombatComponent::Initiate_Aim_Pressed()
{
	Local_Aim(true);
	Server_Aim(true);
}

void UCombatComponent::Initiate_Aim_Released()
{
	Local_Aim(false);
	Server_Aim(false);
}

void UCombatComponent::Server_Aim_Implementation(bool bPressed)
{
	Local_Aim(bPressed);
}

void UCombatComponent::Local_Aim(bool bPressed)
{
	bAiming = bPressed;
}

void UCombatComponent::Equip(AWeapon* Weapon)
{
	CurrentWeapon = Weapon;
	CurrentWeapon->AttachToOwningPawn();
}

void UCombatComponent::SpawnInventory()
{
	if (GetOwner()->GetLocalRole() < ROLE_Authority) return;
	// 断线重连时 PossessedBy 会再次触发，Inventory 已有内容则不重复生成。
	if (Inventory.Num() > 0) return;
	for (auto& WeaponClass : DefaultWeaponClasses)
	{
		AWeapon* Weapon = SpawnWeapon(WeaponClass);
		Inventory.AddUnique(Weapon);
	}
	
	if (Inventory.Num() > 0)
	{
		Equip(Inventory[0]);
	}
}

void UCombatComponent::DestroyInventory()
{
	for (AWeapon* Weapon : Inventory)
	{
		if (IsValid(Weapon))
		{
			Weapon->Destroy();
		}
	}
}

void UCombatComponent::OnRep_CurrentWeapon(AWeapon* LastWeapon)
{
	// Inventory 和 CurrentWeapon 都是 Replicated，但 Actor 属性和 Actor 本体的
	// 复制顺序不保证。此处 CurrentWeapon 无效说明 Weapon Actor 本体还未到达客户端，
	// 直接返回即可——AWeapon::OnRep_Instigator 会在 Actor 就绪后补调 AttachToOwningPawn。
	if (!IsValid(CurrentWeapon)) return;
	CurrentWeapon->AttachToOwningPawn();
}


AWeapon* UCombatComponent::SpawnWeapon(TSubclassOf<AWeapon> WeaponClass) const
{
	AActor* OwningActor = GetOwner();
	if (!IsValid(OwningActor)) return nullptr;
	if (OwningActor->GetLocalRole() < ROLE_Authority) return nullptr;
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Instigator = Cast<APawn>(OwningActor);
	SpawnParams.Owner = OwningActor;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	return GetWorld()->SpawnActor<AWeapon>(WeaponClass, SpawnParams);
}



