// Raymond Learn UE Project


#include "Combat/CombatComponent.h"

#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/WeaponData.h"
#include "Engine/Engine.h"
#include "FPS/FPS.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Weapon/Weapon.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAiming = false;
	bTriggerPressed = false;
	Local_WeaponIndex = 0;
}

void UCombatComponent::TickComponent(float DeltaTime, enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || !OwningPawn->IsLocallyControlled()) return;
	
	APlayerController* PlayerController = Cast<APlayerController>(OwningPawn->GetController());
	if (!IsValid(PlayerController)) return;
	
	FVector EyesWorldLocation;
	FRotator EyesWorldRotation;
	PlayerController->GetPlayerViewPoint(EyesWorldLocation, EyesWorldRotation);
	const FVector EyesWorldDirection = UKismetMathLibrary::GetForwardVector(EyesWorldRotation);
	
	const FVector Start = EyesWorldLocation;
	if (!IsValid(CurrentWeapon)) return;
	const FVector End = Start + EyesWorldDirection * CurrentWeapon->TraceLength;

	FHitResult Hit;
	FCollisionQueryParams CollisionQueryParams;
	CollisionQueryParams.AddIgnoredActor(GetOwner());
	
	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, FPSTraceChannels::ECC_Weapon, CollisionQueryParams);
	/*FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse.SetAllChannels(ECR_Ignore);
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Block);
	ResponseParams.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Block);
	
	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, FPSTraceChannels::ECC_Weapon, CollisionQueryParams, ResponseParams);*/
	
	bHitPlayer = IsValid(Hit.GetActor()) && Hit.GetActor()->Implements<UPlayerInterface>();
	
	if (bHitPlayer != bHitPlayerLastFrame)
	{
		OnTargetingPlayerStatusChanged.Broadcast(bHitPlayer);
	}
	
	bHitPlayerLastFrame = bHitPlayer;
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatComponent, Inventory);
	DOREPLIFETIME(UCombatComponent, CurrentWeapon);
	DOREPLIFETIME_CONDITION(UCombatComponent, bAiming, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UCombatComponent, CurrentReserveAmmo, COND_OwnerOnly);
}

void UCombatComponent::Initiate_CycleWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling) return;
	
	AdvanceWeaponIndex();
	Local_CycleWeapon(Local_WeaponIndex);
}

void UCombatComponent::Local_CycleWeapon(int32 WeaponIndex)
{
	// 索引来自客户端 RPC，必须校验范围；CurrentWeapon 可能尚未复制到本端
	if (!Inventory.IsValidIndex(WeaponIndex)) return;
	AWeapon* NextWeapon = Inventory[WeaponIndex];
	if (!IsValid(NextWeapon) || !IsValid(CurrentWeapon) || !IsValid(WeaponData)) return;
	CurrentWeapon->WeaponStatus = EWeaponStatus::Cycling;
	NextWeapon->WeaponStatus = EWeaponStatus::Cycling;
	
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	const bool bIsLocal = IsValid(OwningPawn) && OwningPawn->IsLocallyControlled();
	
	// 只在 montage 真正播放成功时才绑定 blend-out，避免误触发
	const bool bMontagePlayed = PlayMontageOnMesh(
		GetViewMesh(bIsLocal),
		GetBodyMontage(NextWeapon->WeaponType, ECombatMontageSlot::Equip, bIsLocal));

	if (bIsLocal)
	{
		Server_CycleWeapon(WeaponIndex);
		if (bMontagePlayed)
		{
			if (UAnimInstance* AnimInstance = GetViewMesh(bIsLocal)->GetAnimInstance(); IsValid(AnimInstance))
			{
				AnimInstance->OnMontageBlendingOut.AddDynamic(this, &ThisClass::BlendOut_CycleWeapon);
			}
		}
	}
}


void UCombatComponent::Server_CycleWeapon_Implementation(int32 WeaponIndex)
{
	// 客户端传来的索引不可信，越界会导致所有端 Inventory[WeaponIndex] 崩溃
	if (!Inventory.IsValidIndex(WeaponIndex)) return;
	Local_WeaponIndex = WeaponIndex;
	Multicast_CycleWeapon(WeaponIndex);
}

void UCombatComponent::Multicast_CycleWeapon_Implementation(int32 WeaponIndex)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn)) return;
	
	if (!OwningPawn->IsLocallyControlled())
	{
		Local_WeaponIndex = WeaponIndex;
		Local_CycleWeapon(WeaponIndex);
	}
}

void UCombatComponent::Initiate_FireWeapon_Pressed()
{
	if (!IsValid(CurrentWeapon)) return;
	bTriggerPressed = true;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Idle && CurrentWeapon->GetAmmo() > 0)
	{
		Local_FireWeapon();
	}
}

void UCombatComponent::Local_FireWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	ensure(IsValid(WeaponData));
	
	CurrentWeapon->WeaponStatus = EWeaponStatus::Firing;
	//play the fire weapon montage for the first person mesh
	PlayMontageOnMesh(
		GetViewMesh(true),
		GetBodyMontage(CurrentWeapon->WeaponType, ECombatMontageSlot::Fire, true));
	// 本地做 Trace 只用于立即播放本地特效（枪口火焰、弹孔贴花等），
	// 不把结果上传服务端，避免外挂伪造命中数据。
	FHitResult Hit;
	CurrentWeapon->WeaponTrace(Hit, CurrentWeapon->TraceLength);
	EPhysicalSurface ImpactSurfaceType = Hit.PhysMaterial.IsValid(false)? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType1;
	CurrentWeapon->Local_Fire(Hit.ImpactPoint, Hit.ImpactNormal, ImpactSurfaceType, true);
	
	OnRoundFired.Broadcast(CurrentWeapon->GetAmmo(), CurrentWeapon->GetMagCapacity(), CurrentReserveAmmo);
	GetWorld()->GetTimerManager().SetTimer(FireTimer, this, &ThisClass::FireTimerFinished, CurrentWeapon->FireTime);
	Server_FireWeapon();
}

// 服务端不使用客户端的 Hit 数据，而是用服务端自己的视角重新做 Trace，
// 保证命中判定在服务端权威数据上进行，客户端无法伪造结果。
void UCombatComponent::Server_FireWeapon_Implementation()
{
	if (!IsValid(CurrentWeapon)) return;
	if (CurrentWeapon->GetAmmo() <= 0) return;
	// 服务端校验射速：拒绝间隔不足 FireTime 的请求，防止客户端绕过本地 Timer 加速射击
	const float Now = GetWorld()->GetTimeSeconds();
	if (LastFireTime >= 0.f && (Now - LastFireTime) < CurrentWeapon->FireTime)
	{
		Client_FireRejected(CurrentWeapon->GetAmmo());
		return;
	}
	LastFireTime = Now;

	// Listen Server 上的本地玩家（Host）已在 Local_Fire 里预测扣弹，跳过避免双重扣弹。
	// 其他情况（Dedicated Server、Listen Server 上的远程客户端）均需在此执行权威扣弹。
	if (GetNetMode() != NM_ListenServer || !Cast<APawn>(GetOwner())->IsLocallyControlled())
	{
		if (!CurrentWeapon->Auth_Fire())
		{
			Client_FireRejected(CurrentWeapon->GetAmmo());
			return;
		}
	}
	FHitResult Hit;
	CurrentWeapon->WeaponTrace(Hit, CurrentWeapon->TraceLength);
	Multicast_FireWeapon(Hit);
}

void UCombatComponent::Client_FireRejected_Implementation(int32 AuthAmmo)
{
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->ResetPrediction(AuthAmmo);
	}
}

void UCombatComponent::Multicast_FireWeapon_Implementation(const FHitResult& Hit)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsValid(Pawn)) return;

	if (Pawn->IsLocallyControlled())
	{
		// Ammo 校正由 OnRep_Ammo 负责，Multicast 只需触发本地已预测过的效果
	}
	else
	{
		if (!IsValid(CurrentWeapon)) return;
		ensure(IsValid(WeaponData));

		EPhysicalSurface ImpactSurfaceType = Hit.PhysMaterial.IsValid(false) ? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType1;
		CurrentWeapon->Local_Fire(Hit.ImpactPoint, Hit.ImpactNormal, ImpactSurfaceType, false);

		PlayMontageOnMesh(
			GetViewMesh(false),
			GetBodyMontage(CurrentWeapon->WeaponType, ECombatMontageSlot::Fire, false));
	}
}

int32 UCombatComponent::AdvanceWeaponIndex()
{
	if (Inventory.Num() >=2)
	{
		Local_WeaponIndex = (Local_WeaponIndex + 1) % Inventory.Num();
	}
	return Local_WeaponIndex;
}

void UCombatComponent::Notify_CycleWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	if (!Inventory.IsValidIndex(Local_WeaponIndex)) return;
	AWeapon* NewWeapon = Inventory[Local_WeaponIndex];
	if (IsValid(NewWeapon))
	{
		EquipWeapon(NewWeapon);
	}
}

void UCombatComponent::BlendOut_CycleWeapon(UAnimMontage* Montage, bool bInterrupted)
{
	UAnimInstance* AnimInstance = IPlayerInterface::Execute_GetMesh1P(GetOwner())->GetAnimInstance();
	if (IsValid(AnimInstance) && AnimInstance->OnMontageBlendingOut.IsAlreadyBound(this, &ThisClass::BlendOut_CycleWeapon))
	{
			AnimInstance->OnMontageBlendingOut.RemoveDynamic(this, &ThisClass::BlendOut_CycleWeapon);
	}
	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	OnReticleChanged.Broadcast(CurrentWeapon->GetReticleDynamicMaterialInstance(), CurrentWeapon->ReticleParams, bHitPlayer);
	OnAmmoCounterChanged.Broadcast(CurrentWeapon->GetAmmoCounterDynamicMaterialInstance(), CurrentWeapon->GetAmmo(), CurrentWeapon->GetMagCapacity());
	OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->GetAmmo(), CurrentWeapon->WeaponIcon);
	
	if (bTriggerPressed && CurrentWeapon->FireType == EFireType::Auto && CurrentWeapon->GetAmmo() > 0)
	{
		Local_FireWeapon();
	}
}

void UCombatComponent::FireTimerFinished()
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(CurrentWeapon) || !IsValid(OwningPawn)) return;
	//auto reload
	if (CurrentWeapon->GetAmmo() == 0 && CurrentReserveAmmo > 0 && OwningPawn->IsLocallyControlled())
	{
		Local_ReloadWeapon();
		Server_ReloadWeapon();
		return;
	}
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Firing)
	{
		CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	}
	if (bTriggerPressed && CurrentWeapon->FireType == EFireType::Auto && CurrentWeapon->GetAmmo() > 0)
	{
		Local_FireWeapon();
	}
}

void UCombatComponent::Initiate_FireWeapon_Released()
{
	bTriggerPressed = false;
}

void UCombatComponent::Initiate_ReloadWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	if (CurrentWeapon->WeaponStatus == EWeaponStatus::Cycling || CurrentWeapon->WeaponStatus == EWeaponStatus::Reloading) return;
	if (CurrentWeapon->GetAmmo() == CurrentWeapon->GetMagCapacity()) return;
	if (CurrentReserveAmmo == 0) return;
	Local_ReloadWeapon();
	Server_ReloadWeapon();
}

void UCombatComponent::Local_ReloadWeapon()
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(CurrentWeapon) || !IsValid(OwningPawn)) return;
	ensure(WeaponData);
	
	const bool bIsLocal = OwningPawn->IsLocallyControlled();

	// 身体动画：与换枪同一条查找路径
	PlayMontageOnMesh(
		GetViewMesh(bIsLocal),
		GetBodyMontage(CurrentWeapon->WeaponType, ECombatMontageSlot::Reload, bIsLocal));

	// 武器自身动画：Reload 特有，走独立的 WeaponMontages 表
	PlayMontageOnMesh(
		bIsLocal ? CurrentWeapon->GetMesh1P() : CurrentWeapon->GetMesh3P(),
		GetWeaponMontage(CurrentWeapon->WeaponType, ECombatMontageSlot::Reload));

	CurrentWeapon->WeaponStatus = EWeaponStatus::Reloading;
}

void UCombatComponent::Notify_ReloadWeapon()
{
	if (!IsValid(CurrentWeapon)) return;
	if (GetNetMode() == NM_ListenServer || GetNetMode() == NM_DedicatedServer || GetNetMode() == NM_Standalone)
	{
		const int32 EmptySpace = CurrentWeapon->GetMagCapacity() - CurrentWeapon->GetAmmo();
		const int32 AmountToRefill = FMath::Min(EmptySpace, CurrentReserveAmmo);
		CurrentWeapon->SetAmmo(AmountToRefill + CurrentWeapon->GetAmmo());
		ReserveAmmo[CurrentWeapon->WeaponType] -= AmountToRefill;
		CurrentReserveAmmo = ReserveAmmo[CurrentWeapon->WeaponType];
		Client_ReloadWeapon(CurrentWeapon->GetAmmo(), CurrentReserveAmmo);
	}
	CurrentWeapon->WeaponStatus = EWeaponStatus::Idle;
	if (bTriggerPressed && CurrentWeapon->GetAmmo() > 0)
	{
		Local_FireWeapon();
	}
}

void UCombatComponent::AddAmmo(const FGameplayTag& WeaponType, int32 AmmoAmount)
{
	if (GetOwner()->HasAuthority() && !IsValid(CurrentWeapon)) return;
	if (!ReserveAmmo.Contains(WeaponType))
	{
		ReserveAmmo.Add(WeaponType, AmmoAmount);
	}
	else
	{
		const int32 NewAmmo = ReserveAmmo.FindChecked(WeaponType) + AmmoAmount;
		ReserveAmmo[WeaponType] = NewAmmo;
	
		if (CurrentWeapon->WeaponType.MatchesTagExact(WeaponType))
		{
			CurrentReserveAmmo = NewAmmo;
			if (CurrentWeapon->GetAmmo() == 0 && NewAmmo > 0)
			{
				Server_ReloadWeapon();
			}
			OnAmmoCounterChanged.Broadcast(CurrentWeapon->GetAmmoCounterDynamicMaterialInstance(),CurrentWeapon->GetAmmo(),CurrentWeapon->GetMagCapacity());
			OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->GetAmmo(), CurrentWeapon->WeaponIcon);
		}
	}

}

void UCombatComponent::Client_ReloadWeapon_Implementation(int32 NewWeaponAmmo, int32 NewCarriedAmmo)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(CurrentWeapon) || !IsValid(OwningPawn)) return;
	
	if (OwningPawn->IsLocallyControlled())
	{
		CurrentWeapon->SetAmmo(NewWeaponAmmo);
		CurrentReserveAmmo = NewCarriedAmmo;
		
		OnAmmoCounterChanged.Broadcast(CurrentWeapon->GetAmmoCounterDynamicMaterialInstance(), CurrentWeapon->GetAmmo(), CurrentWeapon->GetMagCapacity());
		OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->GetAmmo(), CurrentWeapon->WeaponIcon);
	}
}

bool UCombatComponent::PlayMontageOnMesh(const USkeletalMeshComponent* Mesh, UAnimMontage* Montage) const
{
	if (!IsValid(Mesh) || !IsValid(Montage)) return false;
	if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance(); IsValid(AnimInstance))
	{
		AnimInstance->Montage_Play(Montage);
		return true;
	}
	return false;
}

const USkeletalMeshComponent* UCombatComponent::GetViewMesh(bool bIsLocal) const
{
	return bIsLocal
		? IPlayerInterface::Execute_GetMesh1P(GetOwner())
		: IPlayerInterface::Execute_GetMesh3P(GetOwner());
}

UAnimMontage* UCombatComponent::GetBodyMontage(const FGameplayTag& WeaponType, ECombatMontageSlot Slot, bool bIsLocal) const
{
	if (!IsValid(WeaponData)) return nullptr;

	const TMap<FGameplayTag, FMontageData>& Montages = bIsLocal
		? WeaponData->FirstPersonMontages
		: WeaponData->ThirdPersonMontages;

	if (const FMontageData* Data = Montages.Find(WeaponType))
	{
		switch (Slot)
		{
		case ECombatMontageSlot::Equip: return Data->EquipMontage;
		case ECombatMontageSlot::Reload: return Data->ReloadMontage;
		case ECombatMontageSlot::Fire: return Data->FireMontage;
		default: return nullptr;
		}
	}
	return nullptr;
}

UAnimMontage* UCombatComponent::GetWeaponMontage(const FGameplayTag& WeaponType, ECombatMontageSlot Slot) const
{
	if (!IsValid(WeaponData)) return nullptr;

	if (const FMontageData* Data = WeaponData->WeaponMontages.Find(WeaponType))
	{
		switch (Slot)
		{
		case ECombatMontageSlot::Equip: return Data->EquipMontage;
		case ECombatMontageSlot::Reload: return Data->ReloadMontage;
		case ECombatMontageSlot::Fire: return Data->FireMontage;
		default: return nullptr;
		}
	}
	return nullptr;
}

void UCombatComponent::Server_ReloadWeapon_Implementation()
{
	Multicast_ReloadWeapon();
}

void UCombatComponent::Multicast_ReloadWeapon_Implementation()
{
	Local_ReloadWeapon();
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

void UCombatComponent::OnRep_CurrentReserveAmmo()
{
	if (IsValid(CurrentWeapon))
	{
		OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, CurrentWeapon->GetAmmo(), CurrentWeapon->WeaponIcon);
	}
}

void UCombatComponent::Local_Aim(bool bPressed)
{
	bAiming = bPressed;
	OnAimingStatusChanged.Broadcast(bAiming);
}

void UCombatComponent::Equip(AWeapon* Weapon)
{
	CurrentWeapon = Weapon;
	CurrentWeapon->AttachToOwningPawn(Cast<APawn>(GetOwner()));

	if (const int32* FoundReserve = ReserveAmmo.Find(Weapon->WeaponType))
	{
		CurrentReserveAmmo = *FoundReserve;
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Equip: no reserve ammo mapped for weapon type %s — defaulting to 0"),
			*Weapon->WeaponType.ToString());
		CurrentReserveAmmo = 0;
	}

	OnCurrentReserveAmmoChanged.Broadcast(CurrentReserveAmmo, Weapon->GetAmmo(), Weapon->WeaponIcon);
}

void UCombatComponent::EquipWeapon(AWeapon* Weapon)
{
	if (!IsValid(Weapon) || !IsValid(GetOwner())) return;
	if (GetOwner()->GetLocalRole() == ROLE_Authority)
	{
		SetCurrentWeapon(Weapon, CurrentWeapon);
	}
	else
	{
		Server_EquipWeapon(Weapon);
	}
}

void UCombatComponent::Server_EquipWeapon_Implementation(AWeapon* Weapon)
{
	// 只接受库存内的武器，防止客户端传任意 Weapon Actor 作弊
	if (!Inventory.Contains(Weapon)) return;
	EquipWeapon(Weapon);
}

void UCombatComponent::SetCurrentWeapon(AWeapon* NewWeapon, AWeapon* LastWeapon)
{
	AWeapon* LocalLastWeapon = nullptr;
	if (IsValid(LastWeapon))
	{
		LocalLastWeapon = LastWeapon;
	}
	else if (NewWeapon != CurrentWeapon)
	{
		LocalLastWeapon = CurrentWeapon;
	}
	if (IsValid(LocalLastWeapon))
	{
		LocalLastWeapon->DetachFromOwningPawn();
		LocalLastWeapon->WeaponStatus = EWeaponStatus::Unequipped;
	}
	
	CurrentWeapon = NewWeapon;
	
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn)) return;
	if (OwningPawn->HasAuthority() && IsValid(CurrentWeapon))
	{
		CurrentReserveAmmo = ReserveAmmo.FindChecked(CurrentWeapon->WeaponType);
	}
	
	CurrentWeapon->AttachToOwningPawn(OwningPawn);
	if (CurrentWeapon->GetAmmo() == 0 && CurrentReserveAmmo > 0 && OwningPawn->IsLocallyControlled())
	{
		Local_ReloadWeapon();
		Server_ReloadWeapon();
	}
}


void UCombatComponent::SpawnInventory()
{
	if (GetOwner()->GetLocalRole() < ROLE_Authority) return;
	// 断线重连时 PossessedBy 会再次触发，Inventory 已有内容则不重复生成。
	if (Inventory.Num() > 0) return;
	for (auto& WeaponClass : DefaultWeaponClasses)
	{
		AWeapon* Weapon = SpawnWeapon(WeaponClass);
		if (!IsValid(Weapon))
		{
			UE_LOG(LogTemp, Warning, TEXT("SpawnInventory: DefaultWeaponClasses contains an invalid entry, skipping"));
			continue;
		}
		Inventory.AddUnique(Weapon);
		ReserveAmmo.Add(Weapon->WeaponType, Weapon->GetStartingCarriedAmmo());
	}
	
	if (Inventory.Num() > 0)
	{
		Equip(Inventory[0]);
		InitializeWeaponWidgets();
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

void UCombatComponent::InitializeWeaponWidgets() const
{
	// Broadcast if weapon is ready
	if (IsValid(CurrentWeapon))
	{
		OnReticleChanged.Broadcast(CurrentWeapon->GetReticleDynamicMaterialInstance(), CurrentWeapon->ReticleParams, bHitPlayer);
		OnAmmoCounterChanged.Broadcast(CurrentWeapon->GetAmmoCounterDynamicMaterialInstance(), CurrentWeapon->GetAmmo(), CurrentWeapon->GetMagCapacity());
	}
}

void UCombatComponent::OnRep_CurrentWeapon(AWeapon* LastWeapon)
{
	// Inventory 和 CurrentWeapon 都是 Replicated，但 Actor 属性和 Actor 本体的
	// 复制顺序不保证。此处 CurrentWeapon 无效说明 Weapon Actor 本体还未到达客户端，
	// 直接返回即可——AWeapon::OnRep_Instigator 会在 Actor 就绪后补调 AttachToOwningPawn。
	if (!IsValid(CurrentWeapon)) return;

	// 在客户端卸下旧武器：SetCurrentWeapon 的 Detach 只在 authority 端执行，
	// 而 SetHiddenInGame 不复制，客户端必须自己隐藏旧武器，否则换枪后新旧武器
	// 同时可见，画面从第二次切换起不再变化
	if (IsValid(LastWeapon) && LastWeapon != CurrentWeapon)
	{
		LastWeapon->DetachFromOwningPawn();
		LastWeapon->WeaponStatus = EWeaponStatus::Unequipped;
	}

	CurrentWeapon->AttachToOwningPawn(Cast<APawn>(GetOwner()));
	IPlayerInterface::Execute_WeaponReplicated(GetOwner());
	InitializeWeaponWidgets();
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



