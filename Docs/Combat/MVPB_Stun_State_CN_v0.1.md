# 共用 Stun 状态（v0.1）

日期：2026-10-27（Asia/Tokyo）。起始为干净 `main / 914cc53fd56a550bd06228edee04b1a9b2a393ad`，本机 UE 5.8.2-56702186。用户授权制作共用 Stun，明确不制作 Stun Zone；后续由另一个机关调用。既有状态文档顶部的 5.8.3／77 项属于另一设备历史，不作为本轮验证。

## 1. 规则

- `State.Stun` 由 `UHALStunEffect` 在 GAS 中授予；持续时间为来源传入的正有限游戏秒数。原生 GE 默认 1 秒仅为独立定义默认值，共用入口始终覆盖 Spec 时长。
- 服务器 `UHALAbilitySystemComponent::ApplyStun(DurationSeconds, SourceActor)` 是共用入口，不新增客户端施加 RPC。返回 true 也可能表示已有剩余时间已经覆盖本次请求。
- 多来源取 `max(原结束时间, 当前时间 + 本次时长)`，不累加强度或时长。短请求保留原效果；长请求先增加新效果再移除旧效果，调用完成后保留一个原生 Stun，刷新时标签不中断、不重复落球。
- Stun 期间禁止油门、转向、刹车／倒车、漂移、技能／通过 GAS 的道具使用、发射、捕球及自救。保持线／角速度、重力、外部冲击与 Chaos 碰撞，不冻结刚体、不清零速度。此时跳过车辆自定义驾驶／抓地／转向施力，刚体本身阻尼与接触仍存在。
- 首次进入 Stun 由服务器立即解除球约束，球 Controlled→Free；不主动发射、不增加脱球冲量或后坐。沿用原有重新捕获锁；Stun 本身持续阻止该车拾球，其他合格车辆仍可争夺。
- 既有加速／免疫效果继续计时；Stun 本身不修改 HP／免疫／冲击档位。无额外伤害、击退或表现资产。
- `Effect.Scope.Avatar` 沿用既有换车／退出清理，不把旧 Avatar 的 Stun 留给新车。

## 2. 来源接入

后续新增的主机控制台测试入口为 `HALStun <PlayerId> <秒数>`，先用 `HALGASStatus` 查询ID；该入口没有客户端任意目标RPC。详见[指令说明](../Configuration/Stun_Debug_Command_CN.md)及[独立验证记录](MVPB_Stun_DebugCommand_Verification_20261027.json)。以下机关／道具正式接入保持共用C++接口。

在服务器已经确认机关／道具合法命中的位置，取得目标的 `UHALAbilitySystemComponent` 后调用：

```cpp
TargetASC->ApplyStun(StunDurationSeconds, SourceActor);
```

来源 Actor 不需要 ASC。时长在来源的 C++ 配置中用 `UPROPERTY(EditDefaultsOnly)` 暴露秒数，校验必须有限且大于 0；不需要每种机关创建一个 Stun GE 子类，不直接添加 loose tag，也不绕过共用入口施加另一份 GE。

未来道具执行入口必须在服务器使用 `UHALAbilitySystemComponent::IsActorStunned(TargetVehicle)` 拒绝请求；若以 GAS Ability 执行，则现有 ASC 激活阻挡已涵盖。当前工程没有库存／道具使用框架，本轮只提供查询并验证 GAS 激活限制，不声称不存在的道具链路已实现。

表现层可订阅 ASC 的 `State.Stun` NewOrRemoved 事件；本轮未建设特效、动画、倒计时 UI 或机关蓝图。Mixed 复制下远端观察者可读取标签，不能假设拥有完整活动 GE 时长。

## 3. 物理与网络

`HALMovementEffectBridge` 保留原加速采样，同时监听 Stun 标签变化。服务器采样到的 `bControlsBlocked` 与加速度／速度倍率一并作为绝对快照发布，只有实际值变化才递增 Revision。

`FHALMovementSnapshot` 的序列化、相等比较、未设置判断都包含此位；`FHALMovementTimeline` 格式从 1 升为 2。两端必须使用同一版本重新编译，不与旧程序混连。继续沿用可靠 Authority 参数动作、实际物理帧边界、有界时间线和迟到回滚；不在物理线程访问 GAS，也不让客户端输入提供 Stun 位。

物理步按该历史帧状态清空主动输入并跳过驾驶施力，到期后恢复实时输入，持续按住油门会恢复驾驶；Stun 中的技能／发射离散请求不排队补发。回滚只重放物理权限，不重新施加 GE 或再次落球。球 Actor 的取得球权与主动发射入口均独立检查，避免只拦本地按键。

## 4. 验证与团队步骤

本轮构建／测试的实际结果见 [验证记录](MVPB_Stun_Verification_20261027.json)。不得以代码完成代替已运行验证。

本轮实际结果：UE5.8.2 Editor／Game Win64 Development完整链接成功；普通80/80、报告0警告／失败。新增3项Stun测试与扩展真实Solver回归通过；最终真实Listen Server双端各1/1、0警告／失败，客户端完成点35次实际重模拟、1次桥接回滚请求。主机出站80ms注入与60FPS上限不是实测RTT。首轮DLL占用、测试少头文件、联机夹具两端重力／就绪协商问题及最终修正分别留档；生产代码未因联机夹具失败放宽。1157份保存资产前后逐份SHA-256一致，四个专用联机进程均已退出，未提交／Push。普通回归在最后一次仅修改联机测试夹具前执行，最终Editor／Game均已重编该夹具；生产Stun代码在普通与联机验证之间未变。

团队接入后在 Host 与 Client 分别检查：

1. 驾驶／转向／漂移及持球时被机关 Stun，立即落球且不能拾取新球、发射、发动技能或自救。
2. 滑行／腾空／侧撞中 Stun，惯性、重力、受击与扣血仍正常，不被强制定住。
3. 3 秒 Stun 后重施 1 秒不缩短；再施 5 秒延长到本次起 5 秒，刷新无重复落球或状态闪断。
4. 到期／驱散后恢复；退出／换 Avatar 不残留。连续按住油门恢复，Stun 中按过的离散发射不会自行补发。
5. 其他车能抢走掉落球；同时存在加速／免疫时各自计时和效果保持独立。
6. 两端延迟、反复进入／结束及重模拟下无球权重复、无旧输入绕过。自动化 NullRHI／空中隔离不能代替真实地图、画面或 F4 完整验收。
