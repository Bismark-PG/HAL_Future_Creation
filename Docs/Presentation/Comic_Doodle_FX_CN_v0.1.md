# 塗鴉／漫畫風表現層（Comic Doodle FX）v0.1

> 2026-11-18 自動化疊代輪。關鍵字：**塗鴉、漫畫風**（語彙沿用胎痕噴漆與 Impact Burst 墨線碎片：平塗填色＋黑色墨線＋右下重墨＋「逐格」跳動、以縮小而非淡出收尾）。
> 全部為**純表現**：不改物理、傷害、球規則、控球／發射許可或任何權威結果；除既有事件外**沒有新增任何網路資料**。

本輪共六項：一項必做（S0 火花改版）＋五項新效果。驗證結論見 §8，**未做過的檢查不寫成通過**。

| 代號 | 效果 | 觸發（全部讀已複製狀態） | 落點 |
|---|---|---|---|
| S0 | 車對車漫畫火花 | 既有 Impact Burst 事件的 `Sparks` | `NS_VehicleImpactSparks`＋`M_FX_ComicSpark` |
| P1 | 危險球火焰尾跡 | 球 `Launched` | `UHALBallComicFXComponent`＋`NS_BallInkTrail` |
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

## 3. P1 危險球火焰尾跡（2026-11-18 由墨帶改版）

> **改版原因（用戶驗收回饋）**：原本的 Ribbon 墨帶「形狀像火，但拖曳的動態不像火焰」——整條是一體的帶子、只會整體變細。用戶確認示意後改為由獨立火團組成的漫畫火焰。資產路徑與 C++ 契約不變，**沒有改 C++**。原 Ribbon 版（`NS_BallInkTrail`／`M_FX_BallInkTrail`／`MI_FX_BallInkTrail`／`BallInkTrail.hlsl`）備份在 `Saved/ComicDoodle/Flame/Backup_InkTrail/`（不入版控）。

- 球進入 `Launched` 時啟動 `NS_BallInkTrail`（名稱沿用，內容已是火焰）：發射者身份色、黑墨框、Unlit／Translucent（不加法、不發光）。四個動態重點：
  1. **火團各自獨立**：主體由一顆顆拉長的火滴精靈重疊而成，出生時帶隨機側向偏移與大小差，離球後各自搖擺，整體頭粗尾細、邊緣不規則。
  2. **火舌甩開脫離**：另一個發射器從主體兩側邊緣甩出較小的火舌，強阻力、很快燒盡。
  3. **內亮外深**：年輕火團有往白提亮的焰心，重疊後在靠近球的一段連成亮色內焰。
  4. **漫畫感**：12 fps 階梯（位置、尺寸、尾彎都只在換格時跳）、以縮小收尾（最後一格尺寸 0）、不淡出。
- 寬度基準 W＝`User.TrailWidth`＝**球的外觀直徑** × `TrailWidthDiameterFraction`(0.75)（測試圖約 240 cm）；系統內所有尺寸都是 W 的倍率。
- 球離開 `Launched` 的**同一次狀態處理**就 `DeactivateImmediate`（`bCutTrailImmediately=true`）：尾跡若比危險狀態活得久，會把無害的球繼續標成危險。
- 狀態比對用 **`StateSequence`＋發射者 PlayerId**，不是狀態邊緣；由權威端 `CommitRepState` 與客戶端 `OnRep_BallRepState` 兩處呼叫（Listen Server 主機不走 OnRep），做法比照磁力束。

### 3.1 材質 `M_FX_BallFlame`

`Tools/ComicDoodle/BallFlame.hlsl`＋`CreateComicDoodleAssets.py` 的 FLAME 段（可單獨重建：`CreateComicDoodleAssets.py FLAME`）。單一 Custom 節點、無貼圖；顏色、尺寸、繪製格數、年齡、焰心開關由 Niagara 透過 Particle Color／Particle Size／Dynamic Parameter 0 傳入，沒有 runtime driven 參數。三個實例以 `FX_Layer` 分層：`MI_FX_BallFlame_Ink`(0，黑色剪影)、`_Fill`(1，填色)、`_Heart`(2，焰心)。

預設值：HeadRadius 0.55、Margin 0.34、TailPower 1.7、TailBend 0.38（每格左右翻）、TipBias 0.08、Boil 0.03、InkWidth 0.28、CoreScale 0.82、CoreLife 0.55、CoreLift 0.55、CoreGain 1.8、ColorLift 0、FillGain 1、Opacity 1、InkColor 黑、FX_Reverse 0。

> **墨框融合**：每顆精靈各畫自己的墨框，重疊處會滿是內部黑線（魚鱗）。做法是同一發射器多個 Sprite Renderer，用 SortOrderHint 分層：黑色剪影 → 填色 → 焰心。焰心獨立一層，是因為放在填色層時年輕火團的填色會蓋掉年老火團的焰心，只剩白色月牙。副作用：兩團之間比兩倍墨寬還窄的縫會被塗成一小段黑色摺痕。
>
> **坑：焰心提亮到接近純白會被 tonemapping 壓成灰。** 第一版 CoreLift 0.75，紅藍兩色的焰心都呈同一種灰；改為 0.55 並以 CoreGain 1.8 推亮。

### 3.2 `NS_BallInkTrail` 實際設定

經 Monolith 編輯器連線製作，**沒有腳本**。自訂模組 `/Game/Assets/FX/Modules/NM_BallFlameSpawn`、`NM_BallFlameUpdate` 的 HLSL 只存在於模組資產內，`Tools/ComicDoodle/NiagaraBallFlameSpawn.hlsl`／`NiagaraBallFlameUpdate.hlsl` 是原文紀錄，改模組要手動同步。**三個資產必須一起提交。** User 參數 `TrailColor`、`TrailWidth`（預設 240，只影響編輯器預覽）名稱必須與 `UHALBallComicFXSettings` 一致。

| 項目 | FlameBody | Tongues |
|---|---|---|
| 模擬／EmitterState | CPU、Local Space 關；Self、Loop Infinite、Duration 5 | 同左 |
| SpawnPerUnit | 間距 W×0.32、Movement Tolerance 0.5 | 間距 W×0.2、Spawn Probability 0.7 |
| SpawnRate（低速下限） | 16/s | 8/s |
| SpawnBurst_Instantaneous（啟動補量，只觸發一次） | 0.034 s 時 6 顆 | 0.034 s 時 3 顆 |
| NM_BallFlameSpawn（取代 InitializeParticle） | 寬 W×0.7～1.2、長寬比 2.2、壽命 0.35～0.5 s、Inherit 0.15、MinBackSpeed 4.0 W/s、MinValidSpeed 0.2 W/s、BackOffset 0.62 W、DropHeight 0、SideJitter ±0.17 W、TailLean 0.25（±0.15）、StartWindow 0.2 s／StartBoost 1.0／StartSizeScale 0.35、Core 1 | 寬 W×0.35～0.5、長寬比 2.4、壽命 0.3～0.45 s、Inherit 0.15、MinBackSpeed 4.0、MinValidSpeed 0.2、BackOffset 0.4 W、側向初速 W×1.2～2.0/s、SideOffset 0.5 W（左右隨機）、SideJitter ±0.05 W、TailLean 0.6（±0.15）、StartWindow 0.2／StartBoost 1.0／StartSizeScale 0.35、Core 0 |
| NM_BallFlameUpdate（取代 SolveForcesAndVelocity） | 12 fps 階梯；尺寸 0.85 →（0.2）1.1 → 自 0.15 線性縮小，最後一格 0；MinScale 0.12；搖擺 W×0.2、2.5 Hz；Drag 0 | 12 fps；0.7 →（0.2）1.0 → 自 0.25 線性縮小；MinScale 0.35；不搖擺；Drag 6/s |
| Renderers（Sprite、CustomAlignment、FaceCamera、依 Age 降序＝年輕在上、Pivot (0.5, 0.2)＝頭端） | Ink SortOrder 1／Fill 3／Heart 4 | Ink 0／Fill 2 |

**為什麼有 BackOffset／MinBackSpeed／啟動補量（第一次 PIE 實機的發現）**：系統掛在球心，而球是外觀半徑約 0.67 W 的不透明球體、Launched 會持續 4 秒以上且球速從約 2200 掉到約 450 cm/s。第一版以精靈中心為樞紐、從球心出生、只按距離生成：粗頭與內焰被球蓋住甚至從球的**前緣**冒出，球後只露出約 1/3 球徑；尾長正比於球速，約 500 cm/s 時完全看不到（但球仍危險）；發射後約 0.25 s 才成形。修正：樞紐移到頭端並沿行進反方向後移 `BackOffset`；粒子相對球的後退速度＝max(球速×(1−Inherit), `MinBackSpeed`×W)，慢球時火焰主動往後吹；加時間型 SpawnRate 與一次性 Burst。球速低於 `MinValidSpeed` 時方向不可信，該批粒子直接丟棄（模組讀不到上一幀方向）。材質墨框同時由 0.14 加粗到 0.28（Margin 0.2 → 0.34），因為實機鏡頭距離下原墨框只有數像素。

離屏量測（W=240；`fx.DumpParticleData` 讀回；`Saved/ComicDoodle/Flame/measure_round3_speeds.json`）：粒子留在世界空間（z 偏移 0）；火焰最前緣在球心後方 0.27～0.37 W；主體長度（自球後緣）450 cm/s 時 2.1 W、1000 cm/s 時 2.1 W、2500 cm/s 時 3.55 W；啟動 0.1 s 時已有 9 顆主體、5 顆火舌；尺寸縮到 0。只測了等速直線。

**啟動段（第二次 PIE 的發現）**：為了讓發射後立刻成形而加的 `StartBoost`（後退速度 ×3）會把火焰噴到三個球直徑長、整個蓋住發射車，已改回 1.0。單顆主體精靈本身長約 2.2 W，剛發射時仍會伸到發射點後方約 4 m，因此兩個模組都加了 `StartSizeScale`：出生於前 `StartWindow` 秒的粒子整個壽命都以 lerp(0.35, 1, 出生時刻／StartWindow) 縮小（update 模組每幀重算尺寸，所以兩邊都要套）。離屏量測（2500 cm/s）：畫出的尾端在發射點後方 193／263／203／85 cm（t=0.05／0.10／0.15／0.30 s），0.3 s 時主體 2.81 W 接近穩態；所有粒子世界速度不朝行進反方向。**這一項改完後沒有再跑 PIE。**

**兩次單機 PIE（`Saved/ComicDoodle/PIE/R7`、`R8`，不入版控）看到的**：球前緣無火、粗頭與白色內焰緊貼球後完整露出（露出寬約 0.6～0.9 個球直徑）；約 2000 cm/s 時連續、頭粗尾尖、長約 1.5～2 個球直徑；250～500 cm/s 時仍有約一個球直徑的完整火焰、方向穩定；離開 `Launched` 同幀消失；無 Niagara／材質警告。**已知觀感，待團隊判斷**：(1) 撞牆反彈後約 0.1～0.2 s 舊方向的火團留在原地燒完，球貼牆時火焰被牆面截掉；(2) 慢球仍拖著固定長度的噴焰（`MinBackSpeed` 的刻意下限，嫌不自然就調低）；(3) 墨框是完整一圈但一側細一側粗、像帶投影的描邊，原因未查明；(4) `Launched` 實際由低速檢查或命中結束（實測 2.3～4.9 s），撞牆不會結束。

> **坑：Niagara 模組改值後 `request_compile` 回成功，模擬卻仍用舊值。** 要臨時加一個發射器再刪掉，強迫系統整個重編後再存檔；對已編譯系統用 `set_module_input_value` 改既有模組輸入時連這招也無效，可靠做法是**先加一個新模組並設好所有值，再刪舊的**（階段只剩一個模組時不可先刪後加，stack 會斷鏈）。自訂 HLSL 裡的 `bool` 區域變數與 `sign()` 可能讓 spawn 腳本編譯失敗且錯誤訊息不明確。另：`create_module_from_hlsl` 對既有路徑會直接原地覆寫、不報錯；`NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate` 對這個系統的發射器會觸發引擎 assert 讓編輯器崩潰（本輪發生一次），量測請改用 `fx.DumpParticleData`。

## 4. P3 持球瞄準塗鴉箭頭

- 球 `Controlled` 時，在**持球車車頭前方地面**放一張依車寬計尺寸的平面（引擎內建 Plane；長＝車寬 × `AimLengthWidthFraction`(0.575，驗收後由 1.15 減半)、寬＝車寬 × `AimWidthWidthFraction`(0.5，驗收後由 1.0 減半)），材質 `MI_FX_BallAimArrow`：持有者身份色的短粗手繪箭頭、黑墨框、8fps 三姿態「線條抖動」。
- **方向取物理根的 forward 投影到水平面**（＝實際發射方向），不讀會傾斜的視覺車身；抖動只動線條，不動方向。車頭朝天／朝地時不畫。
- 位置：取「車頭」與「**畫面上的球**沿車頭方向的遠端」兩者較遠者，再加車寬 × `AimGapWidthFraction`(0.3) 之後才是箭尾，**不壓在球下**；刻意做短，只表示方向、不暗示射程。第一版以物理球半徑估算，實機上整個箭頭被外觀較大的球蓋住。
- 控球瞬間 0.16 秒彈出（0→1.25→1）。只有箭頭顯示期間才 Tick。

## 4.1 H1 持球環（2026-11-18 追加，取代磁力束弧線）

用戶要求把持球效果靠攏塗鴉風，從四個提案中選定「球上軌道環＋車頂環」，車頂環的造型由用戶提供參考圖後定案。

- **球上**：以球的外觀中心為準、面向攝影機的平面（引擎內建 Plane，與車頂共用 `HoldRoofMesh`；原為 `UMaterialBillboardComponent`，團隊要求環隨車頭轉後改用平面）。平面只在畫面內旋轉：**環的長軸＝持球車右向在畫面上的投影，所以環始終與車頭方向垂直**（`HALBallComicFX::HoldBallQuad`，取本機第一個 PlayerController 的視角，純表現）；被球遮住的那半邊固定取畫面上方，車頭朝畫面下方時不會翻到近側。材質 `MI_FX_BallHoldRing_Ball`：兩圈持有者身份色的手繪橢圓環，一左傾一右傾，8fps 三姿態晃動；每圈的遠半邊落在球的輪廓內時被遮掉，讀成「繞著球」而不是「畫在球上」。精靈寬＝球外觀直徑 × `HoldBallSizeDiameterFraction`（程式預設 1.7，`DefaultGame.ini` 調為 1.35：團隊看過實機後要求「環只比球稍大」，環最寬處約為球徑的 1.2 倍）。
- **車頂**：持球車世界包圍盒頂面上方 `HoldRoofLiftCm`(6) 的水平平面（引擎內建 Plane），材質 `MI_FX_BallHoldRing_Roof`：**兩圈同心細環＋上下左右四顆四角星芒**，星芒朝外的尖端短、朝圓心的尖端細長且不碰到圓心；整體以 0.12 圈／秒逐格慢轉，星芒隔格縮到 0.86。寬＝車寬 × `HoldRoofSizeWidthFraction`（程式預設 1.1，`DefaultGame.ini` 現為 0.975：曾調到 1.3，團隊看過實機後要求縮為 75%；星芒本身已在材質預設值放大）。永遠取世界向上，車翻滾時仍在正上方。
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
