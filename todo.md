# TODO — 项目完成后待评审/可增强项

> 目的：当前项目功能已基本跑通，以下为后续增强/加固的候选清单。
> 等整个项目完成后，再一步步回来核对：是否需要做、改动成本、收益。
> 已完成的项可以勾掉。

## 1. 网络/复制可靠性

- [ ] `Weapon::OnRep_Ammo` 加 `Sequence > 0` 守卫
  - 现状：`OnRep_Ammo` 无守卫地执行 `--Sequence; Ammo = Clamp(Ammo - Sequence)`，
        会把每次复制都当"开了一枪"对账，reload 这类非开枪的 Ammo 变化会被算错，
        且可能把 reload 后的弹药显示抬错（Sequence 为 0 时 `--Sequence = -1`）。
  - 建议：
    ```cpp
    void AWeapon::OnRep_Ammo()
    {
        if (Sequence > 0)
        {
            --Sequence;
            Ammo = FMath::Clamp(Ammo - Sequence, 0, MagCapacity);
        }
    }
    ```

- [ ] Reload 完成改为"服务器确定性完成"（不再依赖动画通知在服务器触发）
  - 现状：客户端弹药是否增加，完全依赖服务器对远程 Pawn 播 3P 装弹蒙太奇、
        且蒙太奇上的通知在服务器上触发 `Notify_ReloadWeapon` 后调 `Client_ReloadWeapon`。
        任一环节断（3P 蒙太奇没配/通知没挂/服务器动画未推进）客户端就不涨弹。
  - 建议：`Server_ReloadWeapon_Implementation` 用服务器定时器（时长=Reload 动画长度）
        到点后直接扣弹并下发 `Client_ReloadWeapon`，摆脱对动画通知的依赖；
        `Notify_ReloadWeapon` 退化为仅本地状态复位。需给武器加 `ReloadTime` 字段。

- [ ] `Multicast_FireWeapon` 从 `Reliable` 改为 `Unreliable`
  - 现状：自动步枪每发都用可靠组播，占用带宽并可能触发可靠通道排队。
  - 建议：命中特效丢一两发可接受，`Multicast, Unreliable`。

- [ ] 服务端射速限流增加容差
  - 现状：用相邻两次 RPC 到达的服务端墙钟间隔 < `FireTime` 判拒绝，
        网络抖动可能导致合法开枪被误拒（客户端弹药被拉回、画面闪跳）。
  - 建议：留容差（如 `FireTime * 0.5`）或服务端自维护独立射击节奏。

## 2. 稳定性 / 崩溃风险

- [ ] `BlendOut_CycleWeapon` 增加判空
  - 现状：`IPlayerInterface::Execute_GetMesh1P(GetOwner())->GetAnimInstance()` 和
        `CurrentWeapon->WeaponStatus` 无有效性判空，角色销毁后 montage 才 blend out 会空指针崩溃。

- [ ] `TickComponent` / `WeaponTrace` 忽略武器自身/其他装备武器
  - 现状：只 `AddIgnoredActor(GetOwner())`，若武器网格带碰撞，球体扫描可能命中自己枪身产生假命中。

- [ ] `CalculateFABRIKSocketTransform` 对 `FABRIK_Socket` 做存在性判断
  - 现状：每帧 `GetSocketTransform("FABRIK_Socket")`，socket 缺失会每帧打 warning。

- [ ] `bHitPlayer` 显式初始化 `= false`

## 3. 未完成功能（可后续补齐）

- [ ] Cycle Weapon 真正切换 `CurrentWeapon`
  - 现状：只播换枪动画、改状态，未调用 `EquipWeapon`；`NextWeapon` 状态卡在 `Cycling`。
  - 需把 `EquipWeapon` 接入 `Notify_CycleWeapon`/`BlendOut` 流程，并复位 `NextWeapon` 状态。

- [ ] Reserve ammo 完整弹药系统
  - 现状：`Initiate_ReloadWeapon` 已能装填，但备弹消耗/补充、换枪备弹联动等仍需验证与完善。

## 4. 已确认并说明的项（可选复核）

- [ ] reload 完成后 `Notify_ReloadWeapon` 中 `Client_ReloadWeapon` 与 `OnRep_Ammo` 的执行顺序竞争
  - 修好第 1 组 `OnRep_Ammo` 守卫后，此问题基本消解，可复核一次。
