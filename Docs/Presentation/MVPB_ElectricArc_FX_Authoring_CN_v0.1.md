# 電流特效（兩柱之間電弧）製作步驟

## 0. 本文件範圍

本文件只涵蓋 **Niagara 電弧特效本體的製作**，用於兩根柱子之間的電流連接表現。

不包含：

- 碰撞體與傷害判定（後續由機關 Actor 的 `UBoxComponent` + `FVehicleHitContext` → `CombatResolver` 負責）
- 機關狀態機（`Idle / Warning / Active / Cooldown`）與複製
- 球是否被電牆阻擋的玩法規則（未確認，見第 9 節）
- 柱子本體的網格製作與碰撞設定

特效屬於表現層。按 AGENTS.md 第 7 節，Niagara 與藍圖不得直接修改 HP，也不得決定「電流當前是否造成傷害」。

Niagara System 是 `.uasset`，按 AGENTS.md 第 8.3 節由團隊在 Unreal Editor 內手動完成，本文件提供可直接執行的操作步驟。

---

## 1. 資產位置與命名

特效資產放在 `Content/Assets/FX/`，與既有的 `Assets/Cars`、`Assets/Materials`、`Assets/Textures` 並列。`Content/Assets/` 是本工程自製資產的根目錄，與第三方素材區 `Content/Imported/` 分開。

```text
Content/Assets/FX/
├── NS_ElectricArc          Niagara System
├── NS_TireSkid             （既有）
├── Materials/              M_FX_Beam_Additive, MI_FX_Beam_Cyan
└── Textures/               T_FX_BeamGradient
```

資產數量變多之後，可再往下分 `Systems/` 與 `Emitters/` 子目錄；目前只有兩個 System，先平放即可。

設計理由：

- FX 材質需要 Additive + Unlit + Niagara usage flag，與車體 PBR 材質混放會難以維護；獨立目錄也讓 `FX/` 成為可整包 Migrate 的封閉單元。
- 不放入 `Content/Blueprints/MapMechanic/`：該目錄是機關藍圖（邏輯組裝）。電弧只是第一個特效，後續命中火花、球拖尾、危險區警示屬於跨系統共用資源。
- 不放入 `Content/Imported/`：該目錄為第三方素材區，混入自製資產會影響素材包更新。
- 手感試驗階段可先放 `Content/Developers/USER/`（預設不打包），定稿後移動，並對來源目錄執行 **Fix Up Redirectors in Folder** 清理 redirector。

命名前綴沿用 UE 慣例：`NS_` System、`NE_` Emitter、`NM_` Module Script、`M_` / `MI_` 材質、`T_` 貼圖。

UE 的 Content 目錄需有資產才存在，空目錄不會被 Git 追蹤，因此目錄在 Content Browser 內隨首個資產一起建立即可。

---

## 2. 建立 Niagara System

1. Content Browser 右鍵 → **FX → Niagara System**。
2. 選擇 **New system from selected emitter(s)**。
3. 在模板清單選 **Dynamic Beam**。
4. 儲存為 `Content/Assets/FX/NS_ElectricArc`。

Dynamic Beam 模板本身即為「兩點之間的 Ribbon 光束」，骨架可直接使用，不需從空白 System 搭建。

### 2.1 首次開啟的模組版本相依性修正

開啟模板後，`Spawn Beam` 模組可能出現紅色錯誤：

```text
The module has unmet dependencies.
The following pre-dependency is not met: BeamEmitterSetup;
Switch BeamEmitterSetup module version from 1.0 to 3.0
```

成因：模板資產內的 `Spawn Beam` 已升級到 2.0（支援 Position 型別），但 `Beam Emitter Setup` 仍停在 1.0，兩者版本不相容。這是引擎模板的既有狀況，不是操作錯誤。

處理：點 **Fix issue**，再 **Compile** 並儲存。

**不要點 Dismiss issue。** 該按鈕只隱藏警告、不修正版本，端點設定會因此無法正常運作。上方的 `Version upgrade note`（Spawn Beam 2.0: Support position type）屬於說明性質，確認後可按 Dismiss note。

---

## 3. 兩項必須確認的設定

開啟 `NS_ElectricArc`，選中 Emitter 節點：

| 設定 | 值 | 原因 |
|---|---|---|
| Emitter Properties → **Sim Target** | `CPUSim` | Niagara Ribbon Renderer 不支援 GPU Sim，此項無替代方案 |
| Emitter Properties → **Local Space** | 勾選 | 端點使用相對座標，機關 Actor 移動或旋轉時電流自動跟隨，無需額外處理 |

---

## 4. 將端點開放為 User Parameter

目的：柱距在編輯器內可調，電流長度自動跟隨，不需重做特效。

1. 左側 **Parameters** 面板 → `User Exposed` 區塊點 **+**。
2. 新增兩個參數，命名 `BeamStart`、`BeamEnd`，型別選 **Position**。
   - `Beam Emitter Setup` 升級到 3.0 後端點即為 Position 型別（對應 UE5.1 之後為支援 LWC 的改動）。型別不一致無法連接。
   - 若尚未完成第 2.1 節的版本修正，此處型別會對不上。
3. 端點設定在 **Emitter Update → Beam Emitter Setup** 模組上，不在 `Spawn Beam`。
   展開該模組，將 `Beam Start` / `Beam End` 兩個輸入分別點右側下拉箭頭 → **Link Inputs** → 選 `User.BeamStart` / `User.BeamEnd`。
   - `Particle Spawn → Spawn Beam` 的職責是把粒子沿著這條線分布並寫入 `RibbonLinkOrder`，它讀取 Beam Emitter Setup 的結果，本身不持有端點。

---

## 5. 電弧動態

核心只由兩個模組決定。

**Emitter Update → Spawn Burst Instantaneous**

- `Spawn Count` = `32` ~ `64`。這是光束沿線的取樣點數。過少會呈現折線，過多無收益。

**Particle Update → Jitter Position**（模板若未帶，點 **+** 新增）

- `Jitter Amount` = `15` ~ `30`（單位 cm），控制鋸齒幅度。
- `Jitter Delay` = `0.03` ~ `0.08`（秒），控制重新隨機的間隔。

> 放電的「噗滋」閃爍感主要來自 `Jitter Delay`，調整該值比調整幅度有效。

**不要使用 Curl Noise Force。** 該模組需配合 Solve Forces and Velocity，而 Beam 粒子不做速度積分，接入後表現為飄動而非放電。

---

## 6. 三層 Ribbon 疊加

俯視角下單條細電弧無法讓玩家判斷安全距離。AGENTS.md 第 12 節要求危險區邊界清楚可讀，因此將視覺拆成兩種角色：一條近乎不抖動的核心線負責「碰撞線在哪」，外層電弧負責「這裡危險」。

在 System 內將 Emitter 右鍵 **Duplicate** 兩次，三個 Emitter 分工：

| Emitter | Ribbon Width | Jitter Amount | Jitter Delay | 作用 |
|---|---|---|---|---|
| `Core` | 6 ~ 10 | `0` ~ `3` | — | 亮核心線，玩家實際讀取的危險線 |
| `ArcInner` | 3 ~ 5 | `20` | `0.06` | 主電弧 |
| `ArcOuter` | 2 ~ 3 | `35` | `0.03` | 外層亂流 |

Emitter 命名規則：**以職責命名，PascalCase，不帶編號與空白**。模板複製出的 `DynamicBeam` / `DynamicBeam001` / `DynamicBeam002` 必須改名——編號不表達職責，調參數時無法分辨改的是哪一層。

不使用 `Beam01_` 這類序號前綴：之後插入新層會讓既有編號失去意義，而 Niagara 的堆疊順序本來就是手動排列，序號不提供額外資訊。

後續擴充沿用同一規則，例如貼地光帶命名為 `GroundLine`、命中火花命名為 `ImpactSpark`。

改名方式：在 System Overview 內雙擊 Emitter 標題，或右鍵 → Rename。Emitter 名稱只在 System 內部使用，改名不影響外部引用此 System 的資產。

- Ribbon 寬度位於 **Particle Spawn → Initialize Ribbon → Ribbon Width**。
- **Tessellation 在 Ribbon Renderer 上**（Renderer 的 `Tessellation` 分類），不在 Initialize Ribbon 模組內。此值直接決定電流看起來是圓弧還是尖銳折線，設定方式見第 6.1 節。
- 三個 Emitter 共用同一組 `User.BeamStart` / `User.BeamEnd`，端點改一次三層同步。

### 6.1 轉折的銳利度

電流應呈現尖銳折線，而非平滑波浪。影響銳利度的有三項，依效果大小排列。

**1. Ribbon Renderer → Tessellation（影響最大）**

Tessellation 會在粒子之間插入曲線細分，數值越高轉折越圓滑。要尖銳折線就把 `Tessellation Mode` 設為 **`Disabled`**，粒子之間改以直線相連，轉角即為硬折角。

若需保留部分細分，改用 `Custom` 並把 Tessellation Factor 降到 `1` ~ `2`；同分類下的 `Curve Tension` 調高也能收緊曲率。屬性名稱在不同引擎版本略有差異，以 Ribbon Renderer 的 Tessellation 分類為準。

**2. Spawn Count（影響次大）**

取樣點越多，每段越短，視覺上趨近平滑曲線；取樣點越少，直線段越長、轉折越突出。真實閃電的特徵正是「少量長直線段 + 大角度急轉」。

持續電弧建議 `12` ~ `20`，不要用第 5 節起始值的上限。

**3. Jitter Amount 與段長的比例（決定折角角度）**

```text
段長 = 光束長度 ÷ Spawn Count
```

折角是否明顯取決於 `Jitter Amount` 相對段長的比例，而非絕對值。`Jitter Amount` 達到段長一半以上時，轉折角度才會夠大。

範例：光束長 500 cm、Spawn Count 16，段長約 31 cm，此時 `Jitter Amount` 取 `20` ~ `30` 可得到明顯的銳角。若把光束拉長到 1000 cm 而不調整，段長加倍、折角就會變鈍，需同步提高 `Jitter Amount`。

> 若轉折仍偏圓，先確認 `Particle Update` 內沒有啟用 Curl Noise Force 或其他平滑類力場——這類模組會把折線抹成波浪。

### 6.2 端點表現

電流兩端需要明顯的接點。分三個層次處理，順序即建議的實作順序。

**1. 先修 Beam Width 曲線（必做）**

模板的 `Particle Spawn → Beam Width` 使用 `Float from Curve`，以 `Particles.RibbonLinkOrder` 為索引。預設曲線在兩端收到 `0`，因此 Ribbon 在端點逐漸變細消失——這種情況下不論加什麼發光體，接點都會看起來虛浮。

將曲線首尾兩個 Key 的值提高到非零（或整條曲線拉平），讓 Ribbon 在端點保持寬度。

**2. 端點發光體做在柱子上（建議的主要做法）**

不放進 `NS_ElectricArc`，而是作為柱子 Actor 的一部分：柱頂發光網格（Unlit Emissive 材質）加一盞 Point Light。

理由是玩法層的：機關的 `Warning` 階段必須讓玩家在電流尚未啟動前就看見電牆位置（AGENTS.md 第 12 節）。若端點只存在於電流特效內，電流關閉時端點一併消失，玩家無法預先判讀場地。端點獨立於電流，才能在 `Idle` / `Warning` / `Active` 三個階段各自改變顏色與亮度。

此做法也不依賴 Niagara，開關只需改材質參數與燈光強度，由機關狀態機驅動。

**3. 接點閃光 Sprite（電流啟動時的附加層）**

在 `NS_ElectricArc` 內新增一個 Sprite Emitter，命名 `EndNode`，用來表現電流擊中柱子的瞬間輝光。

設定：

- `Emitter Update → Spawn Burst Instantaneous`，`Spawn Count = 2`
- `Particle Spawn` 新增 `Set new or existing parameter` 模組，寫入 `Particles.Position`
  - 值選 Lerp / Linear Interpolate 動態輸入
  - `A = User.BeamStart`、`B = User.BeamEnd`、`Alpha = Particles.NormalizedExecIndex`
  - 端點為 Position 型別時，選 Position 版本的 Lerp
- `Render` 使用 Sprite Renderer，材質為 Additive + Unlit
- `Local Space` 必須與其他三層一致，否則端點會與電流錯位

兩顆粒子的 `NormalizedExecIndex` 恰為 `0` 與 `1`，因此會分別落在光束起點與終點，柱距改變時自動跟隨，無需額外設定。

### 6.3 Ribbon 朝向（Facing Mode）

Ribbon Renderer 的 `Facing Mode` 預設為 **`Screen`**，即 Ribbon 永遠面向攝影機。除非手動改過，目前的電流即為此模式。

**本專案維持 `Screen`。**

理由：工程使用俯視角（AGENTS.md 第 2 節）。若改成固定平面朝向（`Custom Side Vector` 搭配固定向量），兩柱之間的電流會成為一片垂直薄片，從上方俯視時正好是邊緣方向，寬度幾乎為零、近乎不可見。`Screen` 模式下 Ribbon 永遠以完整寬度朝向玩家，是俯視角唯一合理的選擇。

四人連線時每名玩家各自擁有攝影機，`Screen` 朝向在各自的客戶端分別計算，屬於純渲染行為，不涉及權威狀態，不會造成玩家之間的表現不一致。

檢查方式：選中 Ribbon Renderer，於 Details 內查看 `Facing Mode`。在 Preview 內旋轉視角，Ribbon 寬度不隨視角改變即為 `Screen` 模式。

注意 Preview 視窗的攝影機角度與實際遊戲不同。判斷可讀性時，須將 Preview 攝影機調整到接近遊戲俯視角的高度與俯角，或直接在關卡內以實際攝影機確認。

另建議增加一條貼地的細光帶（第四個 Emitter 或 Decal），標示電流在地面的投影位置。俯視角玩家主要依靠地面判讀，空中光束在透視下位置會產生誤差。本項可在核心效果定稿後再加。

---

## 7. 材質

模板附帶的 beam 材質可先用於驗證流程。製作正式電流材質時：

1. 新建 Material，儲存為 `Content/Assets/FX/Materials/M_FX_Beam_Additive`。
2. **Blend Mode = Additive**，**Shading Model = Unlit**。
3. Details → **Usage → Used with Niagara Ribbons** 必須勾選。未勾選時渲染為棋盤格。
4. Emissive Color = 軟邊漸層貼圖 × `Particle Color` × 強度常數。
5. 強度建議：Core 層 `20` ~ `50`（依靠 Bloom 產生光暈），Arc 層 `5` ~ `15`。
6. 建立 Material Instance 供各層分別調色，避免重複編譯材質。

配色方向：青白核心 + 外層偏藍紫。工程基調為明亮的未來體育慶典（AGENTS.md 第 12 節），純白偏醫療感，純紫偏賽博龐克，均與基調不符。

---

## 8. 驗證步驟（團隊手動執行）

本階段不需要任何 C++ 改動，也不需要機關 Actor。

1. 將 `NS_ElectricArc` 直接拖入測試關卡。
2. 選中 Niagara Component → Details 面板 **Niagara** 分類下覆寫 User Parameters，手動填入 `BeamStart` / `BeamEnd` 兩個座標。
3. 兩值拉開數百 cm，確認：
   - 電流正確連接兩點，長度隨參數變化
   - 鋸齒與閃爍頻率符合預期
   - Core 層位置穩定，不隨 Arc 層抖動
4. 切到實際遊戲用的俯視角攝影機高度，確認能判斷電流的橫向位置與安全距離。
5. 在關卡內移動、旋轉該 Actor，確認 Local Space 設定下電流正確跟隨。
6. 檢查效能：Niagara Debugger 或 `stat GPU`，確認三層 Ribbon 的開銷在可接受範圍。

### 8.1 預覽呈現一團球、不是長條光束

症狀：Preview 內是一顆球或一片扁圓，不是兩點之間的線。成因是全部粒子生成在同一點，Ribbon 因此塌縮成一團。依下列順序排查。

**1. 第 2.1 節的版本相依性是否已修**

未修正時 `Spawn Beam` 無法正確讀取 `Beam Emitter Setup` 的輸出，所有粒子落在原點。這是最常見的成因，優先處理。

**2. User Parameter 是否真的接上**

在 **Parameters** 面板看 `BeamStart` / `BeamEnd` 右側的參考次數（格式為 `讀取|寫入`）。顯示 `0|0` 代表該參數沒有被任何模組使用，只是建了名字、沒有連到 `Beam Emitter Setup`，端點仍在用模組預設值。

對照：`SYSTEM Age` 之類的內建參數會顯示 `3|1`、`4|2` 這種非零值。接好之後 `BeamStart` / `BeamEnd` 的讀取次數必須大於 0。

**3. User Parameter 本身要有預設值**

User Parameter 建立時預設為 `(0, 0, 0)`。若兩個參數都保持預設，即使正確連上，兩個端點仍重合在原點，結果一樣是一團球。

在 Parameters 面板選中參數，於 Details 內填入預設值，例如 `BeamStart = (0, 0, 0)`、`BeamEnd = (500, 0, 0)`。

**建議的排查順序**：先不要接 User Parameter。直接在 `Emitter Update → Beam Emitter Setup` 上手填 `Beam Start = (0,0,0)`、`Beam End = (500,0,0)`，確認 Preview 出現一條沿 X 軸約 5 公尺的光束。形狀確認無誤後，再把這兩個輸入 Link 到 User Parameter 並補上預設值。這樣能區分「模組設定錯」與「參數沒接上」兩類問題。

### 8.2 Details 顯示 `0 Particles`、預覽空白

先看 Timeline 的播放頭位置，與各 Emitter 綠色區段的結束點比對。若播放頭已經越過綠色區段尾端，代表粒子都已死亡、而下一次生成還沒到，此時 `0 Particles` 是正常的時間點問題，不是設定錯誤。把播放頭拖回 `0.5` 附近即可確認。

成因：`Spawn Burst Instantaneous` 只在每個循環開頭生成一次，若粒子壽命短於 `Loop Duration`，循環後段就會出現空檔，表現為電流週期性斷掉。模板預設 `Loop Duration = 4.0`，對持續電流而言過長。

兩種處理方式：

**方式 A：讓壽命覆蓋整個循環**（改動小，建議先試）

- `Emitter Update → Emitter State → Loop Duration` 設為 `1.0`
- `Particle Spawn → Initialize Particle → Lifetime` 設為 `1.0`

每個循環舊粒子死亡時新粒子正好生成，中間無空檔。

**方式 B：粒子不死，靠 Update Beam 持續重排**

- `Particle Spawn → Initialize Particle → Lifetime Mode` 設為 `Infinite`
- `Emitter State → Loop Behavior` 設為 `Once`，避免每個循環重複累積粒子

Dynamic Beam 模板在 `Particle Update` 內含 `Update Beam` 模組，會每幀將粒子重新排列到兩端點之間，因此粒子不需要重生也能跟隨端點移動。此方式適合長時間常駐的電牆。

三個 Emitter 都要套用同一組設定，否則三層會不同步閃爍。

---

**本文件所列步驟與參數均未在 Unreal Editor 內實際執行或驗證**（代理環境無法執行 Editor 與編譯）。參數為建議起始值，非已驗證的玩法規則，需團隊按上述流程實測後回填實際值。

---

## 9. 已知限制與待確認事項

- 端點型別已於編輯器內確認為 **Position**（`Beam Emitter Setup` 3.0）。使用 Dynamic Beam 模板時必須先處理第 2.1 節的模組版本相依性，否則端點無法連接。
- Ribbon Renderer 強制 CPU Sim，後續若電流數量增多需評估 CPU 粒子開銷。火花、碎屑類 Sprite 可另開 GPU Sim 的 Emitter。
- [Build.cs](../../Source/HAL_Future_Creation/HAL_Future_Creation.Build.cs) **本次不需修改**。僅在編輯器內掛載 Niagara Component 時無需模組依賴；後續若從 C++ 持有 `UNiagaraComponent` 或呼叫 `UNiagaraFunctionLibrary` 設定參數，需將 `"Niagara"` 加入 `PublicDependencyModuleNames`。
- Niagara 插件在 UE5 預設啟用，`.uproject` 無需新增插件條目。
- 球碰到電流的規則未確認（穿過 / 阻擋 / 帶電屬性）。該決定影響碰撞體設定與爭球路線，屬於機關玩法階段，不影響本文件的特效製作。
- 柱子是否帶實體碰撞、以及「撞歪柱子導致電流中斷」是否納入玩法，均未確認。

---

## 10. 後續環節入口

特效定稿後的下一步，按 AGENTS.md 第 7 節的職責劃分：

1. 建立機關 Actor（C++），參照既有 [SpringPunchGimmick.h](../../Source/HAL_Future_Creation/SpringPunchGimmick.h) 的模式：伺服器權威階段結構 + 可複製狀態，支援 late join。
2. 狀態機 `Idle / Warning / Active / Cooldown` 在 C++，Niagara 僅接收狀態變化。
3. 判定使用細長 `UBoxComponent`，橫跨兩柱，**不隨粒子抖動**；判定寬度略窄於 Core 層視覺寬度（「看起來閃過卻被電到」比反向情況更難接受）。
4. 傷害走 `FVehicleHitContext` → `CombatResolver`，按 `EHALImpactTier` 結算。
5. 兩個 Scene Component 作為端點，位置餵給 `User.BeamStart` / `User.BeamEnd`。
6. 離散命中表現可走 Gameplay Cue；持續電弧由機關狀態切換呼叫 `Activate` / `Deactivate`。
