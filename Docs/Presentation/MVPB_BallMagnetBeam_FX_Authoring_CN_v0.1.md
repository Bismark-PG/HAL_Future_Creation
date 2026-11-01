# 球權磁力光束特效 —— Editor 製作與接線步驟（繁體 v0.1）

## 1. 這個特效解決什麼問題

球被持有時幾乎貼合車頭：球半徑 50cm、車體 Box Extent (100,70,35)，球直徑 100cm 對車寬 140cm。
水平方向沒有畫特效的空間，所以歸屬識別改走**垂直方向**——從持有者車頂拉一道弧形光束落到球上。
相機為 `FRotator(-60, 0, 0)`（見 `ATestVehiclePawn` 建構式），60° 仰角下垂直方向在螢幕上仍佔實際長度的
一半，且車身上方是空螢幕區，不會被車體或球遮擋。

兩層表現，都在 `UHALMagnetBeamPresentationComponent`（掛在 `ABasicBallActor` 上，元件名 `MagnetBeam`）：

| 層 | 作用 | 沒做資產時的表現 |
|---|---|---|
| 車頂光束（Ribbon） | 回答「哪輛車持有」 | `Beam System` 留空時不產生任何元件，看不到光束 |
| 球殼染色脈衝（材質參數） | 回答「這顆球已被持有」，與光束同色同相位 | 球材質沒有對應參數時為無操作，不會報錯 |

幾何在 C++ 算（`HALMagnetBeamGeometry.h`／`.cpp`），Niagara 只負責畫線 —— 與車身受電弧光同一套做法。

## 2. 資產製作

按 AGENTS.md 第 8.3 節，程式代理不修改 `.uasset`，以下由團隊在 Editor 內完成。

### 2.1 複製 Niagara System（已完成）

由 `NS_VehicleShockRibbon` 複製為 `Content/Assets/FX/NS_BallMagnetBeam`。

六個 User Parameter 名稱是與 C++ 的契約，**錯一個這一層就靜默失效**。團隊已改好，核對清單：

| User Parameter | 型別 |
|---|---|
| `BeamPositions` | Position 陣列 |
| `BeamWidths` | Float 陣列 |
| `BeamPointsPerRibbon` | Int |
| `BeamRibbonCount` | Int |
| `BeamAlpha` | Float |
| `BeamColor` | Linear Color |

> 已於 2026-11-02 核對存檔後的 `.uasset`：六個 `Beam*` 全部存在，`Arc*` 殘留為 0。
>
> 本文件早期版本曾要求刪除遺留的 Sprite Renderer —— 那是狀態文件裡對 `NS_VehicleShockRibbon`
> 的記錄，實際複製出來的 `NS_BallMagnetBeam` 的 `Render` 分組下只有 `Ribbon Renderer`，
> 沒有可刪的東西，該步驟作廢。

### 2.2 和受電弧光唯一的結構性差異：Local Space 必須**取消勾選**

受電弧光推的是車體區域座標，所以它勾 Local Space。
磁力光束橫跨「車頂」和「球」兩個不同 Actor，C++ 每幀用兩端的當前世界姿態重算，推的是**世界座標**。
Local Space 若保持勾選，光束會出現在錯誤的位置。

### 2.3 Spawn 數量

`PointsPerStrand × StrandCount` = 預設 21 × 3 = **63 個粒子**，Burst 數量要對上，
`Loop Behavior = Infinite`。改了 C++ 的 `BeamSettings` 裡這兩個值，Burst 要跟著改。

若 Burst 數量已經綁在 `BeamPointsPerRibbon × BeamRibbonCount` 兩個 User Parameter 上，
則不需要手動改數字。

### 2.4 Ribbon 材質（已產生）

**`Content/Assets/FX/Materials/M_FX_MagnetBeam_Ribbon`**，在 Ribbon Renderer 上指派它。

不要沿用 `M_FX_ShockArc_Ribbon`：那份帶一道很亮的白熱芯，適合電弧。
Additive 混合會把超過 1 的部分裁成白色，而光束唯一的工作就是表達「球屬於哪輛車」，
持有者顏色必須活下來，所以這份材質刻意沒有白熱芯，峰值壓在 1 附近。

兩組可調參數（在材質實例裡改，不用動程式）：

| 組 | 參數 | 預設 | 說明 |
|---|---|---|---|
| Body | `EdgePower` | 2.5 | 橫截面軟硬。調大→更細更銳 |
| Body | `EdgeBoost` | 1.0 | 整體亮度。調大會開始把顏色洗白 |
| Body | `BaseGlow` | 0.35 | 磁通帶之間的底光，避免斷續感 |
| Flow | `FlowTiling` | 3.0 | 一條光束上同時有幾道磁通帶 |
| Flow | `FlowSpeed` | 0.6 | 流動速度。**正值＝流向車頂**，讀作「吸住」；負值會讀成車在往外發射 |
| Flow | `FlowSharpness` | 2.5 | 磁通帶的銳利度 |
| Flow | `FlowBoost` | 0.9 | 磁通帶亮度 |

沿線流動的磁通帶是「磁力」和「發光繩子」的分界線 —— 電弧那份材質只做橫向塑形，沒有這一層。

重新產生（需要 Editor 開著）：

```bash
python Tools/UnrealBridge/ue_remote.py exec Tools/UnrealBridge/build_magnet_beam_ribbon_material.py
```

橋接是**只在作者本機使用的工具，按決定永不入庫**（`Tools/UnrealBridge/` 已列入 `.gitignore`），啟用它所需的 `Config/DefaultEngine.ini` Python 區塊也不提交。因此換設備時只能直接用這份 `.uasset`；要重新產生，得在那台機器上自行備齊橋接與設定。

## 3. 接線

### 3.0 找不到 `MagnetBeam` 元件？先編譯

`MagnetBeam` 是在 `ABasicBallActor` 的 C++ 建構式裡建立的，**不是在藍圖裡加的元件**。
執行中的 Editor 載的是舊的 DLL，所以在重新編譯並重啟 Editor 之前，
`BP_BasicBall` 的元件列表裡不會有它，這是正常的。

編譯需要先關掉 Editor 與 Live Coding Console（新增 UCLASS 無法走 Live Coding）：

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" HAL_Future_CreationEditor Win64 Development -Project="C:\UEProjects\HAL_Future_Creation\HAL_Future_Creation.uproject" -WaitMutex
```

### 3.1 指派光束系統

1. 編譯並重開 Editor 後，開啟 `Content/Blueprints/Balls/BP_BasicBall`。
2. 選 `MagnetBeam` 元件，`Magnet Beam → Beam System` 指派 `NS_BallMagnetBeam`。
3. 球只有這一個藍圖，**不需要**像受電特效那樣逐個車輛藍圖指派
   （那四個車輛藍圖至今仍為空、被電沒有任何表現，正是按車輛掛特效的代價）。

### 3.2 球殼染色（可選，不做也不影響光束）

球的材質加兩個參數即可接上第二層：

- `HolderColor`（Vector 參數）—— 持有者顏色
- `HolderPulse`（Scalar 參數，0~1）—— 呼吸強度，未持有時為 0

參數不存在時 C++ 的 `SetVectorParameterValue`／`SetScalarParameterValue` 是無操作，不會報錯。

## 4. 可調參數與建議值

全部在 `MagnetBeam` 元件上，`EditDefaultsOnly`。

| 參數 | 預設 | 說明 |
|---|---|---|
| `Apex Height Cm` | 170 | 弧頂高度。低於 80 貼著車頂、浪費垂直空間；超過 250 開始擋住車後方並與其他持球者的光束交疊。甜區 140~200 |
| `Strand Count` | 3 | 股數，1~6 |
| `Points Per Strand` | 21 | 曲線平滑，不需要像折線弧光那麼多點 |
| `Helix Turns` | 1.2 | 螺旋圈數。**為 0 時看起來是繩子不是磁力** |
| `Spread Cm` | 26 | 中段螺旋半徑，兩端自動收攏 |
| `Drift Turns Per Second` | 0.35 | 連續漂移。注意不是弧光那種跳變時鐘 |
| `Tension Reference Cm` | 320 | 判定為滿張力的長度。超過靜止間距後光束變粗變亮，這是「快要脫球」的提前提示 |
| `Holder Palette` | 4 色 | 按 `PlayerId` 取模索引。**刻意不含藍色**，藍色已經表示「被電」 |

## 5. 階段時間軸

由球狀態事件驅動，不輪詢：

| 階段 | 觸發 | 表現 |
|---|---|---|
| `Attach` | 進入 `Controlled` | 0.14s 從車頂掃向球，弧頂與螺旋半徑過衝後收斂 |
| `Hold` | Attach 結束 | 穩定漂移，粗細亮度隨張力變化 |
| `Release` | 離開 `Controlled` 且不是 `Launched` | 0.3s **從球端**抽回車頂 —— 抓不住了 |
| `Launch` | 進入 `Launched` | 0.12s **從車頂端**塌縮進球 —— 能量進了這一發 |

`Release` 與 `Launch` 的收回方向相反，這是兩者唯一的區分點，驗收時要分開看。

## 6. 團隊驗收步驟

1. `hal.MagnetBeamLog 1` —— 印出掛載與階段變化，用來區分「沒執行」和「執行了但沒畫出來」。
   沒看到光束時先看這條日誌裡的 `beam=` 是 0 還是 1。
2. 單機：吸球 / 發射 / 被撞脫球各一次，確認三種收尾動作不同。
3. Listen Server 雙端：
   - 主機玩家本人持球能看到（這條走的是權威路徑，和客戶端不是同一條程式碼）。
   - 客戶端看另一名玩家持球時，光束末端要**落在球的算繪位置上**。球在 `Controlled` 時由
     相對姿態顯示，所以這裡專門用了 `GetDisplayedRootPose()`；如果看到光束指向球以外的地方，
     表示這一條失效了。
4. 車輛側翻／倒扣時持球：光束弧頂方向鎖世界 +Z，不能朝地裡打。
5. 四人／多球同時持球：看中段交疊是否超出可接受範圍，必要時降 `Apex Height Cm`。

## 7. 本文件未涵蓋的內容

- 地面共享光環（方案 C）與車頭磁爪（方案 D）未實作，按討論結論後置。
- 正式玩家配色系統不存在，`Holder Palette` 是按 `PlayerId` 取模的窄適配層；
  將來接入正式配色時，`ResolveHolderColor()` 是唯一需要改的地方。
- 球碰到電流的規則、撞歪柱子是否中斷電流仍未確認，與本特效無關。
