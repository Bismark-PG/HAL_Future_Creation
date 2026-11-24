# 塗鴉／漫畫風表現層（Comic Doodle FX）v0.1

> 2026-11-18 自動化疊代輪。關鍵字：**塗鴉、漫畫風**（語彙沿用胎痕噴漆與 Impact Burst 墨線碎片：平塗填色＋黑色墨線＋右下重墨＋「逐格」跳動、以縮小而非淡出收尾）。
> 全部為**純表現**：不改物理、傷害、球規則、控球／發射許可或任何權威結果；除既有事件外**沒有新增任何網路資料**。

本輪共六項：一項必做（S0 火花改版）＋五項新效果。驗證結論見 §8，**未做過的檢查不寫成通過**。

| 代號 | 效果 | 觸發（全部讀已複製狀態） | 落點 |
|---|---|---|---|
| S0 | 車對車漫畫火花 | 既有 Impact Burst 事件的 `Sparks` | `NS_VehicleImpactSparks`＋`M_FX_ComicSpark` |
| P1 | 危險球三角形尾跡（2026-11-24 起預設，見 §3.0；脈衝環見 §3） | 球 `Launched` | `UHALBallComicFXComponent`＋`NS_BallInkTrail` |
| P3 | 持球瞄準塗鴉箭頭 | 球 `Controlled`＋`Holder` | `UHALBallComicFXComponent`＋`MI_FX_BallAimArrow` |
| H1 | 持球環（球上軌道環＋車頂雙環星芒，取代磁力束弧線；見 §4.1） | 球 `Controlled`＋`Holder` | `UHALBallComicFXComponent`＋`MI_FX_BallHoldRing_Ball／_Roof` |
| P4 | 眩暈漫畫星星 | 車輛 ASC 的 `State.Stun` 標籤 | `UHALVehicleDoodleStatusComponent`＋`MI_FX_DoodleStatus_Stun` |
| B2 | 低 HP 汗滴 | `UVehicleHealthComponent::OnHealthChanged` | 同上＋`MI_FX_DoodleStatus_LowHealth` |
| N1 | 畫面外來球預警 | 本地：`Launched` 球的位置／速度＋本地攝影機投影 | `UHALIncomingBallWidget`（本地 Slate 疊層） |

---

> **2026-11-18 用戶驗收後的三項調整（以此為準，下文相關段落為歷史紀錄）**
> 1. **S0 火花改版完全取消**：`NS_VehicleImpactSparks` 與 `HALImpactBurstSettings.h`（數量 2～6）已還原到 `a0fe105` 的版本，車對車火花回到原本的橘色拉長條；`MVPB_Vehicle_Impact_Burst_CN_v0.1.md` 的改版指引也已撤回。`M_FX_ComicSpark`、兩個實例、`Modules/NM_ComicSparkRadialBurst` 與 `Tools/ComicSparks` 已移出專案，備份在 `Saved/ComicDoodle/RemovedSparks_20261118/`（不入版控）。§2 全節僅供日後參考。
> 2. **P3 瞄準箭頭縮為一半**：`AimLengthWidthFraction` 1.15 → 0.575、`AimWidthWidthFraction` 1.0 → 0.5；箭尾與球的間距（0.3 車寬）未動。
> 3. **B2 低 HP 汗滴預設不顯示**：新增 `FHALVehicleDoodleStatusConfig::bLowHealth`（預設 `false`）；程式、材質與 ini 設定保留，Project Settings → Vehicle Doodle Status FX → Low Health 勾選即可恢復。

## 1. 審議紀錄（三位 subagent、兩輪）

提案階段先列出 S0＋P1～P5 與備選 B1～B3，交給三位審議員各自評分（第一輪），再把三份意見互相交叉後表決（第二輪）。

| 審議員 | 角色 |
|---|---|
| fx-reviewer | 可讀性與造型（俯視 -60°、四人同屏） |
| ue-arch-reviewer | 架構與 AGENTS.md 邊界（實際讀程式碼佐證） |
| general-purpose | 玩家體驗／game feel 反方 |

**第一輪共識**：P1、P3、P4 三方一致入選。P5（命中潑漆印）出局——事件裡沒有扣血旗標，「不新增網路資料」不成立；且地面已有身份色胎痕與屬於危險物的 SlipperyTrail 塗鴉，再潑一灘同色會與危險區邊界搶讀取。B1（落地墨圈，地面環與危險區語彙相同）、B3（速度線，無新資訊）出局。反方另提出清單整體缺漏：**八項全是對四人對稱的世界空間效果，沒有一項回答「這件事跟我有關嗎」**，因此新增 N1（畫面外來球預警）。

**第二輪表決（第 4、5 名）**

| 項目 | 可讀性 | 架構 | 反方 | 結果 |
|---|---|---|---|---|
| N1 來球預警 | 第 4 | 第 5 | 第 4 | **入選** |
| B2 低 HP 標示 | 第 5 | 第 4 | 替補 | **入選** |
| P2 擬聲字印章 | 否決 | 否決 | 第 5 | 出局（2:1） |

P2 出局理由：`ColorMode==Source` 只代表「有身份色的發起者、非車身直接接觸」，技能攻擊也算在內，要精確判定「球命中」得多帶一個位元組（新增網路資料）；且接觸點已有碎片／衝擊環，再疊字不增加資訊。

**其他定案**

- **S0 造型**：用戶上一輪退回過「畫在爆點上的星形圖案」，要的是火花粒子。因此主體是**沿飛行方向的墨框閃電條**，四角星只當 1 顆點綴。固定漫畫黃、不取身份色（身份色已由爆點碎片承擔，火花只表達「這是車對車撞擊」）。兩車各發一個事件，數量會乘二，所以每事件降為 3～5 條。貼地放射（仰角 15～30°）：俯視下向上噴會讓條狀縮成一點。關閉深度測試（與 Burst 一致；接觸點本來就在兩車剪影裡）。
- **身份色來源**：P1／P3／N1 一律走 `UHALMagnetBeamPresentationComponent::ResolveIdentityColor`——房間座位色 → PlayerId 調色盤 → （無 PlayerState 的 Actor）車型胎痕色 → 後備色。「誰發的球」是玩家問題，兩人同車時車型色會撞色。
- **磁力束的可見行為變更**：`ResolveHolderColor` 現在轉呼叫 `ResolveIdentityColor`。有 PlayerState 的車結果不變；**沒有 PlayerState 的 Actor（測試木樁）原本回傳後備橘色，現在先取自己的胎痕色**。既有測試沒有依賴這個回傳值。以 PlayerId 回查只看 `GameState->PlayerArray`，已離場玩家通常不在其中，此時落到後備色。
- **P4 與電擊同時存在**：兩位審議員意見相反（1:1）。採「照樣顯示」：星星表示「這台車現在不能操作」，讀法應與來源無關；電弧畫在車身、星星在車頂上方，空間不重疊。此項列入待團隊確認。
- **P4 落點**：架構審議原建議走 Vehicle Status FX 管線（新增 `State.Visual.Stunned`＋目錄 DA＋配方 DA＋符合七個 User 參數契約的 Niagara）。實作時改為直接訂閱既有 `State.Stun` 標籤的獨立小元件：不新增 GE 授予標籤（零新增複製資料）、不需要 Editor Python 構造不出來的 `FGameplayTag` 目錄項、也不必為三顆星星建一套 Niagara。代價是不經過 Status FX 的統一優先序；若日後需要與其他狀態互相抑制，再遷入該管線。

---

## 2. S0 車對車漫畫火花（2026-11-18 驗收後已取消並還原）

- **材質** `M_FX_ComicSpark`（`Tools/ComicSparks/ComicSpark.hlsl`＋`CreateComicSparkAssets.py`）：Unlit／Translucent（**不是加法**）／關閉深度測試／Niagara Sprite 用。單一 Custom 節點、無貼圖。讀 `ParticleColor`（填色）、`ParticleRelativeTime`（壽命）、`ParticleRandom`（閃爍相位與閃電折線）。
  - `FX_Shape=0` 四角星（`MI_FX_ComicSpark_Star`）：內凹超橢圓＋實心圓核；每壽命 `FX_Steps`(4) 格，隔格轉 45° 並縮到 `FX_TwinkleScale`(0.78)。
  - `FX_Shape=1` 閃電條（`MI_FX_ComicSpark_Bolt`）：沿精靈 V 軸的三角波中心線＋中段肥、兩端尖的刃形。
  - 共同：黑色墨線 `FX_InkWidth`(0.11，精靈半寬比例)、右下重墨 `FX_ShadowOffset`(0.07)、白心 `FX_HeartMix`(0.85)；`FX_ShrinkStart`(0.45) 之後縮小到 0，透明度不變。
- **Niagara** `NS_VehicleImpactSparks`：見 §2.1（由資產作者經編輯器連線修改，實際數值以該節為準）。
- **C++**：只改預設數量 `MinimumSparks` 2→3、`MaximumSparks` 6→5（`HALImpactBurstSettings.h`）。生成路徑、事件、網路資料皆未動。

### 2.1 `NS_VehicleImpactSparks` 實際設定

經 Monolith 編輯器連線修改（**沒有腳本，下次修改需照下表重做**；退回：關 Editor 後 `git checkout Content/Assets/FX/NS_VehicleImpactSparks.uasset`）。User 參數仍只有 `SparkCount`(int，預設 4)。

| 模組 | `Sparks`（閃電條，主體） | `Twinkles`（四角星，**已停用**，見下） |
|---|---|---|
| EmitterState | Loop Once、Duration 1 | 同左 |
| SpawnBurst_Instantaneous | 綁 `User.SparkCount` | 固定 2 |
| InitializeParticle 壽命 | 隨機 0.35～0.45 s | 同左 |
| 顏色 | (1, 0.75, 0.07, 1) 線性，≈ #FFE14A | 同左 |
| 尺寸 | 非均勻：寬 70～90 × 長 130～170 cm | 均勻 70～95 cm |
| 初始旋轉 | 不設（沿速度對齊） | 隨機 0～360° |
| Write Material Random | true | true |
| `NM_ComicSparkRadialBurst`（自訂模組，在 AddVelocity 之前）＋AddVelocity（Linear，讀該模組輸出） | 把圓周平均切給每一條（第 i 條落在第 i 個切片，切片內 ±45% 抖動），整體起始角＝撞擊位置雜湊＋時間；仰角 15～30°、初速 2400～3200 cm/s | 原錐形設定（已停用） |
| ScaleSpriteSize | 曲線 0 →1.2（壽命 12%）→1.0（22%）→1.0 | 同左 |
| GravityForce／ScaleColor | 停用 | 停用 |
| Drag | 8.0 | 8.0 |
| Renderer | `MI_FX_ComicSpark_Bolt`、Velocity Aligned | `MI_FX_ComicSpark_Star`、Unaligned |

實測（SimCache，v2）：0.30 s 時閃電條水平飛行 249～351 cm（平均 296，約一個車寬）；星形約 103～108 cm。

> **第一版太小**：最初用絕對尺寸 26～34×50～66 cm、飛行 130 cm，PIE 實機一看每條只有車寬的 1/6、墨框看不出來——車體實際約 270 cm 寬、450～640 cm 長。v2 整體放大約 2.6 倍，並把材質白心 `FX_HeartMix` 0.85→0.5、`FX_FillGain` 1.5→1.0、`FX_InkWidth` 0.11→0.14，避免在曝光下洗成粉白。

> **方位角為什麼不用錐形隨機**：5 條獨立隨機取方向時，約三分之一的機率全部落在同一個半圓（SimCache 20 次中 7 次），實機連續三次都只往一側噴，讀起來像壞掉。改成切片後 SimCache 5 次爆發的最大空角為 100～129°，80 顆的四象限為 25/25/24/26。新模組資產 `/Game/Assets/FX/Modules/NM_ComicSparkRadialBurst` 必須與系統一起提交。

> **`Twinkles` 發射器已停用**（保留在資產內，Emitter Enabled = false）：實機截圖送特效評審後判定為必改——撞擊點的黃色四角星與 P4 眩暈星星同形同色，而撞擊後常緊接著眩暈，會被誤讀成「這台車被暈了」。這也與用戶上一輪退回星形的方向一致。要恢復就把發射器重新啟用。
>
> **未採納的評審建議**：數量上限 5→4。既有測試要求弱／中／強三檔數量可分辨，3～4 做不到；數量可在 Project Settings 直接調。

> **坑**：`duplicate_emitter` 複製出的發射器一顆粒子都不生成（`request_compile` 不重建系統腳本）；臨時加一個發射器再刪掉、強迫整個重編後才正常。

---

## 3.0 P1 危險球三角形尾跡（2026-11-24，現行預設；脈衝環保留為可選樣式）

> **改版原因**：團隊要求把連續圓環改成「簡單的三角形拖影」。下面 §3 的脈衝環資產、Niagara 系統與程式路徑都保留，只是 `DefaultGame.ini` 不再指定 `TrailSystem`；兩種樣式是兩個獨立欄位，可各自開關或並存。

- 球 `Launched` 時，在球後方放一張**水平**平面（引擎內建 Plane，高度＝球外觀中心），材質 `MI_FX_BallTrailWedge`：發射者身份色的一片三角形、黑墨線、8fps 三姿態線條抖動＋兩姿態尖端伸縮。底邊貼球、尖端指向球來的方向。
- 方向與長度取球的**地面速度**（`ABasicBallActor::GetVelocity()` 去掉 Z）：長度＝速度 × `TrailWedgeLengthSeconds`(0.3)，夾在 球徑 × `TrailWedgeMinLengthDiameters`(0.8) 與 × `TrailWedgeMaxLengthDiameters`(2.0) 之間；團隊看過 0.4／2.5 後要求縮短，材質 `FX_Opacity` 同時由 1.0 降為 0.7；地面速度低於 `TrailWedgeMinSpeed`(150 cm/s) 時不畫。寬度沿用 `TrailWidthDiameterFraction`(0.75)。
- 平面起點在球心後方 球半徑 × `TrailWedgeStartRadiusFraction`(0.35)，底邊因此埋在球內、由球本身遮掉。歷程：0.6 配長度 0.12 s 時三角形大半被球蓋住只露尖角；改 0.9 並拉長後，往畫面上方射時底邊落在球與鏡頭之間而露出一條平邊（團隊回報）；現值在拉長後的長度下兩者都不發生。
- 平面非等比拉伸，材質以 `FX_Aspect`（長／寬，C++ 每幀寫入）換算，墨線粗細不隨長度變。開深度測試，球與車會自然遮擋它。
- 離開 `Launched` 當幀隱藏，沒有殘留。純函式 `HALBallComicFX::TrailWedgeQuad`，有自動化斷言。
- **技能改拖影的接點**：開始／停止／顏色都在 `UHALBallComicFXComponent::StartTrail` 一處決定，只讀同步的球狀態。尚未做樣式選擇接口；待第一個需要的技能出現時，在發射時擷取的 `AttackSnapshot` 加樣式標記，由這裡選材質。
- **已驗證**：編譯；無頭全量 280/280；單機 PIE 發射後看到三角形（`Saved/ComicDoodle/HoldRings/PIE12_trail_before_after.png`）。**未驗證**：PIE 截圖時只有約 3.5 fps，發射後第一格沒有三角形，推測是速度晚一幀才讀得到，正常幀率下未確認；反彈轉向、斜向飛行、雙端／多人、Cook 都未看。

## 3. P1 危險球脈衝環尾跡（2026-11-18 由漫畫火焰改版）

> **改版原因**：用戶看過示意後指定「脈衝環（能量場）」方向，取代漫畫火焰。資產路徑 `NS_BallInkTrail` 與 C++ 契約（`User.TrailColor` LinearColor、`User.TrailWidth` float）不變，**沒有改 C++ 與 Config**。
>
> **歷史**：P1 先後做過 Ribbon 墨帶（`M_FX_BallInkTrail`）與漫畫火焰（`FlameBody`＋`Tongues` 兩個發射器、`M_FX_BallFlame`＋`MI_FX_BallFlame_Ink／_Fill／_Heart` 三層疊合、自訂模組 `NM_BallFlameSpawn／NM_BallFlameUpdate`）。火焰版的資產、HLSL 紀錄（`Tools/ComicDoodle/BallFlame.hlsl`、`NiagaraBallFlame*.hlsl`）與本節原有的數值表、PIE 發現都在提交 `3f8041d` 中，需要時以 `git show 3f8041d:<路徑>` 或 `git checkout 3f8041d -- <路徑>` 取回。火焰版學到、仍適用於本版的教訓：絕對尺寸一律用 W 的倍率；系統掛在球心而球外觀半徑約 0.67 W；球 `Launched` 時球速會從約 2200 掉到約 450 cm/s，慢球時效果不能消失。

- 球進入 `Launched` 時啟動 `NS_BallInkTrail`：球沿路徑留下一圈圈**平貼地面的環**，環留在原地（世界空間、不跟球走）、擴大、線條變細後消失，像球穿過一層層力場。離開 `Launched` 的同一次狀態處理 `DeactivateImmediate`，所有環同時消失（C++ 未改，行為同前）。
- 寬度基準 W＝`User.TrailWidth`＝球外觀直徑 × 0.75（測試圖約 240 cm）；下文 D＝W／0.75＝球外觀直徑（約 320 cm）。系統內所有長度都是 W 的倍率。
- 語彙：發射者身份色的色帶＋內外兩側黑墨框，Unlit／Translucent／不發光／深度測試開；12 fps 逐格（尺寸、線寬、抖動形狀只在換格時變）；**以線條變細收尾，不淡出**。
- 顏色完全取 Particle Color（＝`User.TrailColor`），不改色相；只有年輕環中央的細亮線往白提亮。

### 3.1 材質 `M_FX_BallPulseRing`

`Tools/ComicDoodle/BallPulseRing.hlsl`＋`CreateComicDoodleAssets.py` 的 PULSE 段（可單獨重建：`CreateComicDoodleAssets.py PULSE`；在開著的編輯器裡跑時以 `sys.argv = ['CreateComicDoodleAssets.py', 'PULSE']` 後 `exec`）。單一 Custom 節點、無貼圖，一次畫完墨框＋色帶。實例 `MI_FX_BallPulseRing`（0 個覆寫）是 `PulseRings` 唯一的 Sprite Renderer 材質。

**幾何全部由 Niagara 以公分傳入**（材質不知道 W）：Particle Size＝精靈大小（含線寬與抖動的留白）；Dynamic Parameter 0＝(x 量化後正規化年齡、y 每格抖動種子、z 色帶半寬 cm、w 單側墨框寬 cm)；Dynamic Parameter 1＝(x 垂直行進方向的半軸 cm、y 沿行進方向的半軸 cm、z 抖動幅度 cm、w 起始脈衝旗標)。材質以「到橢圓的一階距離」畫線，所以整圈線寬一致、與精靈長寬比無關。腳本的 `build()` 因此新增 `Dynamic1` 來源（Dynamic Parameter 節點 `parameter_index=1`）。沒有 runtime driven 參數（C++ 不寫材質）。

| 參數 | 預設 | 作用 |
|---|---|---|
| `FX_BoilWaves` | 3 | 輪廓抖動的波數（取整，第二道諧波為 +2） |
| `FX_Pressure` | 0.2 | 色帶沿圈的粗細起伏（手繪壓力感），色帶半寬的比例 |
| `FX_CoreWidth` | 0.3 | 中央亮線半寬，色帶半寬的比例 |
| `FX_CoreLife` | 0.45 | 亮線消失的正規化年齡（逐格縮細） |
| `FX_StartCoreLife` | 1.5 | 起始脈衝的 `CoreLife` 倍率 |
| `FX_CoreLift` / `FX_CoreGain` | 0.3 / 1.3（R9 前為 0.55 / 1.6） | 亮線往白提亮與推亮；R9 實機上淡色身份色被洗到近白，故下修 |
| `FX_ColorLift` / `FX_FillGain` / `FX_Opacity` | 0 / 1 / 1 | 色帶顏色（預設＝原色） |
| `FX_InkColor` | 黑 | 墨框顏色 |

編譯：0 訊息、`validate_material` 0 issue、像素 190／頂點 344 條指令、19 個節點、實例暴露 11／11 個參數。

### 3.2 `NS_BallInkTrail` 實際設定（2026-11-19 版，以此為準）

經 Monolith 編輯器連線製作（**沒有腳本**）。2026-11-19 發射器 `PulseRings` 由引擎範本 `/Niagara/DefaultAssets/Templates/Emitters/Minimal` **整個重建**（原發射器內殘留火焰版的覆寫節點，見 §3.4 坑 10）。自訂模組 `/Game/Assets/FX/Modules/NM_BallPulseRate`、`NM_BallPulseSpawn`、`NM_BallPulseUpdate` 的 HLSL 只存在於模組資產內，`Tools/ComicDoodle/NiagaraBallPulseRate.hlsl`／`NiagaraBallPulseSpawn.hlsl`／`NiagaraBallPulseUpdate.hlsl` 是原文紀錄（含輸入清單），改模組要照 §3.4 坑 11 的順序。**系統與三個模組必須一起提交。** User 參數 `TrailColor`、`TrailWidth`（預設 240，只影響編輯器預覽）名稱必須與 `UHALBallComicFXSettings` 一致。模組節點名稱皆無數字尾碼。

| 階段 | 模組 | 設定 |
|---|---|---|
| 模擬 | — | CPU、Local Space 關；EmitterState Life Cycle Self、Loop Infinite、Duration 5 |
| Emitter Update | `NM_BallPulseRate`（在 SpawnRate 之前） | 寫 `Emitter.PulseRate`＝clamp(球水平速度 ÷ (Spacing×W), MinRate, MaxRate)；Spacing **0.9**（未乘 0.75）、MinRate **4.5/s**、MaxRate **9/s** |
| | SpawnBurst_Instantaneous | 0.03 s 時 1 顆（UseLoopCountLimit、Loop Count Limit 1）＝起始脈衝 |
| | SpawnRate | `SpawnRate` 綁 `Emitter.PulseRate` |
| Particle Spawn | `NM_BallPulseSpawn`（無 InitializeParticle） | 壽命 **0.55～0.6 s**；Squash **0.28**（任何速度）；HeadingMinSpeed **0.02 W/s**（約 5 cm/s，低於此不生環）；環心後移：BackOffset **0.3 W**（快）↔ BackOffsetSlow **0.95 W**（慢），以 SlowSpeed **0.8 W/s**～FastSpeed **3.0 W/s** 的手寫 smoothstep 混合；DropHeight 0；StartWindow **0.06 s**、StartScale **1.3** |
| Particle Update | ParticleState → `NM_BallPulseUpdate` | Fps **12**；垂直方向直徑 SizeStart **0.9 W** → SizeEnd **1.8 W**（GrowPower 2）；Band **0.11 W**、單側 Ink **0.08 W**（ThinPower 1）；MinInk **0.01125 W**；Boil **0.015 W**；HoleMin **0.075 W** |
| Renderer | Sprite | `MI_FX_BallPulseRing`；FacingMode CustomFacingVector；Alignment CustomAlignment；Pivot (0.5, 0.5)；SortMode CustomDecending |

**為什麼這樣生成**：「依距離＋每秒上限＋最低頻率」三條規則合成一個速率交給內建 SpawnRate（它自帶小數餘數，速率每幀變也能均勻間隔）。球速在約 972～1944 cm/s 之間時，速率＝球速 ÷ 間距（每 0.9 W 一個）；更快由上限 9/s 控制，更慢由下限 4.5/s 控制。

**方向與位置（2026-11-19）**：任何速度都是同一個橢圓（長短比 0.28），對齊**當下**球速方向，環心沿行進反方向後移；後移量隨球速由 0.3 W（快）平滑加大到 0.95 W（慢），讓慢球的環落在球後緣之外（球繪製半徑約 0.67 W）。球速低於 HeadingMinSpeed（約 5 cm/s，幾乎靜止）時方向不可信，**該環直接丟棄**（壽命 0，update 也不畫），而不是放到任意一側。

**起始脈衝**：系統啟動後約 0.05 s 單發一個 1.3 倍大的環，旗標寫入 Dynamic Parameter 1.w，材質讓它的亮線多留 50%。

**逐格與收尾**：年齡以 12 fps 量化，取「這一格開始時」的正規化年齡 t：直徑＝lerp(SizeStart, SizeEnd, 1−(1−t)²)，色帶與墨框 ×(1−t)。墨框細到 MinInk 以下的那一格整圈不畫，所以收尾是「線變細 → 消失」，從不調透明度。

**HoleMin**：年輕環沿行進方向的半軸至少為「線條半寬＋HoleMin×W」，避免出生那幾格中孔被粗墨框填滿成黑色菱形（只在線條還粗時把長短比放寬到約 0.47）。

### 3.2.0 用戶實測回饋與調整（2026-11-19）

用戶看過 R9 版實機（截圖：球快停下、貼牆時，大的同心圓環包住球，看起來像生成在球正下方）後要求：(A)「球快停下時，環會像生成在球的下方，讓它保持在球行徑的反方向」；(B)「特效太大，縮小為原本的 75%」。

調整：
1. **取消 R9 的低速同心圓形態**（SlowSpeed／FastSpeed 不再混合形狀，刪除 SizeStartSlow／SizeEndSlow）；任何速度都是對齊行進方向的橢圓。
2. **慢球環心後移**：BackOffset 0.3 W（快）→ BackOffsetSlow 0.95 W（慢），混合區間沿用 0.8～3 W/s。協調者建議約 0.75 W；離屏預覽中 0.75 W 時 100～300 cm/s 最年輕的環仍有一段被球邊緣蓋住，0.95 W 時完整露在球後，故採 0.95。
3. **尺寸 ×0.75**：SizeStart 1.2→0.9 W、SizeEnd 2.4→1.8 W、Band 0.147→0.11 W、Ink 0.107→0.08 W、MinInk 0.015→0.01125 W、Boil 0.02→0.015 W、HoleMin 0.1→0.075 W；StartScale 1.3、BackOffset、Spacing、Min／MaxRate 不變。縮小後 2000 cm/s 的環距（222 cm）看圖不疏，Spacing 未改。
4. **「最後一次可信方向」沒有做到**：原計畫在 `NM_BallPulseRate` 內把方向存進發射器屬性 `Emitter.PulseHeading` 給 spawn 讀。實作時自訂 HLSL 寫新發射器屬性報「hasn't been encountered yet」，改用綁定輸入宣告後，emitter spawn 與 particle spawn 又報「read before being set（Fail If Previously Not Set）」；以 Monolith／Python 改不到該變數的 Default Mode，放棄。替代做法：方向一律取當下球速，門檻降到約 5 cm/s（減速中的球仍有可靠方向）；完全靜止時不生新環。實際影響：`Launched` 的球低於 250 cm/s 持續 0.5 s 即回 Free，完全靜止的窗口很短；那段時間只剩之前的環在球後燒完，不會再出現「環在球下」。

離屏量測（重啟後，`Saved/ComicDoodle/Pulse/measure_r7.json`；截圖 `r7_*.png`，藍與淡紫，含 320 cm 參考球，俯角 -60°）：

| 情況 | 存活／畫出 | 環心距球心（出生時） | 間距 | 畫出的直徑（D） |
|---|---|---|---|---|
| 600 cm/s 減速到 0 後 0.5 s | 1／0（靜止後只剩被丟棄的粒子） | — | — | — |
| 100 cm/s | 3／2 | 約 0.95 W | 22 cm | 0.68、1.15 |
| 300 cm/s | 3／2 | 約 0.9 W | 67 cm | 0.68、1.13 |
| 700 cm/s | 3／2 | 約 0.3 W | 156 cm | 0.68、1.13 |
| 2000 cm/s | 5／4 | 0.3 W | 222 cm | 0.68～1.22 |
| 300 cm/s 朝鏡頭／背鏡頭 | 3／2 | 約 0.9 W | 67 cm | 0.68、1.14 |

看圖：100～300 cm/s 時環完整出現在球後方（橫向移動）；朝鏡頭移動時環在畫面上方（世界中的球後方），最年輕一圈貼著球頂；700 cm/s 時最年輕一圈約一半被球蓋住（高速形態的既有取捨）。

> 以下 §3.2.1 的低速同心圓形態為**歷史**，已於 2026-11-19 取消。

### 3.2.1 R9 實機發現與調整（2026-11-18，歷史：低速同心圓形態已於 2026-11-19 取消）

PIE 截圖 `Saved/ComicDoodle/PIE/R9`（例：`L2_19_slow_v300.png`、`L4_16_t250.png`、`L4_05_t060.png`）：高速段很好；但球的繪製直徑約 1.33 W，第一版的環出生直徑 1.2 W 比球小、環心又在球心高度，所以**球速低於約 700 cm/s 時只看得到一條細弧加一個貼著球的月牙**。實測 `Launched` 有 40～80% 的時間處在這個速度：發射後前 1 秒約 2560 → 1400 cm/s，撞牆後只剩 330～540 cm/s，低於 250 cm/s 持續 0.5 s 就回到 Free。另外中央亮線讓淡色身份色的環看起來近白。

調整：(1) 加入上述低速形態並以 smoothstep 連續混合（`NM_BallPulseSpawn` 的 DirSpeed 換成 SlowSpeed／FastSpeed，`NM_BallPulseUpdate` 新增 SizeStartSlow／SizeEndSlow；兩個模組都重建，並照 §3.4 的坑以「末端加新節點 → 刪舊節點 → 重綁 User 參數」換上）；(2) MinRate 3 → 4.5/s，讓低速時同時可見 2～3 個同心環；(3) `FX_CoreLift` 0.55 → 0.3、`FX_CoreGain` 1.6 → 1.3；(4) 起始脈衝 1.3 倍不變。壽命與線寬沒有改：預覽中 3 個同心環各自分得開。

離屏量測（R9 調整後，`Saved/ComicDoodle/Pulse/measure_r4.json`；截圖 `r4_*.png`，藍 (0.1, 0.35, 1.0) 與淡紫 (0.58, 0.45, 0.95)）：

| 球速 cm/s | 混合比例 f | 存活／正在畫 | 長短比 | 畫出的直徑（D） |
|---|---|---|---|---|
| 0 | 0 | 3／2～3 | 1.0 | 1.12、1.67、1.93（同心） |
| 150 | 0 | 3／2 | 1.0 | 1.12、1.69 |
| 300 | 0.11 | 3／2～3 | 0.92 | 1.10、1.67、1.91（間距 67 cm） |
| 500 | 0.62 | 3／2 | 0.55 | 0.98、1.59（間距 111 cm） |
| 700 | 1.0 | 3／3 | 0.28 | 0.90、1.50、1.78（間距 156 cm） |
| 1000 | 1.0 | 3／3 | 0.28 | 1.14、1.50、1.78（間距 216 cm） |
| 2000 | 1.0 | 5／4～5 | 0.28 | 0.90～1.78（間距 222 cm） |

（年輕環在高速時長短比為 0.34～0.47，是 HoleMin 保留中孔所致。）看圖：0～300 cm/s 時最外圈與中圈都完整露在球外，最年輕的一圈遠側仍被球擋住（其直徑 1.12 D 只比球大 12%，在 -60° 俯角下遠側約落在球的輪廓內）；淡紫的主色保留、亮線只比主色淺一階，不再近白；2000 cm/s 的藍／紫仍看得到亮線層次。

### 3.3 離屏量測（第一版，W=240，`fx.DumpParticleData`；`Saved/ComicDoodle/Pulse/measure_r3.json`、`measure_loop.json`，不入版控；R9 調整前的數值，低速列已被 §3.2.1 取代）

做法：在開著的編輯器開一張暫存空白地圖，放一個 `NS_BallInkTrail` 與一顆直徑 320 cm 的參考球，以腳本逐幀移動兩者並 `advance_simulation(1, 1/60)` 同步推進，最後一幀開 `fx.DumpParticleData` 讀回，再以 SceneCapture2D（俯仰 -60°、FOV 60）截圖。量測與截圖腳本存在 `Saved/ComicDoodle/Pulse/pulse_probe.py`、`parse_dump.py`。

| 情況 | 同時存活／正在畫的環 | 環間距 | 畫出的垂直方向直徑 |
|---|---|---|---|
| 靜止（0 cm/s） | 1～2／1 | 同心（每 1/3 s 一個） | 1.33～1.51 D（在 t=1.0／1.2 s 的快照） |
| 250 cm/s | 2／1 | 約 83 cm（下限 3/s 控制） | 1.34 D |
| 600 cm/s | 1／1 | 約 200 cm（下限 3/s） | 1.5 D |
| 1000 cm/s | 3／2～3 | **216 cm（＝0.9 W）** | 1.13、1.53、1.78 D |
| 2000 cm/s | 5／4 | **222 cm**（上限 9/s） | 0.9、1.36、1.52、1.65 D |
| 3000 cm/s | 5／4 | 333 cm（上限 9/s） | 0.9～1.65 D |
| 反彈 1000／2000 cm/s（約 150° 轉向） | 3／2～3、6／5 | 轉角處 142～275 cm | 新環沿新方向對齊、舊環原地燒完 |

其他：出生那一格色帶 0.11 D、單側墨框 0.08 D（墨框總寬 0.16 D ＞ 色帶）；最後一個畫出的格色帶約 0.018 D、墨框約 0.013 D；環中心在 1000 cm/s 時位於出生當下球心後方 72 cm（0.3 W）；起始脈衝出生 1.17 D（0.9 D × 1.3）、旗標 1，5.3 s 時新環旗標為 0。只測了等速直線與瞬間反彈，沒有測漸變速度與連續曲線。

### 3.4 離屏預覽（第一版 `Saved/ComicDoodle/Pulse/r3_*.png`；R9 調整後見 §3.2.1 的 `r4_*.png`；不入版控）

每張都有直徑 320 cm 的白色參考球，藍 (0.1, 0.35, 1.0) 與黃 (1.0, 0.75, 0.05) 兩種身份色，`far`＝2500 cm、`near`＝1300～1600 cm。看圖確認：環完整、內外墨框都在；越老的環越大越細；2000 cm/s 時 4 個環等距排開；靜止時是同心圓；反彈後新環轉向、舊環留在原處；起始脈衝較大較粗。

**看圖發現、未解的觀感問題（待團隊判斷）**
1. ~~年輕的環被球擋住~~（R9 已以低速形態處理，見 §3.2.1；高速時最年輕一圈的前一兩格仍貼在球後）。
2. **在測試光照下黃色偏暗成芥末色**：離屏截圖用固定曝光，實機曝光不同；本版沒有改色（規格要求不改色相），是否要 `FX_FillGain` > 1 待 PIE 看。
3. **反彈處新舊環交疊**：年輕環的粗墨框會壓在舊環的細線上，讀得出是兩個環，但轉角處比直線段亂一點；沒有改成墨／色兩層 renderer。
4. 最後一格的細線（約 4～6 cm 寬）在 2500 cm 鏡頭下只剩 1～2 px，之後整圈消失；看起來像「斷掉」而不是「縮成一點」，是刻意的收尾方式。
5. 球的投影（測試光照）在截圖中是一塊黑色，與墨框同色，觀感上偶爾會跟環的墨框連在一起；這是預覽場景的燈光，不是特效本身。

> **坑（本輪新遇到）**
> 1. **`remove_module` 刪掉階段裡最後一個模組會斷鏈**（`stack_valid: false`，之後該階段不能再 `add_module`）。可靠順序：新模組加在**末端**、設好全部值，再刪中間的舊模組。斷了就以 `EditorLoadingAndSavingUtils.reload_packages` 從磁碟重載該系統（尚未存檔的改動全丟）再重來。
> 2. **刪模組後，新模組上綁 User 參數的輸入可能失去來源**：編譯報「Parameter Maps must be created via an Input Node … Map Set Pin: NM_BallPulseUpdate001.Width」。對該輸入重新 `set_module_input_binding` 一次即恢復。
> 3. `create_module_from_hlsl` 對既有路徑原地覆寫；只改 HLSL 內容（不改輸入清單）時可改用 `set_custom_hlsl_text`，`request_compile` 後 op_count 變化即代表新內容已生效（本輪實測：141 → 137）。改輸入清單則必須重建模組，再照第 1 點「加新節點 → 刪舊節點」。
> 4. 自訂模組可以放在 Emitter Update（Monolith 不檢查 ModuleUsageBitmask），HLSL 內直接寫 `Emitter.PulseRate = …` 就會成為發射器屬性，可供 SpawnRate 綁定。
> 5. 新增的 Particle Update 會在插值生成（Particle Spawn Script Interpolated）時同幀執行，所以出生那一幀就有正確大小，spawn 模組不必重寫尺寸公式。
> 6. **CPU Vector VM 沒有 `smoothstep`**：spawn 腳本報「VM does not have a valid operation for float smoothstep」，改手寫 x²(3−2x)。
> 7. **同一個編輯器裡的編譯結果不能當證據，必須存檔、重啟編輯器後再驗。** 2026-11-18 的 R9 調整在同一個編輯器內 `get_system_diagnostics` 為 0 錯誤、離屏量測也有粒子；隔天重開編輯器後 `PulseRings` 的 Particle Spawn（Interpolated）腳本編譯失敗，只有一句「Error compiling Pin - Node: Output Particle Spawn Pin: Out -」（沒有內文），op_count 0，PIE 完全沒有尾跡。查證：模組資產 `NM_BallPulseSpawn` 本身沒問題（圖完整、HLSL 正確；原樣重寫 HLSL 後重編仍失敗），**在同一階段新加一個引用同一模組的節點、設好全部值、刪掉舊節點、重綁 User 參數**後立即 0 錯誤，且存檔→重啟後仍 0 錯誤。所以壞的是系統裡那個函式呼叫節點（含它的覆寫節點），不是模組資產。該節點的特徵：它是在舊節點仍在時加入的（名稱被自動加尾碼成 `NM_BallPulseSpawn001`，覆寫常數也以此名存檔），之後模組又被 `set_custom_hlsl_text` 改過；同樣流程但沒有後續改 HLSL、名稱沒有尾碼的 update 節點重啟後正常。哪一個因素是真正觸發點**沒有證實**（壞掉的系統與模組備份在 `Saved/ComicDoodle/Pulse/Broken_20261119/`，不入版控）。規則：換模組節點後一律「存檔 → 關閉編輯器 → 重開 → diagnostics 與 op_count > 0 → 強制重編仍 0 錯誤 → 實際生成粒子」才算完成。
> 8. 重開編輯器後第一次截圖可能看不到粒子（材質著色器還在載入），隔一幀再截才正常；不能拿第一張空白圖判斷「沒有生成」，要以 `fx.DumpParticleData` 為準。
> 9. 量測仍**不要用** `NiagaraSimCacheFunctionLibrary`；`set_paused(True)` 之後 `advance_simulation` 實測沒有推進（dump 無資料），要先推進再暫停（暫停是為了讓截圖前的編輯器幀不再以「靜止」推進它）。
> 10. **`remove_module` 會留下覆寫節點，之後可能被接回堆疊鏈。** 2026-11-19 在原發射器裡換 `NM_BallPulseRate` 節點後，System Update 腳本出現「Map Set Pin: Multiply_Float.A … Parameter Maps must be created via an Input Node」——火焰版 SpawnPerUnit（間距 W×0.32 用 Multiply_Float）早已刪除，它的覆寫 Map Set 卻還在圖裡，換節點時被接回鏈中。Monolith 沒有刪孤兒節點的工具，因此改為從引擎 `Minimal` 範本**整個重建發射器**（刪舊發射器 → `add_emitter` → 依 §3.2 重設）。另：同一批次刪模組時一度回報 `stack_valid: false`，以編輯器 `TRANSACTION UNDO` 復原。
> 11. **模組資產的輸入清單一改，系統裡既有的該模組節點在重新載入後就會失效**（重啟後出現「Particles.Age hasn't been encountered yet」「Input nodes on called graph not found」等）。2026-11-18 那次重啟後 spawn 腳本編譯失敗，最可能就是這一類（當時節點加入後模組又被改過）。安全順序：先把引用該模組的節點拿掉（階段只剩它時先在階段末端放一個暫時的 ParticleState 佔位）→ 用 `create_module_from_hlsl` 改好模組（HLSL 與輸入清單一次定案）→ 再加回節點（此時不會有數字尾碼）→ 刪佔位 → 重綁 User 參數 → 存檔 → **重啟驗證**。
> 12. **自訂 HLSL 不能新增發射器屬性給粒子腳本讀**：在 Emitter 腳本寫 `Emitter.X` 需要同一腳本中有綁定輸入讀過它（`Emitter.PulseRate` 能用，是因為 SpawnRate 綁定讀它），但綁定讀取在 emitter spawn 與 particle spawn 會觸發「read before being set（Fail If Previously Not Set）」，Default Mode 以現有工具改不到。所以「記住最後方向」沒有實作（§3.2.0 第 4 點）。
> 13. `get_system_diagnostics`／`request_compile` 會把系統標成 dirty。驗證完不要為此再存檔，改用 `EditorLoadingAndSavingUtils.reload_packages` 從磁碟重載該 package 清掉 dirty（不要在重載後馬上再跑 diagnostics，否則又變 dirty）。

**未驗證**：PIE 實機（遊戲攝影機、真實曝光、實際球的外觀網格與高度、`Launched` 的真實速度曲線）、連續動態的逐格觀感（只看靜態幀）、雙端／多人、與其他特效疊加、效能（四顆危險球 × 最多約 6 個半透明精靈）、Cook。

## 4. P3 持球瞄準塗鴉箭頭

- 球 `Controlled` 時，在**持球車車頭前方地面**放一張依車寬計尺寸的平面（引擎內建 Plane；長＝車寬 × `AimLengthWidthFraction`(0.575，驗收後由 1.15 減半)、寬＝車寬 × `AimWidthWidthFraction`(0.5，驗收後由 1.0 減半)），材質 `MI_FX_BallAimArrow`：持有者身份色的短粗手繪箭頭、黑墨框、8fps 三姿態「線條抖動」。
- **方向取物理根的 forward 投影到水平面**（＝實際發射方向），不讀會傾斜的視覺車身；抖動只動線條，不動方向。車頭朝天／朝地時不畫。
- 位置：取「車頭」與「**畫面上的球**沿車頭方向的遠端」兩者較遠者，再加車寬 × `AimGapWidthFraction`(0.3) 之後才是箭尾，**不壓在球下**；刻意做短，只表示方向、不暗示射程。第一版以物理球半徑估算，實機上整個箭頭被外觀較大的球蓋住。
- 控球瞬間 0.16 秒彈出（0→1.25→1）。只有箭頭顯示期間才 Tick。

## 4.1 H1 持球環（2026-11-18 追加，取代磁力束弧線）

用戶要求把持球效果靠攏塗鴉風，從四個提案中選定「球上軌道環＋車頂環」，車頂環的造型由用戶提供參考圖後定案。

- **球上**：以球的外觀中心為準、面向攝影機的平面（引擎內建 Plane，與車頂共用 `HoldRoofMesh`；原為 `UMaterialBillboardComponent`，團隊要求環隨車頭轉後改用平面）。平面只在畫面內旋轉：**環的長軸＝持球車右向在畫面上的投影，所以環始終與車頭方向垂直**（`HALBallComicFX::HoldBallQuad`，取本機第一個 PlayerController 的視角，純表現）；被球遮住的那半邊固定取畫面上方，車頭朝畫面下方時不會翻到近側。材質 `MI_FX_BallHoldRing_Ball`：兩圈持有者身份色的手繪橢圓環，一左傾一右傾，8fps 三姿態晃動；每圈的遠半邊落在球的輪廓內時被遮掉，讀成「繞著球」而不是「畫在球上」。精靈寬＝球外觀直徑 × `HoldBallSizeDiameterFraction`（程式預設 1.7，`DefaultGame.ini` 調為 1.35：團隊看過實機後要求「環只比球稍大」，環最寬處約為球徑的 1.2 倍）。
- **車上指示器**（2026-11-24 取代原本的車頂雙環星芒，團隊選定方案 2）：面向攝影機的平面（引擎內建 Plane），材質 `MI_FX_BallHoldRing_Roof`（`FX_Kind=1`）：**一個三角形箭頭**（第一版是帶環小球＋箭頭，團隊看過實機後只留三角形），8fps 兩姿態沿箭頭方向跳動。位置在車子**後半上方**：車頂中心沿地面往車尾退 車長 × `HoldMarkerBackLengthFraction`(0.3)、抬高 車寬 × `HoldMarkerLiftWidthFraction`(0.1)；箭頭在畫面上**由該位置斜指向車子中心**，車轉向時跟著轉（`HALBallComicFX::HoldMarkerQuad`，取本機視角，純表現）。大小＝車寬 × `HoldRoofSizeWidthFraction`（程式預設 1.1，`DefaultGame.ini` 為 0.9）。抬高刻意取低：第一輪用 0.45 車寬，透視視差把指示器帶到車身左後方之外。設定欄位沿用 `HoldRoof*` 命名。
- **語彙**：平塗身份色＋黑墨線＋單側重墨，與其他塗鴉層一致；兩者都關閉深度測試。
- **進出場**：控球瞬間 0.16 秒彈出（0→1.25→1）；發射或脫球後 `HoldShrinkSeconds`(0.12) 內**縮小到 0，不淡出**，車頂環縮在剛失去球的那台車上。被搶球（一次更新內換持有者）時舊標示直接切掉、在新持有者身上重新彈出。
- **觸發**：與 P3 瞄準箭頭同一條件（`Controlled`＋已解析的 `Holder`），沿用 `ResolveTransition` 的 `bStartAim／bStopAim`，沒有新增狀態或網路資料。
- **落點**：兩個元件都由球側的 `UHALBallComicFXComponent` 建立並每幀以世界座標擺放（只有顯示期間才 Tick），因此不需要逐車接線，多球時每顆球各有一組。
- **磁力束**：`bHoldReplacesBeam`(true) 且至少一個環的材質載入成功時，呼叫 `UHALMagnetBeamPresentationComponent::SetBeamRibbonSuppressed(true)` 隱藏弧線；磁力束的階段、身份色解析與球殼脈動照常運作。球殼脈動新增 `ShellPulseStepsPerSecond`(8)，亮度改為逐格變化（0＝原本的平滑正弦）。關閉 `bHoldRings` 或清空兩個材質即回到原本的弧線。
- **已知取捨**：磁力束原本以「拉長時變粗變亮」預告快要脫球，環沒有這個資訊；提案時已向用戶說明。
- **資產**：`M_FX_BallHoldRing`（`Tools/ComicDoodle/BallHoldRing.hlsl`，`FX_Kind` 0＝球環、1＝車頂環）與兩個實例由 `CreateComicDoodleAssets.py` 建立；無頭執行需加 `-AllowCommandletRendering`，否則沒有著色器統計、腳本會在存檔前自行中止。C++ 只寫 `FX_Color` 與 `FX_BallRadius`。

**H1 已驗證（2026-11-18）**

- `HAL_Future_CreationEditor` Win64 Development 建置通過（含連結，0 錯誤，本項改動的檔案無警告）。
- 獨立無頭全量 `HAL.FutureCreation` **246/246 通過、0 失敗、0 未執行**（2 項既有警告，與上一輪相同；報告 `Saved/Automation/20261118/HoldRings`，時間戳為本次產生）。`ComicDoodle.BallTrailAndAimTransitions` 內含新增的持球環斷言（縮放時間線、車頂定位、球環尺寸、設定檢查）。
- 材質：0 編譯訊息、270 條像素指令、實例暴露 28 個參數。
- 單機 PIE（`SingleVehicleTest`、Car_01、`HALSpawnBall` 後自動控球；截圖 `Saved/ComicDoodle/HoldRings/PIE1`、`PIE2`，未入版控）：球上兩圈環與車頂雙環星芒都出現、同為持有者色；球環遠半邊被球遮住的方向正確；連續三格車頂環角度不同（有在轉）；磁力束弧線沒有出現；瞄準箭頭照常。日誌 `Hold=1`（控球）、`Hold=2`（離開時進入縮小）。

**H1 未驗證**

- **最後一次全量測試之後又改了材質預設值**（星芒放大、環線變細、墨線加粗）與 `DefaultGame.ini` 的車頂環尺寸；這些只在 PIE 看過，沒有重跑自動化（測試不載入這些資產，也不讀 ini 的這個值）。
- 發射與脫球時的縮小收尾、被搶球時的切換、彈出動畫：只有日誌與單元斷言，沒有逐格看過畫面。
- 雙端／多人、其他車型與其他身份色、翻滾中的車、四人同屏的遮擋與效能、Cook。
- 墨線在實機上偏細偏軟（約 2 px，經 TAA 後呈深褐而非純黑），與既有瞄準箭頭相同；是否加粗待用戶看過決定。
- 球殼逐格脈動：測試圖的球材質槽 0 為空，看不到，未驗證。

## 5. P4 眩暈星星／B2 低 HP 汗滴

- `UHALVehicleDoodleStatusComponent` 掛在 `ATestVehiclePawn`，在 `BindVehicleAbilitySystem` 綁定、`UninitializeAbilitySystem` 解除（淘汰／換車／離場隨之清理）。
- **星星**：ASC 的 `State.Stun` 標籤計數變更事件驅動；車還活著才顯示。三顆四角星沿壓扁的橢圓繞圈（俯視下讀成水平環），8fps 跳格、隔格轉 45°。遠端拿不到眩暈剩餘時間，所以**不做倒數**。
- **汗滴**（驗收後預設關閉，`bLowHealth=false`）：`OnHealthChanged` 驅動；HP 比例 ≤ `LowHealthFraction`(0.3) 且 > 0 時顯示。用常駐的汗滴而非閃爍的「!」——後者會被讀成機關預警。
- 尺寸依車寬：星星精靈＝車寬 × 1.25、汗滴＝車寬 × 1.0，高度＝車頂上方車寬 × 0.3／0.22（第一版用絕對 170／130 cm，實機上每顆星只有約 8 px）。
- 兩者都是車頂上方的 `UMaterialBillboardComponent`（世界座標、世界向上），動畫全在材質裡；只有標示顯示期間才 Tick（讓翻滾中的車標示仍在正上方）。材質關閉深度測試。

## 6. N1 畫面外來球預警（純本地）

- 每位本地玩家一個不可互動的 Slate 疊層（`UHALIncomingBallHUDSubsystem`，由球的表現元件在 BeginPlay 確保建立，世界清理時移除）。
- **出現條件**（全部成立）：球為 `Launched`、發射者不是自己、球投影在畫面外、水平最近距離 ≤ `IncomingMissDistanceCm`(300) 且抵達時間 ≤ `IncomingMaximumSeconds`(1.5 s)、球速 ≥ `IncomingMinimumSpeed`(300 cm/s)。
- **防亂閃**：條件連續成立 0.12 s 才出現；出現後至少顯示 0.3 s；同時最多 2 枚，取最快抵達者。
- **消失**：球進入畫面（交棒給 P1 尾跡）、離開 `Launched`、不再對準；本地車被淘汰／觀戰時整層清空。
- 造型：畫面邊緣內縮 6% 處一枚發射者身份色的粗箭頭（長度＝畫面高 7%），黑墨框＋右下重墨，8fps 三姿態擺動，0.1 s 彈出。
- 只讀已複製的球狀態與本地攝影機投影、只寫自己；**不從本地攝影機反向驅動任何權威狀態**（AGENTS.md §11）。

---

## 7. 調參入口

全部在 Project Settings → Game（寫入 `Config/DefaultGame.ini`），**設定在 BeginPlay／綁定時讀一次，改完要重開 PIE**：

| 設定頁 | 類別 | 內容 |
|---|---|---|
| Vehicle Impact Burst FX | `UHALImpactBurstSettings` | 火花開關、數量、系統 |
| Ball Comic FX | `UHALBallComicFXSettings` | 尾跡、瞄準箭頭、來球預警 |
| Vehicle Doodle Status FX | `UHALVehicleDoodleStatusSettings` | 眩暈星星、低 HP 汗滴 |

- 每一項都可單獨關閉：`bTrail`／`bAim`／`bIncomingWarning`、清空 `StunMaterial`／`LowHealthMaterial`、`bSparks`；各頁 `bEnabled=False` 整頁關閉。
- 造型微調改對應的材質實例；母材質與實例由幂等腳本重建（`Tools/ComicSparks/CreateComicSparkAssets.py`、`Tools/ComicDoodle/CreateComicDoodleAssets.py`），**不要手改母材質圖**。
- 調試：`hal.BallComicFXLog 1`（尾跡／箭頭啟停）、`hal.IncomingBallDebug 1`（來球箭頭也對自己的球與畫面內的球顯示——單人測試看造型用，預設 0）、既有 `hal.VehicleImpactBurstLog 1`。
- Cook：三頁設定的軟引用資產都已接到既有的 `ModifyCook` 根集合收集。

---

## 8. 已驗證／未驗證（2026-11-18）

**已驗證**

- `HAL_Future_CreationEditor` Win64 Development **編譯通過**（exit 0），本輪新增／改動的檔案零警告（既有的 C4305 等警告仍在別人的檔案裡）。
- 獨立 `UnrealEditor-Cmd.exe` 無頭全量 `HAL.FutureCreation` **246/246 通過、0 失敗、0 未執行**（2 項既有警告：`VehicleTest.InvalidAndBlockedReplacementRetainsCurrentCar`、`VehicleTest.OpponentSpawnsReplacesAndSurvivesPlayerSwap`；報告 `Saved/Automation/20261118/ComicDoodleFinal2`，時間戳為本次產生）。新增三支：`Presentation.ComicDoodle.BallTrailAndAimTransitions`、`.IncomingBallWarning`、`.StunStarsAndLowHealth`。
- 材質腳本各重跑多次：status ok、0 編譯訊息、節點數前後一致（`M_FX_ComicSpark` 24、`M_FX_BallAimArrow` 19、`M_FX_BallInkTrail` 14、`M_FX_DoodleStatus` 23），實例契約參數全數暴露；`NS_VehicleImpactSparks`、`NS_BallInkTrail` `validate_system` valid（各只剩一條 SpriteSize／RibbonWidth 初始化誤報，SimCache 實測有值）；編輯器 dirty packages = 0。
- **PIE 實機截圖**（`SingleVehicleTest` 圖、遊戲俯視攝影機、60 fps；由 subagent 經 Monolith 操作並逐張看圖，圖在 `Saved/ComicDoodle/PIE/R3`、`R4`、`R5`，未入版控）：
  - S0：車對車撞擊出現飽和黃色墨框閃電條，每條約 1/2 車寬、飛約一個車寬後縮小消失；與粉色爆點碎片分得開。日誌 `Publish … Sparks=5`。
  - P1：發射後出現發射者色的鋸齒墨框尾跡（頭寬約 0.6 車寬、長約 1.7 車寬）；球回 Free 後日誌 `Trail=0`、Niagara 元件 `IsActive=False`、畫面無尾跡。
  - P3：控球時車頭前方地面出現持有者色箭頭，箭頭 yaw 與車 yaw 讀值一致（−99.31）；直行與轉向時都露在球外。
  - P4：`HALStun` 後車頂出現三顆黃色四角星（每顆約 1/5 車寬），連拍位置逐格改變；結束後消失。
  - B2：彈簧拳（權威傷害）三次把 HP 打到 25/100 後出現藍色汗滴（每顆約 1/4 車寬）。
  - N1：以除錯開關 `hal.IncomingBallDebug 1` 對自己發出的球觸發，含 UI 的截圖看到畫面邊緣一枚發射者色粗箭頭（黑墨框＋右下重墨）指向出畫面的球。
- 特效評審（subagent）看過 R3 實機截圖：S0「達到要求」；一項必改（星形點綴與眩暈星星撞形）已修；建議的尾跡／箭頭飽和度、壽命、墨框、汗滴顏色已改並重拍。架構評審（subagent）讀過全部未提交改動：未發現寫入權威狀態或從本地攝影機反向驅動；指出的硬編碼路徑與魔法數已修。

**未驗證（沒做過，不要當成做過）**

- **雙端／多人**：全部只在單機 PIE 看過。客戶端上的啟停時機（晚一個 RTT）、Listen Server 主機與客戶端各自的畫面、晚加入、`State.Stun` 在模擬代理上的可見性（推論與 Shocked 同路徑，未實測）都沒有測。
- **N1 的正式條件**：他人發射的球、防亂閃、同時最多 2 枚、四人同屏的觀感；只看過除錯開關下的造型。
- **正式的車對車對撞**：實機用的是未操控的同款車，只有一方發火花事件；兩車各發一個事件（數量乘二）的觀感沒看。
- 火花、尾跡、箭頭、星星的逐格跳動只看過靜態幀，沒看過連續動態；與既有受擊紅閃、電擊、技能特效的疊加；危險區附近的遮擋；四人同時的效能；Cook／發布包。
- P4 與電擊（Shocked）並存時的觀感（審議 1:1，暫採照樣顯示）。
- 測試木樁（`AHALSkillTestDummy`）沒有掛眩暈星星元件。

**本輪發現、未修的既有問題**（不屬於本輪範圍，詳見狀態文件）

- 編輯器在背景被節流到約 3 fps 時，持球車受撞脫球會**死結**：`HALBallPresentationComponent.cpp` 的 `StopCapture()` 在 Chaos 碰撞事件分派內呼叫 `WaitOnPendingTasks_External()`。
- 測試圖的球 `VisualMesh` 材質槽 0 為空，每次控球都有 `CreateDynamicMaterialInstance … Material index 0 is invalid` 警告（來源磁力束）。
