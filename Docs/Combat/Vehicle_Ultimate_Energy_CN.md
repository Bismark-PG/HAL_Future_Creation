# 车辆大招能量

车辆共用 100 点容量，每辆车单独配置从 0 自然回满需要的游戏秒数。若 Car04 配置 35 秒，每秒恢复约 2.86 点。拾取 25 点相当于提前 8.75 秒攒满。同一个道具给回满时间 20 秒的车也加 25 点，相当于提前 5 秒。

## 配置入口

| 用途 | Unreal Editor 中的位置 |
| --- | --- |
| 单车从 0 回到 100 的秒数 | 在 `/Game/Data/MVP_B/DA_Vehicle_Car_XX` 展开 `Abilities`，或在 Details 搜索 `Full Recharge Seconds`。范围 0.1～600 秒 |
| 正式局开局点数 | 当前房间使用的 `DA_Room_Development` 或 `DA_Room_Exhibition → Match Rules → Starting Ultimate Energy`，默认 0 |
| 技能持续时间、喷射时长、同目标受击间隔 | 各技能原有的技能 DA |
| 自定义 HUD | `UHALMatchHUDWidget.GetLocalUltimateEnergy()` 可读取点数、消耗、是否足够、是否可施放及回满秒数。Designer 自绘时可关闭 `Show Native Ultimate Meter` |

| 车型 | 0 → 100 | 一次消耗 |
| --- | ---: | ---: |
| Car01 钩爪 | 15 s | 100 |
| Car03 陀螺 | 25 s | 100 |
| Car04 蛮牛冲撞 | 35 s | 100 |
| Car05 龙卷风 | 16 s | 100 |
| Car06 追捕 | 18 s | 100 |
| Car07 换线 | 20 s | 25 |
| Car08 烟幕 | 18 s | 100 |
| 沿用 PlayerState 默认主技能的 Car02、09、10 | 20 s | 100 |

## 施放与比赛

- 服务器在技能真正发动后扣能。输入被拒绝、姿态不符、配置无效或生成失败都保留能量。已经发出的钩爪落空、冲锋被迫停止，仍算一次施放。
- 驾驶 HUD 的能量条按服务器采样时间平滑前进，点数、可用格数和施放许可以服务器确认结果为准。Car02、09、10 沿用当前 PlayerState 的默认加速技能，车型仍可各自配置回满时间。
- 发动后立即开始自然恢复，技能持续和眩晕期间照常恢复。提前回满仍须满足姿态和技能自身的重入条件。HUD 分别显示 `CHARGING`、`READY`、`BLOCKED`。Car07 的就绪表示当前可用一次换线，按键时还须给出有效左右方向。
- Car03、Car04 的同目标伤害间隔继续独立计时。补能不会令持续贴靠的目标多受一次伤害。
- Car07 每 25 点显示一格。当前 40 点时，换线后剩 15 点，再拾取 10 点即可获得一格。连续换线仍须满足动作的间隔。
- Car08 提前回满时，待本次喷射窗口结束即可再次喷射。眩晕或腾空只暂停喷射。已经落地的烟团按各自寿命继续作用，同一目标进出重叠烟云时按来源清理。
- 练习和独立车型测试从满能量开始。正式比赛按房间配置重新设定初始能量，进入 Playing 后开始恢复。加载、倒数、结算和淘汰期间停止恢复。下一局重新初始化，重复绑定同一辆车保留当前点数。

## 后续道具与机关

在服务器确认拾取者和拾取资格后，从该车辆取得 `UHALAbilitySystemComponent`，调用 `GrantUltimateEnergy(Points)`。返回值是实际增加的点数。道具只有在返回值大于 0 时才消耗或开始刷新。90 点时吃到 25 点，返回 10。满能量时返回 0，下一辆车仍可拾取。持续回能区域可以按其服务器计时调用同一接口。

能量属于 PlayerState 上的 GAS AttributeSet，服务器将点数和可施放状态复制给该玩家。新主技能继承 `UHALVehicleUltimateAbility`，在 `StartUltimate` 返回实际发动结果。持续型技能在限制再次施放的阶段结束时结束 GA。

## 联调

1. 在练习场确认车型后查看屏幕下方的大招读数。主机或客户端可用原主技能键。开启 `hal.DebugHUD 1` 可读小数点数和车型回满秒数。
2. 在主机控制台运行 `HALGASStatus` 找到目标 `PlayerId`，再用 `HALGrantUltimateEnergy 257 25`，把 257 替换成实测编号。输出中的 `Accepted` 是实际恢复量。可在对方技能仍生效时补满，比较 `BLOCKED` 和 `READY`。
3. 从练习场进入正式比赛。确认倒数期间没有攒能，开赛后开始增长。返回房间并再开一局，检查同样的开局规则。

<a id="ultimate-energy-tests"></a>

## 本机验证（2026-11-19）

使用 UE 5.8.2，CL 56702186。独立 UE 进程重新加载了十辆车和两份房间 DA，核对车型回满时间、主技能引用以及正式局初始点数。Car08 的引用链从已保存的 Pawn 蓝图一直核对到烟幕技能 DA。

全量 `HAL.FutureCreation` 共 257 项通过，255 项干净通过，另外 2 项来自车型测试模式对受阻出生和空对手选项的预期警告。用例覆盖失败起手、额外补能实际入账、眩晕继续自然回能、技能重入、Car07 半格补能、Car08 新旧烟团共存、37 点正式开局和结算冻结。报告在 `Saved/UltimateEnergy/Full_Verified/index.json`。

本机 Listen Server 和独立客户端连续完成两局。在主机设置 `NetEmulation.PktLag 40` 后，两名玩家每局都从倒数进入 Playing，自然回满后从主技能输入成功施放，服务器各扣 100 点，客户端收到扣能值。双端报告在 `Saved/UltimateEnergy/Room_Lag40_R2/`。这里的 40 ms 为模拟出站延迟。

- Car07 使用已保存的主技能和车型 DA。主客端均观察到连续换线从四格降到两格，随后按照第一次施放的服务器时间回到三格。原地漂移转向实测两端分别为 149.29°、149.27°。报告在 `Saved/UltimateEnergy/Shift_LAN_Final/`。
- Car08 使用已保存的主技能和车型 DA。主客端各成功扣除 100 点。两端均验证了自己的喷射、对方造成的烟幕干扰、喷射后的烟团残留和最终清理。客户端 960×540 实拍可见，烟幕覆盖车辆时，前景 HUD 仍显示 `ULT 5/100 CHARGING`。报告和实拍在 `Saved/UltimateEnergy/Smoke_HUD_Final/`。
- Exhibition 房间以本机一主三客完成两局，四个进程均正常退出且无测试警告。报告在 `Saved/UltimateEnergy/Exhibition_Final/Summary.json`。四台展出设备上的 HUD 可读性和操作手感留待团队实测。
