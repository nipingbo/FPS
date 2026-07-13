# 联机前修复清单

---

## 1. 服务端开火验证

### 问题
客户端本地做 Trace，把 `FHitResult` 发给服务端，服务端直接信任并广播。
外挂可以伪造命中位置和目标。

### 当前流程
```
客户端 Local_FireWeapon()
  → CurrentWeapon->WeaponTrace(Hit)
  → Server_FireWeapon(Hit)          // 把客户端结果直接上传
    → Multicast_FireWeapon(Hit)     // 服务端广播客户端数据
```

### 目标流程
```
客户端 Local_FireWeapon()
  → CurrentWeapon->WeaponTrace(Hit) // 本地预测，只播放本地特效
  → Server_FireWeapon()             // 只通知服务端"我开火了"，不传 Hit
    → 服务端自己 WeaponTrace(Hit)   // 服务端用自己的视角重新 Trace
    → Multicast_FireWeapon(Hit)     // 广播服务端 Trace 结果
```

### 需要修改的地方

**CombatComponent.h**
```cpp
// 改前
UFUNCTION(Server, Reliable)
void Server_FireWeapon(const FHitResult& Hit);

// 改后
UFUNCTION(Server, Reliable)
void Server_FireWeapon();
```

**CombatComponent.cpp — Local_FireWeapon()**
```cpp
// 改前
Server_FireWeapon(Hit);

// 改后
Server_FireWeapon();
```

**CombatComponent.cpp — Server_FireWeapon_Implementation()**
```cpp
// 改前
void UCombatComponent::Server_FireWeapon_Implementation(const FHitResult& Hit)
{
    Multicast_FireWeapon(Hit);
}

// 改后
void UCombatComponent::Server_FireWeapon_Implementation()
{
    if (!IsValid(CurrentWeapon)) return;
    FHitResult Hit;
    CurrentWeapon->WeaponTrace(Hit, CurrentWeapon->TraceLength);
    Multicast_FireWeapon(Hit);
}
```

---

## 2. CurrentWeapon 网络时序保护

### 问题
`Inventory` 和 `CurrentWeapon` 都是 Replicated，但 Actor 属性复制和 Actor 本身的复制顺序不保证。
`OnRep_CurrentWeapon` 触发时，`CurrentWeapon` 所指向的 `AWeapon` Actor
可能还没复制到客户端（`IsValid` 返回 false），导致 `AttachToOwningPawn` 静默失败，武器不可见。

### 已有的补救机制
`AWeapon::OnRep_Instigator` 里已经调用了 `AttachToOwningPawn()`。
当 Weapon Actor 本体复制完成后，`OnRep_Instigator` 会触发，可以作为兜底。

### 需要修改的地方

**CombatComponent.cpp — OnRep_CurrentWeapon()**
```cpp
// 改前
void UCombatComponent::OnRep_CurrentWeapon(AWeapon* LastWeapon)
{
    if (!IsValid(CurrentWeapon)) return;
    CurrentWeapon->AttachToOwningPawn();
}

// 改后（加注释说明兜底机制）
void UCombatComponent::OnRep_CurrentWeapon(AWeapon* LastWeapon)
{
    // 如果 Weapon Actor 本体还未复制过来，此处跳过。
    // AWeapon::OnRep_Instigator 会在 Actor 就绪后补调 AttachToOwningPawn。
    if (!IsValid(CurrentWeapon)) return;
    CurrentWeapon->AttachToOwningPawn();
}
```

注意：`OnRep_CurrentWeapon` 的逻辑本身不变，关键是确认 `AWeapon::OnRep_Instigator`
中的 `AttachToOwningPawn` 调用保持存在（当前 Weapon.cpp:44 已有，不要删除）。

如果测试中发现武器偶尔不显示，可以考虑在 `OnRep_CurrentWeapon` 中额外注册一个
`FTimerHandle` 做延迟重试，但通常 `OnRep_Instigator` 的兜底已经足够。
