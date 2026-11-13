# 閃電新星特效 —— 參數契約、接線與驗收（繁體 v0.1）

2026-11-11 建立。接續 `main/b06ecae`（`b686e52` 加入 `NS_LightningNova`、`b06ecae` 接上
`BP_ElectricSphereGimmick`，兩者的 commit 說明都註明未驗證）。本機引擎 **UE 5.8.2-56702186**。

## 0. 本文件範圍

| 內容 | 來源 |
|---|---|
| 五個 User Parameter 的實際名稱與型別 | 本機自動化測試實讀 `NS_LightningNova` 的 `ExposedParameters` |
| `User.` 前綴與裸名是否等價 | 引擎原始碼 `NiagaraUserRedirectionParameterStore.h/.cpp` |
| 名稱／型別打錯時的行為 | 引擎原始碼 `NiagaraComponent.cpp`、`NiagaraParameterStore.h` |
| Niagara 的時間系統參數名 | 引擎原始碼 `NiagaraModule.cpp` |
| Python 能否編寫 emitter 堆疊 | 本機實測，結論與 [`FX_ShockArc_Niagara_Setup_CN.md`](../FX_ShockArc_Niagara_Setup_CN.md) 一致 |
| **實際遊戲畫面、PIE、雙端、效能** | **未驗證** |

## 1. 開工前的硬前置：重編譯

接續這個環節時，第一件事是**重編 Editor Target**，不是打開 Editor。

`AHALElectricSphereGimmick` 是 2026-11-10 才進入原始碼的新類別。若本機 binary 還是 pull 之前的版本，
Editor 會以下列警告載入失敗，而且**不會**出現紅色錯誤彈窗，很容易被當成特效沒做好：

```
LogUObjectGlobals: Warning: Failed to find object 'Class /Script/HAL_Future_Creation.HALElectricSphereGimmick'
CreateExport: Failed to load Class /Script/HAL_Future_Creation.HALElectricSphereGimmick as Parent
  for BlueprintGeneratedClass BP_ElectricSphereGimmick_C - both will fail to load
```

後果是 `BP_ElectricSphereGimmick` 的 generated class 為 `None`、Blueprint Status 為 `BS_UNKNOWN`，
`MVPB_NetTest` 與 `Test` 兩張圖裡擺好的那顆球也一起載入失敗。此時任何特效核對都沒有意義。

2026-11-11 本機實際遇到這個狀況：binary 是 11/02 的，`HALElectricSphereGimmick` 與
`HALExpandingSlipperyArea` 都不在 DLL 內。重編後即恢復。

## 2. 參數契約

`AHALElectricSphereGimmick` 以 `OnStageChanged(NewStage, StageAgeSeconds, StageDurationSeconds, RadiusCm)`
（`BlueprintImplementableEvent`，見 `HALElectricSphereGimmick.h`）把階段交給表現層，由 Blueprint 推進 Niagara。

`NS_LightningNova` 目前實際宣告的五個 User Parameter（2026-11-11 自動化測試實讀）：

| User Parameter | 實際型別 | 用途 |
|---|---|---|
| `User.Phase` | **Int32** | 0=Idle 1=Charge 2=Loop 3=Over，對應 `Stage + 1` |
| `User.PhaseElapsed` | Float | 進入當前階段後經過的秒數 |
| `User.Radius` | Float | cm，驅動各層尺寸 |
| `User.Intensity` | Float | **已宣告但尚未接到任何 emitter** |
| `User.Color` | Linear Color | 主色，與電牆同色系（見 2.4） |

Curve index = `Phase * 1000 + PhaseElapsed`，單一曲線同時編碼「哪個階段」與「階段內的形狀」；
間距 1000 使長 Loop 不會溢進下一個階段區段。

### 2.1 `User.Phase` 是 Int32，不是 Float

推這個參數必須用 **Set Niagara Variable (Int)**。用 Float 版本不會報錯，但也不會生效（見 2.3）。

### 2.2 `User.` 前綴與裸名等價

查 `FNiagaraUserRedirectionParameterStore`：它 override 的 `FindParameterOffset` 會把裸名 `Phase`
透過 `UserParameterRedirects` 重導到 `User.Phase`，而 `User.Phase` 本身也直接命中底層儲存名。
**兩種拼法都可用**，`b06ecae` commit 說明裡「`User.X` versus bare `X` 仍未確認」這一項可以結案。

### 2.3 真正的風險是靜默新增，不是前綴

`UNiagaraComponent::SetVariableFloat/Int` 兩條路徑最後都呼叫
`OverrideParameters.SetParameterValue(Value, VariableDesc, /*bAdd=*/true)`。
`bAdd=true` 的意思是：**名稱或型別對不上時不報錯，而是新增一個新參數**。
結果是沒有任何 emitter 讀它，特效安靜地不動，Log 乾淨，Niagara 編輯器預覽也看不出來。

這就是為什麼本環節加了一項自動化測試（見第 4 節）——這是不靠肉眼就能看見這類失敗的唯一位置。
同樣的風險也存在於球權磁力光束與車身受電弧光的參數契約，兩者的文件都已自行註明「錯一個就靜默失效」。

### 2.4 主色與電牆同色系（2026-11-14）

閃電新星與電牆造成的是同一種效果，顏色因此統一到同一色系。

電牆 `NS_ElectricArc` 的 `Core`／`ArcInner`／`ArcOuter` 三層 `Color` 模組同為
`(R=0, G=0.438499, B=1, A=1)`。`NS_LightningNova` 的 `User.Color` 預設值已由
`(0.35, 0.72, 1)` 改為同一個值。

這一個值足以帶動整個特效：七個 emitter 的 `InitializeParticle.Color` 全部 link 到
`User.Color`（已逐一實讀），`M_FX_NovaRing`／`M_FX_NovaSpokes`／`M_FX_NovaMicroArc`／
`M_FX_NovaShell` 都沒有顏色參數、只乘 Particle Color，`ChargeMotes` 與 `Sparks` 用的是引擎
預設 Sprite 材質。第 4 節說明 Blueprint 不推 `User.Color`，所以資產預設值就是實際主色。

唯一的例外是 `CoreSphere`：它的材質 `MI_FX_ToonNova_Core`（parent `M_FX_ToonSmoke_Mesh`）
有三個硬編碼的卡通分色，不跟 Particle Color 走，必須分開改。同一次變更後的值：

| 參數 | 舊值 | 新值 |
|---|---|---|
| `LitColor` | `(0.95, 1, 1)` | `(0.88, 0.96, 1)` |
| `MidColor` | `(0.38, 0.76, 1)` | `(0, 0.438499, 1)`（電牆同值） |
| `ShadowColor` | `(0.1, 0.3, 0.68)` | `(0, 0.298, 0.68)` |

**已驗證**：兩份資產改後讀回的參數值與上表一致，`MI_FX_ToonNova_Core` 縮圖渲染為電牆同色；
`HAL.FutureCreation.ElectricSphere` 與 `HAL.FutureCreation.ElectricWall` 合併複測 **12/12**
（NullRHI、單進程，0 測試警告／失敗／未執行，報告在 `Saved/ElectricSphere/20261114/NovaColor_R2`），
其中 `NovaFXParameterContract` 確認改後的 `NS_LightningNova` 仍可載入、五個參數名稱與型別不變。
**未驗證**：PIE 實際畫面、三階段、雙端、效能、Cook。Niagara 系統預覽只能在 `User.Phase=0`
的 Idle 狀態截圖、畫面為空，不能當驗收。驗收方式見第 8 節，重點是在同一個畫面裡同時看到電牆與
新星；若核心球在俯視下偏暗，先調 `MI_FX_ToonNova_Core` 的 `MidColor`／`LitColor`，不要回頭改
`User.Color`，否則會再度與電牆分家。

## 3. 為什麼 Niagara 與 Blueprint 由團隊手動施作

AGENTS.md 第 8.3 節規定代碼代理不得直接修改 Blueprint 資產或 Event Graph。
Niagara 這側另有技術限制，2026-11-11 在本機重新確認：

- 不存在 `unreal.NiagaraEditorLibrary`。
- `UNiagaraSystem` 的 `ExposedParameters` 在 Python 是 protected，讀不到。
- UE 5.8 新增的 `NiagaraExternalSystemEditorUtilities`（`NiagaraExt_*`）**只有 USTRUCT 進了 Python，
  函式沒有 `UFUNCTION` 標記**，因此 Python 無法呼叫，不能用來讀寫 emitter 堆疊。
- `UBlueprint` 的 `UbergraphPages`、`SimpleConstructionScript` 在 Python 同樣是 protected；
  `EdGraph` 的 `Nodes` 也是。

所以 emitter 堆疊、曲線與 Event Graph 的實際編修一律是 Editor 手動步驟。
C++ 這側則可以、也應該用自動化測試把契約鎖住。

## 4. 存檔 Blueprint 的實際接線（2026-11-11 核對）

透過 `Exporter.run_asset_export_task` 把 `BP_ElectricSphereGimmick` 匯出為 T3D 逐節點核對，
存檔內容與 `b06ecae` commit 說明**一致**，沒有 guard、沒有 Branch：

**On Stage Changed**

```
GetGameTimeInSeconds ──┐
                       Subtract_DoubleDouble ──→ Set PhaseStartTime
StageAgeSeconds ───────┘                              │
                                                      ▼
NovaFX → SetVariableInt   ("User.Phase",        Conv_ByteToInt(NewStage) + 1)
      → SetVariableFloat  ("User.PhaseElapsed", StageAgeSeconds)
      → SetVariableFloat  ("User.Radius",       RadiusCm)
      → Activate()
```

**Event Tick**

```
NovaFX → SetVariableFloat("User.PhaseElapsed", GetGameTimeInSeconds − PhaseStartTime)
```

四個推送節點的名稱字串與型別全部正確：

| 節點 | 函式 | 名稱字串 | 對應宣告型別 | 相符 |
|---|---|---|---|---|
| `K2Node_CallFunction_7` | `SetVariableInt` | `User.Phase` | Int32 | ✅ |
| `K2Node_CallFunction_10` | `SetVariableFloat` | `User.PhaseElapsed` | Float | ✅ |
| `K2Node_CallFunction_12` | `SetVariableFloat` | `User.Radius` | Float | ✅ |
| `K2Node_CallFunction_9`（Tick） | `SetVariableFloat` | `User.PhaseElapsed` | Float | ✅ |

`User.Color` 與 `User.Intensity` 目前沒有任何推送節點，維持資產預設值。

因此 `b06ecae` 說明中「`User.X` 或裸名未確認」這項已結案：**用的是 `User.` 形式，而且是正確的。**

## 5. 自動化測試

新增 `HAL.FutureCreation.ElectricSphere.NovaFXParameterContract`
（位於既有的 `Source/HAL_Future_Creation/Tests/HALElectricWallTests.cpp`，沿用該檔既有的世界夾具）。

鎖住的內容：

1. 存檔的 `BP_ElectricSphereGimmick` 與 `NS_LightningNova` 都能載入（這一項會直接抓到第 1 節的未重編狀況）。
2. `NS_LightningNova` 宣告了契約中的五個名稱。
3. Blueprint 掛的 Niagara 元件確實指向 `NS_LightningNova`，且 Auto Activate 為關閉。
4. **元件 override store 內不存在任何資產未宣告、或型別與宣告不符的參數。**
   這是第 2.3 節那個靜默失效的機械化偵測：推錯名稱或型別會在 store 內多出一筆，這裡就會失敗。
5. 夾具自檢：一次等價的直接推送必須能被讀回，否則下面觀測到的值是在量測測試世界而不是 Blueprint。

第 1～5 項全部通過。

## 6. 夾具的能力邊界（已由團隊 PIE 結案，不是缺陷）

> **2026-11-11 團隊回報：PIE 內確認三個階段都有。** 推送在真實遊戲世界中確實送達，
> 本節描述的是自動化夾具的限制，不是接線缺陷。第 8 節的驗收走向因此確定為「夾具限制」一路。
> 此為團隊實際畫面觀察，非本機量測結果。

自動化世界**無法**驅動這個 Blueprint 的事件本體，因此不能用它驗證推送的值。證據是單一且明確的：

> 在 Loop 階段轉換之後，世界時鐘已經走到 **0.15 秒**，而 Blueprint 自己的 `PhaseStartTime`
> 仍然是**精確的 0**。`On Stage Changed` 的第一個節點就是
> `Set PhaseStartTime = GetGameTimeInSeconds − StageAgeSeconds`，若本體執行過，這個值必為非零。

所以 store 內 `User.Phase` 全程 0、`User.Radius` 全程 400（皆為資產預設）只說明一件事：
**Blueprint 事件沒有被執行**，與名稱、型別、前綴或 `Activate()` 都無關。

過程中逐一排除的其他解釋，一併留檔避免後人重跑：

- **延後佇列**：`SystemInstanceController` 在推送前後都無效（實測），`SetVariable*` 走的是立即寫入
  `OverrideParameters` 的路徑，不是 `SetVariable_Deferred` 的 world manager 佇列。
- **`Activate()` 重置參數**：`CopyParametersFromAsset` 的三個呼叫點都在 `WITH_EDITOR` 內，
  綁在 `PostApplyToComponent` 與 `SetAsset`／`SynchronizeWithSourceSystem`，不在 `ActivateInternal` 路徑上。
- **名稱或型別打錯**：第 4 節逐節點核對，四個推送全部正確。
- **元件抓錯**：該 Actor 上只有一個 Niagara 元件（實測 count=1，名稱 `NovaFX`，資產 `NS_LightningNova`）。
- **Blueprint 沒有覆寫事件**：`OnStageChanged` 在 generated class 上有 96 bytes bytecode；
  T3D 匯出顯示 exec 鏈完整。

結論：**接線沒有缺陷，且已由團隊在 PIE 內確認三個階段都有**。
值的驗證無法在這個夾具內做，測試因此不斷言這些值，只鎖住名稱、型別與「沒有多出未宣告參數」。

這個限制不只影響本環節：任何「原生事件 → Blueprint 表現」的接線，在這套世界夾具裡都驗不到值。
若之後要讓自動化涵蓋這類推送，需要先解決夾具無法執行 Blueprint 事件本體的問題，屬獨立課題。

## 7. 移除 Event Tick（團隊在 Editor 執行）

AGENTS.md 第 13.7 節要求能由事件／狀態變更驅動時就不新增 Tick。目前 `User.PhaseElapsed` 由
`Event Tick` 每幀推一次，可以改成只在階段轉換時推一次相位起點，讓 Niagara 自己算經過時間。

> **不是只把那段接線刪掉。** `Event Tick` 目前是階段**內**時間唯一的推進來源：
> `On Stage Changed` 只在轉換當下推一次 `StageAgeSeconds`（通常是 0），之後就不再更新。
> 只刪 Tick 會讓 `User.PhaseElapsed` 凍在那個值，曲線停在每個階段的第一幀，三段都變成靜止畫面。
> 必須先讓 Niagara 自己算時間（步驟 1～3），再動 Blueprint（步驟 4～6）。順序反了會把現在能動的特效弄壞。

引擎提供的時間系統參數已查證（`NiagaraModule.cpp`）：

| Niagara 參數 | 型別 | 語意 |
|---|---|---|
| `Engine.Time` | Float | 自關卡開始遊玩的秒數；**會**隨暫停停止、**會**受 time dilation 影響 |
| `Engine.RealTime` | Float | 同上但不受暫停與 dilation 影響 |
| `Engine.System.Age` | Float | 自該 system 建立起的秒數 |

`Engine.Time` 與 Blueprint 的 `Get Game Time in Seconds`（兩者都是 `World->GetTimeSeconds()`）
同一個時間基準，暫停與 dilation 行為一致，所以可以直接相減。

### 7.1 步驟

**Niagara（`NS_LightningNova`）**

1. 在 User Parameters 新增 **`PhaseStartTime`**，型別 **Float**。
2. 每個目前讀 `User.PhaseElapsed` 的位置，改讀 `Engine.Time − User.PhaseStartTime`。
   曲線索引維持 `Phase * 1000 + (Engine.Time − User.PhaseStartTime)`。
3. 兩種參數並存一段時間比較畫面，確認無誤後再移除 `User.PhaseElapsed`。

**Blueprint（`BP_ElectricSphereGimmick`）**

4. `On Stage Changed` 內把
   `SetVariableFloat("User.PhaseElapsed", StageAgeSeconds)`
   改成
   `SetVariableFloat("User.PhaseStartTime", GetGameTimeInSeconds − StageAgeSeconds)`
   —— 這個值與同一條鏈上已經算好的 `PhaseStartTime` 完全相同，可直接接那個節點的輸出。
5. 刪除 `Event Tick` 節點與它後面那個 `SetVariableFloat`。
6. 若 Actor 的 Tick 不再有其他用途，在 Class Defaults 關閉 **Start with Tick Enabled**。

**測試**

7. 同一次變更要把 `HALNovaFX::Contract`（`HALElectricWallTests.cpp`）的名單一起改掉，
   否則第 5 節第 2 項會因為找不到 `User.PhaseElapsed` 而失敗 —— 這正是這個名單存在的目的。

### 7.2 取捨

- `Engine.Time` 是 float32。在長時間不重啟的 Editor session 中，`Engine.Time − User.PhaseStartTime`
  的解析度約為 `Engine.Time × 2⁻²³`：一小時約 0.4 毫秒，一天約 10 毫秒。對 5 分鐘的單局完全足夠。
- 另一個更簡單的做法是改用 `Engine.System.Age` 並在每次階段轉換時 `Activate(reset)`，
  如此連 `PhaseStartTime` 都不必推。代價是**失去中途進入的能力**：
  晚加入的客戶端會從該階段的頭開始重播，而 `b06ecae` 原本刻意避免這件事。不建議改。

## 8. 團隊 PIE 驗收步驟（決定第 6 節走向）

1. 重編 Editor Target（第 1 節），開啟 `Content/Maps/Test`。
2. 選取場景中的 `BP_ElectricSphereGimmick`，Class Defaults → Electric Wall → Debug → 勾選 **Draw Debug**。
3. Play In Editor。對照紅（Begin）→ 藍（Loop）→ 紅（Over）的 debug 範圍，確認特效三段有變化。
4. 開 **Window → Niagara → Niagara Debugger**，附加到該元件，直接看 User Parameters 面板：
   `Phase` 應依序為 1 → 2 → 3，`Radius` 應等於 Sphere Config 的 Radius（預設 300），
   `PhaseElapsed` 應在每段內從 0 開始遞增。
5. 若面板上的值正確但畫面沒有對應變化 → 問題在 emitter 的曲線或材質，不在接線。
6. 若面板上的值停在 `Phase=0`、`Radius=400` → 與第 6 節的夾具觀測一致，才是真缺陷，回報後再查。
7. 兩端（Listen Server + 1 client）各做一次，另測晚加入：客戶端在 Loop 中途才相關時，
   應直接進入 Loop 的中段，不重播 Begin。

## 9. 兩個已知缺口

### 9.1 `User.Intensity` 已宣告但未接線

目前沒有任何 emitter 讀它，也沒有任何 Blueprint 節點推它。兩個選擇，二擇一並更新第 2 節表格：

- **接起來**：在各層的顏色或尺寸上乘上 `User.Intensity`，並在 `On Stage Changed` 加一個
  `SetVariableFloat("User.Intensity", …)`。
- **移除**：從 User Parameters 刪掉，同時從 `HALNovaFX::Contract` 名單移除。

保留一個沒人讀也沒人寫的參數會讓第 2.3 節的靜默失效更難察覺，建議擇一處理，不要擱置。

### 9.2 `ArcSpokes` 從上方被遮住

> **已於 2026-11-14 失效（見第 12.5 節）**：`ArcSpokes` 已停用，放射效果改由 `M_FX_NovaShell`
> 的細絲項繪製，本節的調整建議不再適用，保留作為歷史記錄。

`b686e52` 自己記錄了這一項：俯視角下 ArcSpokes 夾在核心球與地面環之間，幾乎看不到。
本專案相機為俯視（`ATestVehiclePawn` 為 `FRotator(-60, 0, 0)`），所以垂直方向可見度有限、
水平方向才是有效畫面空間。

資產內確實存在的可調旋鈕（已從序列化資料核對名稱）：

- `ArcSpokes.InitializeParticle.Position Offset` —— 把放射臂抬離地面環。
- `ArcSpokes.ScaleMeshSize.Scale Factor`、`ArcSpokes.VectorFromCurve001.Scale Curve` —— 讓放射臂
  在水平方向延伸超過地面環半徑，從上方才看得到。
- `ArcSpokes.SpawnRate.SpawnRate` —— 數量。

本文件不預設哪一個是正解；這一項需要實際畫面才能判斷，屬第 8 節之後的調整。

## 10. 本輪驗證與限制

本機 **UE 5.8.2-56702186**，Editor Target 編譯通過（警告僅既有 `HALCameraTests.cpp` 的 C4305）。

`HAL.FutureCreation.ElectricSphere` 與 `HAL.FutureCreation.ElectricWall` 合併複測
**12/12 通過，0 測試警告／失敗／未執行**：球體 3 項（`FastCrossing`、`LifecycleAndContact` 為既有回歸，
`NovaFXParameterContract` 為本輪新增）與電牆既有 9 項回歸。
NullRHI、單進程。報告與日誌在 `Saved/ElectricSphere/20261111/NovaFX_R3/`；
過程中的診斷輪次留在同目錄的 `NovaFX`、`NovaFX_Diag`～`NovaFX_Diag3`、`NovaFX_R2`。

**未驗證**：PIE、實際畫面、雙端、晚加入、效能、Cook／Shipping。
本輪未修改任何 `.uasset`、`.umap` 或 Config；第 7、9 節的變更一律由團隊在 Editor 執行。

## 11. 三階段視覺規格（2026-11-14）

團隊回報「整個新星看不出明顯的三階段」。本節把三段各自該讓玩家讀到什麼寫死，並記錄本輪
在 `NS_LightningNova` 內實際做的改動。玩法階段定義見
[球形電擊區域：三階段生命週期](../Combat/Electric_Sphere_CN_v0.1.md)，本節只談表現。

### 11.1 三段要讀到的訊息

| 階段 | `User.Phase` | 玩法 | 玩家必須讀到 |
|---|---|---|---|
| Begin（區域提示） | 1 | 預設 1 秒，**不電擊** | 「這塊圓形地面即將通電」——地面環畫出半徑，中心只在蓄力 |
| Loop（生效） | 2 | 預設 5 秒，接觸回彈／眩暈 | 「整顆球正在通電、現在碰到會中」——球殼滿徑、放射臂全開 |
| Over（收束） | 3 | 預設 1 秒，電場已關 | 「正在消失、已經安全」——核心爆閃後全部向中心塌縮歸零 |

色相三段一致，都是電牆的 `(0, 0.438499, 1)`（見 2.4 節）。**不要用換色來區分階段**，
區分靠尺寸、層次與密度；換色會讓新星與電牆分家。

### 11.2 曲線索引由 `Phase * 1000` 改為 `Phase * 10`

原本的索引是 `Phase * 1000 + PhaseElapsed`，第一版曲線照這個刻度編，結果三段幾乎看不出差別。
原因不在曲線值，在 Niagara 曲線 Data Interface 的 **LUT 取樣**：

> 曲線 DI 預設 `bUseLUT = true`，以 **128 格** LUT 覆蓋「第一個 Key 到最後一個 Key」的整個區間。
> 索引用 1000 間距時，區間是 0～3002，**每格約 23.5 個單位**。階段內的變化只有 0～2 秒，
> 整段都塞在一格以內，於是三段各自取到的值幾乎只由鄰近那一兩格決定，次秒級的形狀全被抹平。

把各 emitter 內 `MakeFloatFromInt(User.Phase) × B` 的 `B` 由 `1000` 改為 `10`，曲線 Key 全部改
編在 **9.8～32**：Idle 0、Begin 10～12、Loop 20～22、Over 30～32。區間 22.2 ÷ 128 ≈ **0.17 單位／格**，
0.25 秒級的動作才畫得出來。

兩點附帶結果：

- `User.Phase = 0`（Idle）的索引 0 低於第一個 Key（9.8，值 0），曲線 clamp 成 0，三段以外完全不出現。
- 間距 10 而 `PhaseElapsed` 由 Blueprint clamp 在 2，仍有 8 秒餘裕。即使日後照第 7 節解掉 clamp、
  讓 Loop 的 5 秒完整推進，也不會溢進下一段。**`B` 與曲線 Key 必須同時改，只改一邊會讓該層永遠取到 0。**

`User.Phase`／`User.PhaseElapsed` 的推送契約沒有變，Blueprint 與 `HALNovaFX::Contract` 都不受影響。

### 11.3 重新啟用 `GroundRing`

`7648c5d` 停用了 `GroundRing`、改用 `FieldShell` 球殼。俯視角下球殼與放射臂會糊成一團，
沒有任何一層把「範圍」畫在地面上，區域提示因此讀不出來。本輪重新啟用 `GroundRing`，
並讓它成為 Begin 段的主角。它的半徑來自 `User.Radius × 0.025`，材質 `M_FX_NovaRing` 的
`RingRadius = 0.8`，所以曲線值 1.0 時環正好落在 `User.Radius` 上，可以當作危險邊界來讀。

### 11.4 各層曲線（現行值）

值是 `ScaleMeshSize`／`ScaleSpriteSize` 的倍率，1.0 = 該層的設計尺寸
（mesh 層的設計尺寸 = `User.Radius ×` 下表的 k）。

| 層 | k | Idle | Begin 10→11 | Loop 20→22 | Over 30→31 |
|---|---|---|---|---|---|
| `GroundRing` | 0.025 | 0 | 0.06 →1.06→ 1.0（擴張後定住） | 1.0 | 1.0 → 0.5 → 0 |
| `FieldShell` | 0.02 | 0 | 0.18 → 0.4（薄殼蓄力） | 1.0（±0.03 呼吸） | 1.0 → 0.6 → 0 |
| `CoreSphere` | 0.009 | 0 | 0.1 → 0.5 | 0.7↔0.62 | **1.25 爆閃** → 0.35 → 0 |
| `ChargeMotes` | 精靈 14 | 0 | 1.1 → 0.9（最密，向內吸） | 0.45 | 0.2 → 0 |
| `Sparks` | 精靈 9 | 0 | 0.15 → 0.2 | 1.0 | 0.8 → 0.3 → 0 |
| `MicroArcs` | 0.012 | 0 | 0.1 → 0.18 | 1.0 | 0.6 → 0.25 → 0 |
| `ArcSpokes` | 0.024 | 0 | **0** | 0.15 →0.62（0.25 秒彈出） | 0.58 → 0.3 → 0 |

`ArcSpokes` 的 Loop 值刻意壓在 0.62：k=0.024 代表曲線 1.0 時放射臂伸到 1.2 倍半徑，會蓋過
`GroundRing` 畫出來的危險邊界（AGENTS.md 第 12 節）。0.62 讓臂尖留在球內。

### 11.5 工具上的兩個坑（接手前先看）

1. **Monolith 的 `preview_system` 會拿到舊畫面。** 改完曲線／倍率後，即使 `request_compile`
   回報 `UpToDate`、`save_system` 也存了，預覽仍可能持續算舊的編譯結果。本輪曾因此誤判
   「寫入沒生效」而來回改了好幾輪。**資產內容要用 `get_dynamic_input_value` 之類的讀取動作確認，
   畫面要在重開 Editor 之後或 PIE 內看**，不要用預覽截圖當驗收。
2. **`EditorLoadingAndSavingUtils.reload_packages` 會跳對話框卡住 Editor 主執行緒**，
   Monolith 會整個連不上。要重載資產請直接重開 Editor。

### 11.6 本輪驗證與未驗證

**已驗證**：七個 emitter 的倍率讀回皆為 `10.0`；存檔後的 `.uasset` 內可找到各曲線的特徵 Key
時間（9.8／10.5／20.25／21.6／30.25／30.6 等）與電牆色 `0.438499`；`User.Phase`／`PhaseElapsed`
預設已還原為 0，七層全部啟用。

**Editor 重開後的三段截圖（2026-11-14，俯視、同一組相機設定）**：

| 取樣點 | 畫面 | 檔案 |
|---|---|---|
| Begin `Phase=1, Elapsed=0.6` | 地面環撐滿半徑、charge motes 向中心落、中央只有小蓄力球 | `Saved/Screenshots/Monolith/20261114_045840_NS_LightningNova.png` |
| Loop `Phase=2, Elapsed=0.6` | 地面環維持滿徑，環內整片被電弧填滿 | `…_045851_…png` |
| Over `Phase=3, Elapsed=0.2` | 仍是滿徑，核心爆閃 | `…_045910_…png` |
| Over `Phase=3, Elapsed=0.8` | 地面環已消失，只剩收縮中的核心與散開的火花 | `…_045920_…png` |

三段在俯視角下可明確區分，Over 在 1 秒內收乾淨。

> **截圖時務必注意相機框**：`preview_system` 的相機是固定距離，實測可見範圍只有約 **440 cm 寬**。
> 以預設 `User.Radius = 400` 拍，Loop 的球殼與地面環都在畫面外，看起來會像「沒有變化」——
> 本輪一度因此誤判。截圖時把 `User.Radius` 暫時設成 **110** 左右，拍完再改回 400。

**仍未驗證**：PIE 實際遊戲畫面與手感、雙端、晚加入、效能、Cook。請照第 8 節在
`Content/Maps/Test` 跑一次 PIE 確認。

---

## 12. 外觀改版：等離子球造型（2026-11-14）

### 12.1 目標與來源

團隊提供一張等離子球參考圖，要求：**最外圍有向內漸淡的範圍提示；中間由發光體散出細電弧；
細電弧纏繞在最外圍的半透遮罩上**。並補充三階段的對應：

| 階段 | 必須看到 |
|---|---|
| Begin（提示） | **只有範圍提示與核心**，不出現電弧 |
| Loop（生效） | 參考圖的完整樣貌 |
| Over（收束） | **範圍顯示與電弧一起收進核心內縮小消失** |

> 團隊已明確指出：漸層是**外緣最亮、向核心漸淡**，所以不存在模糊危險邊界的問題。
> 這一點推翻了改版前的疑慮，`M_FX_NovaShell` 因此以 `pow(r, RimFalloff)` 實作——
> `r` 為投影半徑，邊界處為 1、中心為 0。地面環 `GroundRing` 仍保留為硬邊界。

### 12.2 `M_FX_NovaShell` 重寫

改版前是「少數粗硬邊大圓閃電 + 細剪影環」的卡通殼；改版後同一個 Custom HLSL 節點輸出四項相加：

| 項 | 作用 | 關鍵參數 |
|---|---|---|
| `rim` | 外緣最亮、向核心漸淡的範圍提示 | `FillGlow`、`RimFalloff` |
| `edge` | 邊界上那條銳利亮線 | `RingStart`、`RingBoost` |
| `core` | 殼自身帶的中心柔光 | `CoreGlow`、`CorePower` |
| `gc` | 纏繞球面的大圓電弧（沿用舊迴圈，調細） | `Bolt*`、`ArcBoost` |
| `fil` | **新增**：由核心向外放射的細絲 | `Filament*` |

細絲的實作要點：

- **以視線軸對齊**。用 `Parameters.CameraVector` 建基底，取像素繞視線的方位角與投影半徑 `r`。
  對俯視角遊戲是優點（永遠正面朝玩家）；代價是從任何角度看都一樣，過場鏡頭會看得出貼片感。
- **用角度域重複，不是用迴圈加條數**。`frac(angle * FilamentCount)` 讓 34 條與 6 條的成本幾乎一樣。
  若改成 `for` 迴圈逐條畫，條數會直接變成每像素成本——這是這一層唯一的效能地雷。
- **路徑是折線，不是正弦波**（2026-11-14 修訂，見第 12.8 節）。把 `r` 切成 `FilamentKinks` 段，
  每段端點取一個雜湊值，段內用 **線性內插**（不是 `smoothstep`）——所以每一節是直線、節點處是硬轉折。
  用正弦疊加會得到平滑曲線，看起來像觸鬚而不是閃電。
- 線寬 `FilamentThickness * FilamentCount / r`，所以細絲在核心處收斂、往外維持等寬。
- `FilamentInner` 在核心周圍留一圈空隙，細絲從那裡才開始，避免蓋住核心。
- 只掃左右各一個鄰格（`j = -1..1`），避免角度環繞接縫處斷線。

### 12.3 新資產 `M_FX_NovaCore`

`CoreSphere` 原本用 `MI_FX_ToonNova_Core`（父材質 `M_FX_ToonSmoke_Mesh`，三階色卡通煙）。
那個材質的設計目的就是製造明顯色塊分界，與參考圖柔和外擴的白熾球本質衝突，調參數救不回來，
因此新建 `M_FX_NovaCore`（Additive／Unlit／雙面，已開 `used_with_niagara_meshes`）：

```
body = pow(1 - r, CoreFalloff)        // 主色球體
hot  = pow(1 - r, CoreFalloff * 5)    // 白熾中心（不乘主色）
halo = pow(1 - r * 0.85, …) * HaloAmount
```

`CoreSphere` 實際指向的是 `MI_FX_NovaCore`（`M_FX_NovaCore` 的實例），數值調整都在實例上，
不需要重編母材質。`MI_FX_ToonNova_Core` 本身未刪除，仍停留在第 2.4 節的電牆色，可隨時切回比較。

### 12.4 三階段怎麼分：粒子 Alpha 當閘門

Begin 需要「殼在滿徑、但電弧不出現」，而殼的縮放曲線管不到這件事，所以多一個通道：

```
FieldShell / particle_update / Color
  Color       ← User.Color
  Scale Alpha ← FloatFromCurve(CurveIndex = User.Phase * 10 + User.PhaseElapsed)
```

材質把 `ParticleColor` 的 **A** 接到 `Amt`，只乘在 `gc` 與 `fil` 上；`rim`／`edge`／`core` 不受影響。
Additive 混合不使用 alpha，所以這個通道是安全的。

| 索引 | 9.8–12（Begin） | 20.3–22（Loop） | 30–31（Over） |
|---|---|---|---|
| `Scale Alpha` | 0（無電弧） | 1 | 1 → 0（31.0 歸零） |

Over 的「收進核心」由縮放曲線完成，不需要額外機制：

| 層 | Begin 10→12 | Loop 20→22 | Over 30→31 |
|---|---|---|---|
| `FieldShell` | 0 →1.08→ 1.0（彈出到滿徑） | 1.0 | 1.0 → 0.35 → **0（31.05）** |
| `CoreSphere` | 0.08 → 0.34 | 0.36 ↔ 0.44 | 0.44 → **0.95 爆閃** → 0 |

`GroundRing` 沿用第 11.4 節的曲線，三段都在，Over 時一起收掉。

### 12.5 關掉的兩層

`ArcSpokes` 與 `MicroArcs` 已 `set_emitter_enabled = false`：放射與纏繞都改由殼層材質畫，
這兩層在新造型裡沒有存在意義，關掉剛好抵銷殼層變複雜的開銷。第 9.2 節「ArcSpokes 從上方被遮住」
因此成為歷史條目——該層已停用，不需要再調它的 `Position Offset` 或 `Scale Curve`。
資產未刪除，要回退只要重新啟用並把 `MI_FX_NovaShell` 的 `FilamentBoost` 設為 0。

### 12.6 現行參數值

`MI_FX_NovaShell`（`M_FX_NovaShell` 的實例）：

| 參數 | 值 | 參數 | 值 |
|---|---|---|---|
| `FillGlow` | 0.28 | `FilamentCount` | 22 |
| `RimFalloff` | 3.0 | `FilamentThickness` | 0.0075 |
| `RingStart` | 0.965 | `FilamentJitter` | 0.45 |
| `RingBoost` | 1.5 | `FilamentBoost` | 1.25 |
| `CoreGlow` | 0.22 | `FilamentDropout` | 0.3 |
| `CorePower` | 6.0 | `FilamentKinks` | 9 |
| `ArcBoost` | 1.1 | `FilamentInner` | 0.3 |
| `BoltCount` | 5 | `BoltThickness` | 0.018 |
| `BoltSegments` | 7 | `BoltAmplitude` | 0.07 |
| `BoltSpan` | 1.25 | `FlickerRate` | 14 |
| `BoltDropout` | 0.35 | | |

`MI_FX_NovaCore`（`M_FX_NovaCore` 的實例）：`CoreGlow` 1.6／`CoreFalloff` 2.6／`HotCenter` 3.5／`HaloAmount` 0.35。

### 12.7 本輪驗證

**已驗證**：`M_FX_NovaShell`、`M_FX_NovaCore` 編譯無錯誤（`build_material_graph` 回報
`has_errors: false`，材質屬性 Additive／Unlit／雙面／Niagara Mesh 皆保留）；`validate_system`
無 error（七筆 `MeshScale/SpriteSize` warning 為既有誤報，改版前即存在）；三段俯視截圖如下
（`User.Radius` 暫設 110，拍完已還原 400）：

| 取樣點 | 畫面 | 檔案 |
|---|---|---|
| Begin `Phase=1, Elapsed=0.6` | 外圍亮環向內漸淡、charge motes、白熾核心，**無電弧** | `Saved/Screenshots/Monolith/20261114_NovaLook_Begin.png` |
| Loop `Phase=2, Elapsed=1.3` | 參考圖造型：細絲由核心放射至邊界，外圍亮環包覆 | `…_NovaLook_Loop.png` |
| Over `Phase=3, Elapsed=0.55` | 範圍與電弧一起縮小，火花向外散開 | `…_NovaLook_Over055.png` |
| Over `Phase=3, Elapsed=0.9` | 只剩一個小點，1 秒內歸零 | `…_NovaLook_Over090.png` |

**仍未驗證**：PIE 實際遊戲畫面（場地亮度下 additive 藍的對比、細線在遠距離的 TAA 閃爍）、
雙端、效能、Cook。細絲若在遠距離抖動，先調 `FilamentThickness`（加粗）或降 `FilamentCount`。

> **不要用 Monolith 在開著的 Editor 裡跑 `run_automation_tests`**：本輪這樣做讓 Editor 直接
> assert 崩潰（`Assertion failed: LatentCommands.IsEmpty()`，`AutomationTest.cpp:1370`）。
> 資產在崩潰前已存檔，沒有損失，但自動化測試請一律用 `UnrealEditor-Cmd.exe … -nullrhi` 另開行程跑。

### 12.8 折線修訂與核心可見度（2026-11-14 第二輪）

團隊回饋兩點：**細絲沒有明確的折線、太像觸鬚**，以及**細絲出現後中心核心變得不太可見**。

**折線**：原本的中心偏移是兩個 `sin()` 疊加，數學上就是平滑曲線，不管振幅調多大都只會變成更誇張的
觸鬚。改成分段線性雜訊——`sr = r * FilamentKinks * (0.80 + hash * 0.55) + hash * 5.0`，
取 `floor(sr)` 與其下一個節點的雜湊值，段內 `lerp`。每段是直線、節點是硬轉折，才讀得出閃電。
每條細絲的節點間距與相位都乘上自己的雜湊，否則所有細絲會在同一個半徑同時轉折、形成同心圓紋路。

**核心可見度**：三處一起改，缺一個都不夠。

| 做法 | 參數 | 值 |
|---|---|---|
| 細絲在核心周圍留空隙，從空隙外緣才開始 | `FilamentInner` | 0.3（`smoothstep(0.3, 0.52, r)`） |
| 殼層自身的中心柔光收窄，不要填滿空隙 | `CoreGlow` / `CorePower` | 0.5→**0.22** / 4.5→**6.0** |
| 核心球本體加大、白熾中心加亮 | `CoreSphere` Loop 縮放 / `MI_FX_NovaCore.HotCenter` | 0.44→**0.58** / 2.5→**3.5** |

同時把細絲數量由 34 降到 **22**、線稍微加粗到 0.0075：34 條在這個半徑下會互相疊成一片亮面，
反而讓折線和核心都讀不出來。`FillGlow` 由 0.55 降到 0.28，讓盤面中段暗下來、細絲才跳得出來。

**已驗證**：材質重建 `has_errors: false`；三段截圖重拍（檔名同第 12.7 節，已覆蓋）——Loop 的細絲
有明確硬轉折、白熾核心清楚可辨，Begin 仍只有範圍提示與核心、無電弧，Over 仍在 1 秒內收乾淨；
`ElectricSphere` + `ElectricWall` **12/12**，報告在 `Saved/ElectricSphere/20261114/NovaLook_R2`。
**仍未驗證**：PIE 實際畫面、雙端、效能、Cook。

### 12.9 盤面降亮與細絲收細（2026-11-14 第三輪）

本輪由特效評審比對四張截圖與參考圖後提出兩項必改，兩項都動了。

**必改 1：盤面填充降亮，並掛上階段閘門。**
評審量到 Loop 在 `r≈0.2–0.4` 的平均亮度約 150–170/255，Begin 約 95–160；疊到中亮度場地底色上模擬
additive 相加後，盤面幾乎整片逼近飽和——實機 8 公尺直徑的圓盤會把開進去的車洗白，違反 AGENTS.md
第 12 節「不得遮擋碰撞方向、球軌跡與玩家車輛姿態」。而且 Begin 與 Loop 的盤面亮度幾乎一樣，
Over 也只是「縮小的 Loop」、讀不到「要熄了」。

兩處一起改：

| 位置 | 改動 |
|---|---|
| `MI_FX_NovaShell.FillGlow` | 0.28 → **0.15** |
| `M_FX_NovaShell` 的 `rim` 行 | `pow(r, RimF) * Fill` → `pow(r, RimF) * Fill * (0.40 + 0.60 * saturate(Amt))` |

`Amt`（`ParticleColor.A`）本來就是 Begin 0 ／ Loop 1 ／ Over 1→0 的現成閘門（見 12.4 節），
原本只乘在 `gc` 與 `fil` 上。把 `rim` 也掛上去，一次解掉兩件事：Begin 的盤面只剩 Loop 的 40%，
Over 收束時亮度跟著掉。**`edge`、`core` 與 `GroundRing` 刻意不掛閘門**——危險邊界三段都必須一樣硬，
把它跟著調暗會讓「半徑到哪」變模糊。

**必改 2：細絲線寬減半。**
`hwf = FilT * FilN / r`，而 `ts` 的單位是一格角度格距（`qf = frac(q) - 0.5`，範圍 ±0.5）。
以 `FilT = 0.0075`、`FilN = 22`，在 `r = 0.5` 時 `hwf ≈ 0.33`——單條細絲就吃掉整個格距的 **66%**，
相鄰細絲必然互相逼近糊成亮面，12.8 做出來的硬轉折被吃掉。12.8 把條數由 34 降到 22 是對的方向，
但同時「稍微加粗」到 0.0075 又把問題塞了回去，只解了一半。

| 參數 | 改動 | 效果 |
|---|---|---|
| `FilamentThickness` | 0.0075 → **0.0030** | `r=0.5` 時佔格距 ~26%（實心部分 ~15%） |
| `FilamentBoost` | 1.25 → **1.55** | 補回變細損失的亮度 |

條數維持 22 不動。**細絲若在實機遠距離抖動（TAA），優先降 `FilamentCount` 到 14 左右，
不要回頭加粗 `FilamentThickness`**——加粗就會再次走回糊成亮面這條路。

**現行值修正（取代 12.6 節該三列）**：`FillGlow` 0.15／`FilamentThickness` 0.0030／`FilamentBoost` 1.55。

**已驗證**：`recompile_material` 無錯誤，`validate_material` 僅剩既有的
`translucent_no_opacity` warning（Additive 不取 opacity，改版前即存在）；四張截圖以同一組
相機設定（`camera_angle = top`、512×512、`User.Radius` 暫設 110）重拍覆蓋，逐張確認——
Loop 的細絲已是細硬折線、中段盤面暗下來、白熾核心清楚；Begin 盤面明顯比 Loop 暗，白環邊界不變；
Over 0.55 比 Loop 同時更小**也更暗**，halo 已消失；Over 0.9 仍在 1 秒內歸零。
拍完 `User.Radius` 已還原 400、`Phase` 0、`PhaseElapsed` 0 並存檔（已讀回確認）。

**仍未驗證**：PIE 實際畫面——盤面降亮後在真實場地亮度下是否還讀得到範圍、細絲在實機相機距離
是否抖動，兩者都只有在 PIE 裡才驗得掉。建議下輪補一張**車輛開進 Loop 區域**的 PIE 截圖。

**待整併的文件衝突**：12.4 節表格寫 `CoreSphere` Loop 縮放 0.36↔0.44，12.8 節又寫改成 0.58，
兩處不一致。下次動這層時以資產實際曲線為準統一一處，避免有人照錯的表回退。

### 12.10 Over 段收束修訂（2026-11-14 第四輪）

#### 先更正 12.9 節的一句話

12.9 節寫「Over 0.55 比 Loop 同時更小**也更暗**」。**只有前半是對的。** 複評以徑向亮度剖面
實測：環峰值 236.5 對 Loop 239.8、細絲峰值 233.8 對 232.0、同正規化半徑的盤面中位 Over 104
對 Loop 82（Over 還略亮）。消失的只有外圍 bloom，本體一點都沒變暗。**附加 bloom 消失很容易被
誤讀成整體變暗，用肉眼比對兩張 additive 截圖不可靠，要量剖面。**

#### 這輪改了什麼

必改項兩條，病根同一個：Over 只讀得到「變小」，讀不到「已經安全」。

**改動 1：`FieldShell` 的 `Scale Alpha` 曲線，Over 段提早起跌。**
原本接近 31.0 才掉，所以 0.55 秒時電弧與盤面仍是滿功率。整條曲線重寫（linear）：

| 索引 | 9.8 | 12.0 | 20.0 | 22.0 | 30.0 | 30.25 | 30.6 | 31.0 |
|---|---|---|---|---|---|---|---|---|
| 值 | 0 | 0 | 1.0 | 1.0 | 1.0 | 0.55 | 0.22 | 0 |

12→20 之間是一段長斜坡，但索引永遠不會落在那裡（Begin 的 `PhaseElapsed` 由 Blueprint clamp 在 2，
最大 12.0；Loop 最小 20.0），所以不影響任何階段。**刻意不在 19.9 補一個 0 的硬轉折**——那會把
整段升降塞進一個 LUT 格（11.2 節），索引 20.0 反而只取到約 0.5。

**改動 2：`GroundRing` 的縮放曲線，Over 段快速淡出。**
> ⚠️ **2026-11-14 第六輪更正：這項改動其實無效。** `GroundRing` 的 `ScaleMeshSize` 曲線
> 根本不會影響畫面（整條設 0 白環依然在），而且那條硬白環是殼層材質的 `edge` 項、不是
> `GroundRing`。雙同心環消失只能歸功於同一輪的改動 1。證據與機制見第 12.12 節。
> 以下文字保留原樣作為紀錄，**但它對畫面的因果解釋是錯的**。
原因是 `GroundRing`（Over 1.0→0.5→0）收得比 `FieldShell` 快，兩層脫鉤，Over 0.55 時畫面出現
**兩道等亮的同心硬環**（實測徑向中位在 r=66 與 r=92 各一個峰，而 Begin／Loop 都只有單峰），
讀起來像「場在重新構型」。整條曲線重寫（linear，x=y=z）：

| 索引 | 9.8 | 10.0 | 10.5 | 11.0 | 12.0 | 20.0 | 22.0 | 30.0 | 30.3 | 31.0 |
|---|---|---|---|---|---|---|---|---|---|---|
| 值 | 0 | 0.06 | 1.06 | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 | 0 | 0 |

Begin 的 0.06→1.06→1.0 擴張彈跳沿用 11.4 節；只有 Over 段由「1.0→0.5→0」改成 30.3 就歸零。
場已經關掉的 Over 不需要再畫範圍提示，全程只留**一條**邊界線。

> **這兩條曲線是整條重寫的。** `export_system_spec` 與 `get_dynamic_input_value` 都讀不回曲線 key，
> 所以 Begin／Loop 段是照 11.4／12.4 的表重建、再用截圖逐張確認回來的。上表就是現在資產裡的
> 實際 key，之後要改請以上表為準，不要再重新推測。

#### `FieldShell.Scale Color`：讓 Over 時整顆殼一起沉下去

材質回傳 `(arcs + rim + edge + core) * PColor`，而 `edge`（那條硬白環）與 `core` 刻意不吃 `Amt`
閘門——因為 Begin 的 `Amt` 是 0，掛上去會讓 Begin 失去危險邊界。結果是**場已經關掉的 Over，
那條邊界環仍維持 100% 亮度**。`Amt` 一個通道無法區分 Begin 與 Over（兩者都往 0 走），所以需要
第二個通道：`Color` 模組的 `Scale Color`。

已接好的線（節點 GUID 以本次為準）：

```
FieldShell / particle_update / Color / Scale Color
  └ VectorFromCurve001           04F51A7148AF184F29B95DAE8414FA3A
      ├ VectorCurve  = 9.8:1 / 22:1 / 30:1 / 30.25:0.45 / 30.6:0.15 / 31:0  (linear, x=y=z)
      └ CurveIndex   = Add_Float002            C8B3CBB0454D053634415CA7A6B2DF19
            ├ A = Multiply_Float003            A743E6A34DB426DC3825809303FDEA75
            │     ├ A = MakeFloatFromInt002    C4543B754A263F01D71326A2FEC848B7  ← User.Phase
            │     └ B = 10.0
            └ B ← User.PhaseElapsed
```

結構與現有的 `Scale Alpha` 完全同形，`get_dynamic_input_tree` 已讀回確認。
**重開 Editor 後已驗證生效，數據見下方「本輪驗證」。**

> **新發現，比這個改動本身重要：曲線 DI 的「資料」改動會即時進到 `preview_system`，
> 但「改圖」（新增 dynamic input）不會。**
> 證據：同一輪內，改 `Scale Alpha` 與 `GroundRing` 的曲線 key 立刻反映在截圖上；
> 新增 `Scale Color` 的整條 dynamic input 鏈之後，把曲線**整條壓成 0.3**（全階段、全通道）
> 重拍，像素與改動前**逐格相同**（環峰值 226.6 兩次完全一致）。此時
> `get_system_diagnostics` 回報 FieldShell 的 Particle Update Script 為 `UpToDate`、op_count 76，
> 也就是**資產確實編譯好了，是預覽的系統實例還在跑舊腳本**。
> 這是 11.5 節第 1 點的精確版本：資產編譯狀態與預覽畫面是兩回事。
> **要驗證任何改圖，必須重開 Editor 或進 PIE。** 本輪重開 Editor 後，同一組參數、同一個取樣點
> 拍出來的 Over 0.55 殼層亮度由 u0.4:66 掉到 u0.4:28，證實改圖本身是對的，先前看不到純粹是
> 預覽實例沒更新。
>
> **重開 Editor 後還有一個坑**：著色器尚未編譯完時 `preview_system` 會拍出**全黑**的圖
> （本輪前兩張就是，`get_system_diagnostics` 當時一切正常）。拍攝前先等
> `ShaderCompileWorker` 行程歸零，不要把全黑圖當成「特效壞了」。

#### 本輪驗證與未驗證

**已驗證**（四張截圖以 `camera_angle=top`、512×512、`User.Radius` 暫設 110 重拍覆蓋，並量徑向剖面）：

| 取樣點 | 結果 |
|---|---|
| Begin `1 / 0.6` | 與上輪**逐格一致**（峰值半徑 126、峰值 243.0、總亮度 18845→18927）——`GroundRing` 曲線重寫與 `Scale Color` 都沒動到 Begin，危險邊界維持原樣 |
| Loop `2 / 1.3` | 與上輪**逐格一致**（峰值半徑 123、峰值 243.0，剖面 125/113/121/140 對 132/116/117/138，在雜訊內）——`Scale Alpha` 與 `Scale Color` 在索引 21.3 都確實是 1.0，沒有被意外調暗 |
| Over `3 / 0.55` | **雙同心環已消失**，只剩一條邊界；殼層本體相對 Loop 的同正規化半徑降到約 **26%**（u0.4 28 對 113、u0.6 31 對 121、u0.8 40 對 140）；環峰值 243→200.3 |
| Over `3 / 0.9` | 峰值 107.8→48.7、總亮度 2360→1736，只剩一個小核心與極淡的環，1 秒內歸零 |

> **環峰值不是好指標。** Over 0.55 的環峰值只從 243 掉到 200，但環兩側的帶狀區域掉了約 74%。
> 原因是 additive 高光在 sRGB 編碼下已接近飽和，線性亮度砍掉四分之三也只反映成一點點差異。
> **判斷「有沒有變暗」要看環兩側的剖面，不要看峰值。**

`validate_system` 無 error（七筆 `MeshScale/SpriteSize` warning 為既有誤報；`MicroArcs`／`ArcSpokes`
的 `DataMissing` 是 12.5 節停用那兩層的正常結果）。拍完 `User.Radius` 已還原 400、`Phase` 0、
`PhaseElapsed` 0，存檔後讀回確認。

**未驗證**：
1. PIE 實際畫面：盤面降亮後是否還讀得到範圍、細絲在實機相機距離是否抖動（若抖動，先把
   `FilamentCount` 22→14，**不要**加粗 `FilamentThickness`）、車輛開進 Loop 區域的遮擋。
2. 自動化測試（`ElectricSphere` + `ElectricWall`）本輪未跑。
3. 雙端、效能、Cook。

#### 評審指出、尚未處理的項目

- 外圍剪影的纏繞電弧不足（參考圖最具辨識度的特徵）：`BoltCount` 5→8、`BoltDropout` 0.35→0.20、
  `BoltSpan` 1.25→1.8，並把 HLSL 內 `az` 的分佈往 ±0.5 以外偏，讓大圓弧貼著剪影跑。
- `Sparks` 會飛出危險半徑（Loop 時在 1.25–1.36×R 仍有亮度 130 的火花）。目前地面環夠硬不至於
  誤導，但若之後邊界對比下降，應把生成／速度半徑夾在 1.0×R 以內。
- `FieldShell` 的 `local_space=false`，其餘六個 emitter 都是 `true`。目前電擊區靜止所以看不出來，
  但掛到會移動的載體上時只有球殼不會跟著走。
- 12.4／12.8 的 `CoreSphere` Loop 縮放數值衝突（0.36↔0.44 對 0.58）仍未整併。

### 12.11 外圍纏繞電弧（2026-11-14 第五輪）

補上 12.1 節團隊要求的第三項、也是評審排第一的參考圖差距：**細電弧纏繞在最外圍的半透遮罩上**。
改版後這一層（`gc`）一直存在，但在俯視角下幾乎讀不到，邊界看起來只是一條乾淨亮環。

#### 兩個把它卡住的原因（都不是參數值的問題）

**1. `BoltCount` 的實際上限是 6，不是參數面板上的任意值。**
迴圈寫死成 `for (int k = 0; k < 6; k++)`，再用 `if ((float)k >= n) break;` 提早結束。
把 `BoltCount` 設成 8 不會多出任何一條。本輪把迴圈上限改成 **10**，並在程式碼內加註解；
`n` 也改成 `clamp(Count, 1.0, 10.0)`，避免下次有人設 20 卻只拿到 10 條還查不出原因。
**迴圈上限與 `BoltCount` 必須一起改。**

**2. `az` 的分佈有一半是重複的，而且大多數大圓根本不貼邊。**
`az` 是大圓**平面法線** `ax` 的 Z 分量。俯視角下：

| `az` | 大圓在畫面上的樣子 |
|---|---|
| →±1 | 平面接近赤道面，投影貼著剪影跑——這才是「纏繞」 |
| →0 | 平面通過兩極，投影是一條橫過盤面的直線 |

原本是 `az = (2*(fk+0.5)/n - 1) + jitter`，以 `n=5` 展開為 −0.8／−0.4／0／0.4／0.8：
中間三條橫過盤面，只有兩條稍微貼邊。更關鍵的是——**大圓由平面決定，`ax` 與 `−ax` 是同一個圓**，
所以正負是成對重複的，有效條數再打對折。

改成只取正值、在貼邊帶內鋪開：

```hlsl
float t01 = (fk + 0.5) / n;
float az  = clamp(0.735 + 0.21 * t01 + (h1 - 0.5) * 0.14, 0.60, 0.96);
```

`0.735…0.945` 對應大圓平面相對赤道傾斜約 **19°–43°**，每一條都在外圈繞行——正好補在放射細絲
（`fil`，由核心往外）留下的空檔上。方位角沿用既有的黃金角 `fk * 2.39996323 + h2 * 1.4`，
所以傾斜方向彼此錯開、不會疊成同一條。

> `0.735` / `0.21` / `0.14` 是**結構常數不是手感旋鈕**——它們決定「這一層畫在哪裡」，
> 與既有的 `0.30 + 0.70 * r`、`2.39996323` 同性質，因此沒有升成材質參數。
> 手感要調請用下表四個參數。若哪天要讓大圓重新橫過盤面，把下限 `0.735` 往 0 降即可。

#### 參數改動

| 參數 | 舊 | 新 | 作用 |
|---|---|---|---|
| `BoltCount` | 5 | **8** | 條數（受迴圈上限 10 限制） |
| `BoltDropout` | 0.35 | **0.20** | 每個 flicker 間隔被丟棄的比例，愈低同時在場的愈多 |
| `BoltSpan` | 1.25 | **1.8** | 單條弧沿大圓的弧長（弧度），1.8 ≈ 103° |
| `ArcBoost` | 1.1 | **1.4** | 這一層的亮度倍率 |

`BoltThickness` 0.018、`BoltSegments` 7、`BoltAmplitude` 0.07、`FlickerRate` 14 未動。

#### 本輪驗證

**已驗證**（四張截圖以 `camera_angle=top`、512×512、`User.Radius` 暫設 110 重拍覆蓋）：

| 取樣點 | 結果 |
|---|---|
| Begin `1 / 0.6` | **仍然完全沒有電弧**——`Amt` 閘門擋住了，纏繞弧沒有漏進提示階段 |
| Loop `2 / 1.3` | 外圈出現數條沿剪影繞行的弧，與放射細絲交織，造型明顯接近參考圖 |
| Over `3 / 0.55` | 弧與細絲一起淡掉，仍讀得到「正在熄滅」 |
| Over `3 / 0.9` | 仍在 1 秒內歸零 |

**邊界可讀性（AGENTS.md 第 12 節）**：量徑向剖面，環峰值 243.0→243.2（不變），
環內側低點 139.8→150.8（被弧提亮），**對比 103.3→92.4**。約降一成，但仍是清楚的硬階梯，
危險半徑讀得出來。`recompile_material` 無錯誤，`validate_material` 僅剩既有的
`translucent_no_opacity` warning。拍完 `User.Radius` 已還原 400、`Phase` 0、`PhaseElapsed` 0，
存檔後讀回確認，`list_dirty_packages` 為 0。

**自動化測試**：`ElectricSphere` + `ElectricWall` **12/12 通過**，0 失敗／0 警告／0 未執行，
報告在 `Saved/ElectricSphere/20261114/NovaLook_R5`（`index.json` 的 `reportCreatedOn`
為 `2026.11.13-21.33.35` UTC，即本地 06:33，不是舊報告）。其中
`ElectricSphere.NovaFXParameterContract` 通過，等於由測試獨立確認了 `User` 參數契約沒有被
這幾輪的截圖操作破壞——不只是我讀回參數而已。用另開的 `UnrealEditor-Cmd.exe … -nullrhi`
行程跑，沒有動到開著的 Editor；本輪只改資產、未改 C++，不需要重編譯（兩個模組 DLL 的建置時間
晚於 `Source/` 與 `Config/` 下所有檔案）。

**仍未驗證**：
1. **效能**。大圓迴圈由實際執行 5 次變成 8 次（+60%），且 `BoltDropout` 降低讓 `continue` 提早
   跳出的機會變少，`BoltSpan` 加大也讓更多像素進入線寬計算。這是本輪唯一的效能風險，
   請在 PIE 用 `stat GPU` 看 `M_FX_NovaShell` 的成本；若要退，優先降 `BoltCount` 回 6。
   **自動化測試涵蓋不到這一項**——它跑在 `-nullrhi` 下，根本沒有繪製。
2. PIE 實際畫面：弧在實機相機距離是否抖動、盤面降亮後範圍是否還讀得到、車輛開進 Loop 區域的遮擋。
3. 雙端、Cook。

#### 評審指出、仍未處理的項目

- `Sparks` 會飛出危險半徑（Loop 時在 1.25–1.36×R 仍有亮度 130 的火花）。本輪邊界對比降了一成，
  這項的優先度相對上升。
- `FieldShell` 的 `local_space=false`，其餘六個 emitter 都是 `true`。
- 12.4／12.8 的 `CoreSphere` Loop 縮放數值衝突（0.36↔0.44 對 0.58）仍未整併。

### 12.12 核心放大與細絲由核心長出（2026-11-14 第六輪）

團隊回饋兩句：**中間可視的核心太小**、**希望電弧看起來像由核心散發出去**。這兩件事是同一個
決定的後果——12.8 節為了讓核心看得見，用 `FilamentInner = 0.3` 在核心外圍挖了一圈空隙、
細絲從空隙外緣才開始。那圈「護城河」正是讓電弧看起來與核心斷開的原因。本輪把它反過來做：
**核心本體放大，細絲改成從核心內部長出來**。

#### 改動

| 位置 | 參數 | 舊 | 新 | 作用 |
|---|---|---|---|---|
| `MI_FX_NovaShell` | `FilamentInner` | 0.30 | **0.12** | 取消護城河，細絲淡入區間改為 `smoothstep(0.12, 0.34, r)`，起點落在核心輝光**內部** |
| `CoreSphere` spawn | `Mesh Uniform Scale` 的 `B` | 0.009 | **0.020** | 核心網格半徑（`User.Radius × B`） |
| `MI_FX_NovaCore` | `CoreFalloff` | 2.6 | **1.0** | 衰減壓平，輝光鋪滿網格而不是集中在正中心 |
| `MI_FX_NovaCore` | `CoreGlow` | 1.6 | **2.4** | 本體亮度，配合壓平的衰減讓中段也到飽和 |
| `MI_FX_NovaCore` | `HaloAmount` | 0.35 | **0.5** | 外暈外推，填掉原本護城河的位置 |

`HotCenter` 維持 3.5——`hot = pow(1 - r, CoreFalloff * 5)`，`CoreFalloff` 由 2.6 降到 1.0
已經把白熾指數從 13 降到 5，白熾區自然變寬，再加亮會過曝。

**量測結果**（Loop、`User.Radius = 110`、球半徑 123px）：

| 指標 | 改動前 | 改動後 |
|---|---|---|
| 白熾核心（luma > 220）半徑 | 7 px | **13 px** |
| 核心輝光（luma > 128）半徑 | 22 px | **35 px** |
| 輝光 ÷ 球半徑 | 18% | **28%** |
| 邊界環峰值 | 243.2 | 242.4（不變） |

邊界可讀性沒有受影響。細絲現在收束進核心輝光內部，讀起來是由核心長出去的。

---

#### 本輪最重要的發現：**`ScaleMeshSize` 的曲線是死的**

查「核心為什麼放不大」時發現的，**比造型本身重要，而且推翻了 12.10 節的一個結論**。

**證據**（都在同一個取樣點 Loop `2 / 1.3`、同一組相機設定下拍的）：

| 測試 | 預期 | 實際 |
|---|---|---|
| `CoreSphere` 縮放曲線 Loop 設 **4.0** | 核心放大 4 倍 | 與 0.82 幾乎無差（d30: 141 對 102，僅輝光略增） |
| `CoreSphere` 縮放曲線 **整條設 0** | 核心完全消失 | **與設 4.0 的剖面幾乎逐格相同**（d00:255 d05:246 d10:225） |
| `MI_FX_NovaCore` 三個參數**整組設 0** | 核心消失 | **確實消失**（d00 由 255 掉到 222、d05 由 246 掉到 163） |
| `GroundRing` 縮放曲線 **整條設 0** | Begin 的白環消失 | **白環still在** |
| `MI_FX_NovaShell.RingBoost` 設 0 | — | **白環消失** |

**兩個結論：**

1. **`CoreSphere` 與 `GroundRing` 的 `ScaleMeshSize` 曲線完全沒有作用。** 實際決定網格大小的是
   `InitializeParticle` 的 `Mesh Uniform Scale`（spawn 階段，`User.Radius × k`），那個是活的
   （本輪把 `k` 由 0.009 改到 0.020 有效果，只是因為 additive 飽和而遞減）。
   `ScaleMeshSize` 寫的是 `Particles.MeshScale`，但 mesh renderer 的 `ScaleBinding` 綁的是
   `Particles.Scale`——這正是 `validate_system` 那七筆一直被當成「既有誤報」的
   `Particles.MeshScale` warning 在講的事。**那不是誤報。**
2. **那條硬白邊界環是殼層材質的 `edge` 項（`RingBoost`），不是 `GroundRing`。**

**因此 12.10 節的「改動 2」是錯的**：那一節寫「把 `GroundRing` 的 Over 段改成 30.3 就歸零，
修掉了雙同心環」。`GroundRing` 的曲線根本不會動，雙同心環消失**只能**歸功於同一輪的
`FieldShell / Scale Alpha` 改動。12.10 的改動 1 有效、改動 2 無效——該節的曲線表仍是資產現況，
但**不要相信它對畫面的因果解釋**。

**還有兩件事因此其實從來沒發生過**：12.4 節寫的 `CoreSphere` Over 段「0.95 爆閃」、
以及各層的三階段尺寸動畫中凡是靠 `ScaleMeshSize` 的部分。`FieldShell` 是例外——Over 時球確實
變小（實測環半徑 Loop 123px、Over 0.55 為 92px），所以它那條路徑是通的，三者的差異還沒查清楚。

> **12.4／12.8 的 `CoreSphere` 數值衝突（0.36↔0.44 對 0.58）到此可以結案：兩個都沒有生效過。**
> 現行曲線已整理成下表並寫進資產，但在 `ScaleMeshSize` 修好前它不影響畫面：
> `9.8:0 / 10:0.1 / 12:0.5 / 20:1.0 / 22:1.0 / 30:1.0 / 30.25:1.5 / 30.6:0.6 / 31:0`

#### 本輪引入的回歸（尚未修）

核心尺寸現在**三階段恆定**（本來就是，只是 `k` 放大 2.2 倍之後明顯了）。
最明顯的是 **Over `3 / 0.9`：殼層已經收乾淨，核心卻還是一顆不會縮小的亮球懸在那裡**，
與 12.1 節「全部向中心塌縮歸零」不符。

**正確的修法**：照 `FieldShell` 的模式，給 `CoreSphere` 加一個 `Color` 模組，
`Scale Alpha` 與 `Scale Color` 各掛一條以 `User.Phase * 10 + User.PhaseElapsed` 為索引的曲線
（結構見 12.10 節的接線圖）。**Color 模組的曲線是確定有效的**，這是目前唯一驗證過能讓
`CoreSphere` 隨階段變化的通道。那是改圖，**必須重開 Editor 才驗得掉**（見 12.10 節）。

#### 本輪驗證

**已驗證**：四張截圖以 `camera_angle=top`、512×512、`User.Radius` 暫設 110 重拍覆蓋；
Begin 仍無電弧、邊界環不變；Loop 的核心與細絲如上表；Over 0.55 仍讀得到收束。
拍完 `User.Radius` 已還原 400、`Phase` 0、`PhaseElapsed` 0，存檔後讀回確認，
`list_dirty_packages` 為 0。
**未驗證**：Over 0.9 的核心回歸尚未修；PIE 實際畫面、效能、雙端、Cook；本輪未跑自動化測試。

---

### 12.13 核心的階段閘門：補掉 Over 段殘留的亮球（2026-11-14 第七輪）

本輪只處理 12.12 節末尾自己記下的那個回歸：**Over `3 / 0.9` 時殼層已經收乾淨，
核心卻還是一顆不會縮小、也不會變暗的亮球懸在那裡。** 兩個改動，一個在材質、一個在圖。

#### 改動 1：`M_FX_NovaCore` 加上粒子 alpha 閘門

先確認機制，不是猜的。`MaterialExpressionCustom_0` 原本的末段是：

```hlsl
float3 c = PColor * (body * Glow + halo);
c += hot * Hot;          // 白熾中心
return c;
```

**`c += hot * Hot` 刻意不乘 `PColor`**——這是 12.3 節當初為了讓中心保持白熾（不被藍色染掉）
而做的設計。代價是：`Color` 模組的 `Scale Color` 通道走的正是 `PColor`，
所以**它永遠調不暗那顆白心**。12.12 節看到的「亮球不肯消失」就是這一條。

改法是加第七個輸入 `Amt`，接 `MaterialExpressionParticleColor_0` 的 **`A` 輸出（output index 4）**，
在最後統一閘一次：

```hlsl
// 白熾中心刻意不被 PColor 染色，也因此 Scale Color 調不暗它。
// 改用粒子 alpha 當閘門——和殼層同一個手法。additive 不讀 alpha，這個通道是免費的。
c += hot * Hot;
return c * saturate(Amt);
```

這跟 12.4 節殼層用的是同一招：**additive blend 忽略 alpha，所以 `ParticleColor.A`
是一個不花額外成本、逐粒子的全域乘數。** 殼層早就這樣用了，核心本來漏掉。

> `update_custom_hlsl_node` 的 `inputs` 陣列是**整組取代**，送出之後七個輸入腳位會全部斷線，
> 必須逐一重接再 `get_expression_connections` 核對。本輪七個都確認 `connected: true` 才編譯。
> `Amt` 沒接的話讀到 0，整顆核心會直接變黑——這個腳位漏接不會報錯，只會畫面全黑。

`validate_material` 只剩既有的 `translucent_no_opacity` warning（additive 不用 opacity，預期之內）。

#### 改動 2：`CoreSphere` 加 `Color` 模組與階段曲線

照 12.12 節寫下的「正確的修法」做，也就是繞開壞掉的 `ScaleMeshSize` 通道：

```
CoreSphere / particle_update / Color            CD262E0648E3414732DE74873977E970
  ├─ Color        ──▶ 綁 User.Color（保住藍色；Color 模組每幀覆寫 Particles.Color）
  └─ Scale Alpha  ──▶ FloatFromCurve            867D84984C79C53BCB2D46BE0F18BFCE
                        ├─ FloatCurve  = 下表（linear）
                        └─ CurveIndex  ──▶ Add_Float002      7CE14CA94F91F223883D439AFF64BF95
                                             ├─ A ──▶ Multiply_Float003   42CFCAAA4926BEA252008C850FEA7E84
                                             │          ├─ A ──▶ MakeFloatFromInt002  511CC42640C0B89E71B95E874CEAD78F
                                             │          │          └─ Int = 綁 User.Phase
                                             │          └─ B = 10.0
                                             └─ B = 綁 User.PhaseElapsed
```

曲線鍵（linear）：

| index | 9.8 | 10 | 12 | 20 | 22 | 30 | 30.25 | 30.55 | 30.9 | 31 |
|---|---|---|---|---|---|---|---|---|---|---|
| Scale Alpha | 0 | 0.30 | 0.85 | 1.0 | 1.0 | 1.0 | 1.0 | 0.45 | 0.12 | 0 |

Idle 關閉、Begin 充能、Loop 滿值、Over 收束到 0。`Color` 輸入一定要綁 `User.Color`：
`Color` 模組在 `particle_update` 每幀覆寫 `Particles.Color`，不綁就會把 `InitializeParticle`
給的藍色洗成模組預設值。

> **30.25 這個鍵刻意是 1.0 而不是爆閃值。** 材質端寫的是 `saturate(Amt)`，
> 大於 1 會被夾掉，曲線填 1.25 只會變成一個騙人的數字。Over 段的爆閃由殼層負責。

#### 兩個設定上的陷阱

1. **`set_dynamic_input_value` 設不出參數綁定。** 對 `MakeFloatFromInt002.Int` 送
   `value: "User.Phase"`，它會回報 `Set input 'Int' = 'User.Phase'` **而且成功**，
   但 `get_dynamic_input_tree` 顯示 `source: "literal", value: "User.Phase"`——
   那是一個字串字面值，不是綁定，整條索引鏈會算成 0。
   要綁定必須改用 **`set_module_input_binding`**，`module_node` 填那個 dynamic input 的 GUID。
   接完一定要用 `get_dynamic_input_tree` 核對，看到的必須是 `source: "linked"`。
2. **重開 Editor 之後，頭幾張 `preview_system` 可能是全黑的，即使 `ShaderCompileWorker` 已經是 0。**
   本輪 Begin 連拍兩張全黑，接著改 `User.Phase` 拍 Loop 正常，再回頭拍 Begin 就正常了。
   這比 11.5 節原本寫的「等編譯」更精確：**等進程數歸零還不夠，要先丟一次參數變更＋重拍**，
   不要看到黑畫面就以為改壞了。

#### 本輪驗證

**已驗證**：`M_FX_NovaCore` 七個輸入逐一核對 `connected: true`、重編譯、`validate_material`
只剩既有 warning、已存檔；`NS_LightningNova` `request_compile` + `validate_system`
`valid: true`、警告仍是既有那七筆 `Particles.MeshScale`（見 12.12 節，那些不是誤報）、已存檔。
重開 Editor（圖的改動不重開驗不掉，見 12.10 節），等 `ShaderCompileWorker` 歸零並連續四次確認後，
四張截圖以 `camera_angle=top`、512×512、`User.Radius` 暫設 110 重拍覆蓋：

- **Over 0.9 的回歸已修掉**：原本的亮球變成一個很暗的小點加上稀疏火花，與殼層一起收束。
- **Over 0.55 讀得到塌縮**：殼層縮小變暗，核心仍亮（曲線 0.45 vs 殼層約 0.27），
  讀起來是「殼層向核心收進去」。
- **Begin 沒有被誤調暗**：充能光點、小而亮的核心、邊界環都在。
- **Loop 沒有被誤調暗**：核心飽滿，細絲由核心向外放射。

拍完 `User.Radius` 已還原 400、`Phase` 0、`PhaseElapsed` 0，存檔後 `list_dirty_packages` 為 0。

**未驗證**：`ScaleMeshSize` → `Particles.Scale` 的綁定問題仍然存在（本輪是繞開，不是修好）；
PIE 實際畫面、效能、雙端、Cook。

---

### 12.14 核心的飽和平台與剪影硬圓（2026-11-14 第八輪）

上一輪的評審只給了一項【必改】，但它底下有兩個症狀，病根是同一個：第六輪為了「把核心放大」
把 `CoreFalloff` 設成 **1.0**。`body = pow(1 - r, fo)` 在 `fo = 1` 時是**線性**的，
乘上 `CoreGlow 2.4` 之後藍通道在核心網格七成半徑以內全部 clip——核心不是柔光球，
是一顆**硬邊的飽和白盤**。

兩個症狀、兩組量測（球半徑 123px）：

| 症狀 | 改前 | 門檻 | 改後 |
|---|---|---|---|
| 飽和平台過大 | 中位 luma ≥220 一路到 r=44（0.36×R）；參考圖只到 0.25×R | ≤31px | **29px** |
| 剪影處硬階梯 | 相鄰 1px 環帶 G 落差 **117**（Over055 更達 163） | <40 | **26** |
| 核心可讀半徑（不得縮水） | luma ≥128 到 r=55 | 50–58px | **51px** |

為什麼這是必改而不是美術潔癖：12.9 節當初把 `FillGlow` 由 0.28 砍到 0.15，理由是
「盤面 150–170 疊到場地底色會洗白開進去的車」。改前的盤心是 **209–255**，比那個被否決的狀態
更嚴重；換算 `User.Radius = 400` 就是**一塊直徑約 350cm 的近飽和白盤壓在區域正中心**，
車或球穿過中心時讀不到姿態與軌跡，違反 AGENTS.md 第 12 節。而 Over 0.55 那道硬圓落在 0.45×R，
是危險區裡的**第二條同心硬邊**——正是 12.10 節花一整輪修掉的失敗模式。

#### 改動 1：`MI_FX_NovaCore` 三個參數

| 參數 | 改前 | 改後 |
|---|---|---|
| `CoreFalloff` | 1.0 | **2.0** |
| `CoreGlow` | 2.4 | **1.8** |
| `HaloAmount` | 0.5 | **0.45** |

> **`HaloAmount` 沒有照評審建議降到 0.30。** 實測 0.30 會讓 luma ≥128 的半徑掉到 **48px**，
> 低於同一份評審自己訂的 50–58 下限——也就是把「核心太小」的老問題換個方式帶回來。
> 兩個門檻必須同時看，只盯其中一個會來回擺盪。

**沒有回頭縮小核心網格。** 第六輪「中間可視的核心太小」的團隊回饋仍然有效，
這一輪動的是亮度分佈的形狀，不是尺寸。

#### 改動 2：`M_FX_NovaCore` 的 halo 項強制在剪影前歸零

光調 `CoreFalloff` 解不掉硬階梯，因為階梯不是 `body` 畫的，是 `halo` 畫的：

```hlsl
float halo = pow(saturate(1.0 - r * 0.85), fo * 0.35) * Halo;
```

`r` 是視線正交化之後的球面半徑，**`r = 1` 正好是網格剪影**。但 `pow(1 - 1×0.85, ...)`
= `pow(0.15, 0.7)` ≈ **0.26**——halo 在剪影處根本沒有歸零，additive 把這個殘值
畫成一條貼著核心剪影的硬圓。加一個收尾窗：

```hlsl
// The halo must reach zero BEFORE the mesh silhouette, not at it ...
// Start the fade late (0.86) so the readable glow keeps its radius; the window is
// still wide enough that the step stays well under the 40/255 budget.
float halo = pow(saturate(1.0 - r * 0.85), fo * 0.35) * Halo * (1.0 - smoothstep(0.86, 1.0, r));
```

> **起點 0.86 是量出來的，不是挑好看的。** 先試 0.78：階差降到 17（門檻 <40，很寬鬆），
> 但 luma ≥128 的半徑掉到 48px，**低於下限**——淡出開始得太早，把可讀的外圈一起吃掉了。
> 改成 0.86 後階差 26、可讀半徑 51px，兩項同時進窗。
> 這個節點只改 `code`、不送 `inputs`，所以七個腳位不會斷線（`updated_fields` 只有 `code`、
> `input_count` 仍是 7）；即使如此仍重新核對過一次連線才編譯。

#### 本輪驗證

**已驗證**（材質參數與 HLSL 都是**資料／編譯層**的改動，不是 Niagara 改圖，
所以不必重開 Editor 就會進 `preview_system`——這點與 12.10 節的限制不同，
12.11 節改殼層 HLSL 時已經確認過一次）：

- `recompile_material` 無錯誤、`validate_material` 只剩既有的 `translucent_no_opacity`
  warning、`M_FX_NovaCore` 與 `MI_FX_NovaCore` 都已存檔。
- 四張截圖重拍覆蓋。三項門檻全部達標（數字見上表）。
- **硬邊界環完全沒動**：環峰值 Begin **242.7** / Loop **242.4**，位置 r=123 / 122，
  與 12.11 節記錄的 243.0 / 243.2 在量測誤差內。
- **Over 段仍然收乾淨**：Over055 環峰 202.2 @ r=92（球確實縮小）；
  Over090 核心 luma ≥220 只到 r=1、≥128 只到 r=4。
- Begin 的白心由 ≥220 r=14 縮到 **r=9**，但 ≥128 的範圍由 19px 擴到 **28px**——
  這是 falloff 由線性改成平方的預期結果：中心沒那麼死白，暈開的範圍反而更大。

拍完 `User.Radius` 已還原 400、`Phase` 0、`PhaseElapsed` 0，`list_dirty_packages` 為 0。
**全程未提交。**

**未驗證**：PIE 實際畫面、效能、雙端、Cook；本輪未跑自動化測試（只改資產、未改 C++）。
`ScaleMeshSize` → `Particles.Scale` 的綁定問題仍在（12.12 節），本輪沒有碰。

#### 評審留下、本輪刻意沒做的事

- **`User.Color.A` 變成核心的靜默相依。** 新閘門的實際值是 `User.Color.A × 曲線`。
  目前 A = 1 沒問題，但 FX 顏色參數的 alpha 常被當成無意義欄位，哪天被填 0，
  整顆核心會消失且極難追查（additive 不讀 alpha，沒人會懷疑這裡）。
  **建議在 `HALNovaFX` 的參數契約測試補一條 `User.Color.A == 1` 斷言。**
- **Over 收束現在由兩條曲線分別描述，必須手動保持同步**：
  `FieldShell.Scale Alpha`（30.25:0.55 / 30.6:0.22 / 31:0）與
  `CoreSphere.Scale Alpha`（30.25:1.0 / 30.55:0.45 / 30.9:0.12 / 31:0）
  刻意錯開做「殼先塌、核心後收」。這是第二個維護點。
- **`CoreSphere` 曲線尾段 30.9→31.0 只有 0.1 個曲線單位，小於 LUT 格寬**
  （區間 9.8–31 共 21.2 ÷ 128 ≈ 0.166）。目前畫面已歸零所以沒出事，
  但之後精修尾段時**最小有意義的 key 間距是 0.17**，再密會被 LUT 抹平（11.2 節同一個坑）。
- **`Sparks` 仍飛出危險半徑**（Loop 最遠 1.44×R、Over 0.9 達 1.82×R）。
  這是第三輪就被指出、第三次沒處理。邊界對比沒再降所以還不到必改，
  建議把生成半徑與初速夾在 1.0×R 以內。

---

### 12.15 兩個方法學問題：相機角度與 `GroundRing`（2026-11-14 第九輪）

這一輪**沒有改任何造型參數**。獨立評審（刻意不給前次結論、用同一組畫面重量一次）提出兩項
【必改】，兩項都不是「這個數字調錯了」，而是**前面六輪的驗收方法本身有盲點**。
本輪只做查證與留檔。

#### 必改 1：四張截圖都是正俯視，而遊戲相機是 −60°

**前提已從工程核對，不是推測：**

- `Source/HAL_Future_Creation/VehicleDefinition.h` 的 `FVehicleCameraConfig::BoomTransform`
  預設是 `FTransform(FRotator(-60, 0, 0))`；`TestVehiclePawn.cpp:136` 也硬寫
  `CameraBoom->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f))`。**遊戲相機俯角是 −60°。**
  （`HALRoundMatchTypes.h` 的 `SpectatorPitch = -65` 是**觀戰**相機，不是這一個，別混用。）
- `Docs/Combat/Electric_Sphere_CN_v0.1.md`：`AHALElectricSphereGimmick` 的 **Actor 原點是球心**，
  且第 37 行寫「測試時將**球心放到車身高度**」。危險體是真球，球心離地。
- `M_FX_NovaShell` 的 Custom 節點第 2 行是 `float3 V = normalize(Parameters.CameraVector);`，
  `r` 是像素方向扣掉視線分量後的投影長度。**rim／edge／core／fil／gc 五項全部活在視線基底上**，
  所以這顆球從任何角度看都是同一個正圓，**沒有任何透視線索把它綁在地面上**。

於是：亮邊界圓畫在**球心**的螢幕投影位置。`camera_angle=top`（−90°）正是球心與其地面投影
在螢幕上重合的**唯一**角度。12.9～12.14 這六輪所有「邊界沒變動／邊界夠硬」的結論，
都是在一個**系統性隱藏此問題的角度**下得到的。

> **這一項目前無法在黑底預覽裡判定，而且我不同意評審給的結論方向。**
> 評審估算螢幕偏移約 8–13%×R，建議把 `RingBoost` 降到 0.6–0.8、讓 `GroundRing` 當唯一邊界。
> 但**車身中心也在同一個高度**（球心就是放在車身高度），兩者投影偏移相同，
> 所以「環畫在球心」對判斷「車能不能進去」其實是**對的**。真正的風險是
> 球殼剪影（球心高度）與 `GroundRing`（地面高度）在 −60° 下**分離成兩個不同半徑**，
> 玩家不知道該信哪一個。
> **結論：這必須在 PIE 用真實相機角度看，不能在這裡拍板。** 兩個評審各自獨立都指向 PIE。

#### 必改 2：`GroundRing` 到底畫不畫得出東西 —— 查清楚了，而且 12.12 留下的印象是錯的

評審指出 12.10 與 12.12 對這一層的說法互相矛盾。我用兩個直接測試結案：

**測試 A（評審建議的版本）**：`RingBoost` 設 0，重拍 Begin，量 r=120–155。

| r | RingBoost 1.5 | RingBoost 0 |
|---|---|---|
| 110 | 165.7 | 168.1 |
| **123** | **242.7** | **211.1** |
| 130 | 174.5 | 175.9 |
| 136 | 131.5 | 133.7 |
| 144 | 63.0 | 64.5 |

只有 r=123 的尖峰塌掉，**r=126–150 整段完全沒變**。那段在球殼剪影之外，
所以既不是殼層的 `rim`，也不是那條亮環的 bloom。

**測試 B（決定性的版本）**：直接停用 `GroundRing` 這一層，重拍 Begin。

| r | GroundRing 開 | GroundRing 關 | 差 |
|---|---|---|---|
| 110 | 165.7 | 84.5 | **−81** |
| **123** | **242.7** | **240.4** | −2 |
| 130 | 174.5 | 10.0 | **−165** |
| 136 | 131.5 | 1.9 | **−130** |
| 144 | 63.0 | 0.9 | **−62** |
| 全圖總亮度 | 8.357M | 4.475M | **−46%** |

**結論，兩條都要記住：**

1. **那條硬白尖峰確實是殼層的 `edge`**（關掉 `GroundRing` 之後 242.7 → 240.4 幾乎不動）——
   12.12 這一點是對的。
2. **但 `GroundRing` 絕對不是「白做」的一層：它貢獻了全圖 46% 的亮度，
   r=126–150 整圈外暈幾乎全部來自它。** 12.12／12.14 讀起來會讓人以為這一層沒有畫面，
   **那個印象是錯的**，下一個接手的人若照那個印象把它停用，會直接少掉一半的光。

**為什麼它在 Over 會消失（12.12 沒查到的那一塊）**：它的階段閘門**不在曲線上，
而在 spawn**——

```
GroundRing / emitter_update / SpawnRate     9277BE9541AE6EE7494FE7BEF540AACA
  ├─ SpawnRate          = 30.0（字面值）
  └─ Spawn Probability ──▶ SelectFloatFromArray   50956AAC44684B9C059011831530E7AE
                             ├─ Float Selection Array = NiagaraDataInterfaceArrayFloat
                             └─ Direct Array Index    = 綁 **User.Phase**
```

所以這一層是**用一個以 `User.Phase` 為索引的浮點陣列、在 spawn 階段決定要不要生成粒子**。
它既不需要 `Color` 模組，也不靠 `ScaleMeshSize` 曲線——12.12 說它的 `ScaleMeshSize` 曲線是死的
**仍然成立**，只是那不等於這一層沒有階段行為。**同一個系統裡有兩套完全不同的階段機制，
這是目前最容易讓人誤判的地方。**

#### 本輪驗證與狀態

**已驗證**：上述兩組量測；相機俯角與球心高度已從 C++ 與 `Docs/Combat` 核對。
兩個探針截圖（`RingBoost=0`、`GroundRing` 停用）已刪除，**四張正式截圖沒有被覆蓋**。
`RingBoost` 已還原 1.5、`GroundRing` 已重新啟用、`User.Radius` 400 / `Phase` 0 /
`PhaseElapsed` 0 已還原，全部存檔後 `list_dirty_packages` 為 0。**全程未提交。**

**未驗證、而且這裡驗不掉**：必改 1 的相機角度問題——需要 PIE 用 −60° 實拍 Begin 與 Loop，
量亮環中心與地面危險圈是否重合。

#### 評審提出、本輪判定不處理的

- **`Sparks` 飛出半徑：建議結案、不要再帶到下一輪。** 獨立評審自己量完的結論是
  Loop 在 r>1.14×R 只有 **5 個離散亮點**（最遠 1.41×R）、Over 0.9 是 9 個點——
  那是幾顆火星，不是會誤導邊界的光帶。此項已連續列出三輪，就此標記「已量測、不處理」。
- **邊界光暈外溢到 1.22×R**：誤差方向是保守的（看起來比危險區大，不會害玩家誤入），
  不急。真要收，`edge = smoothstep(RingS, RingS+rbw, r) * RingB` 的 `rbw` 只有 0.0105，
  等於一條約 3px 的**實心平頂亮帶**在餵 bloom；改成以 `RingStart` 為中心的雙邊細線
  可以在不移動峰值位置的前提下砍掉外溢。
- **細絲 TAA 風險有了可用的門檻**：實測線寬在 246px 球徑下是 4.2–4.4px、實心部分約 2.4px。
  **球在螢幕上小於約 100px 直徑時，實心部分就掉到 1px 以下**，加上 `FlickerRate 14`
  每秒重抽 14 次，是 TAA 閃爍的標準配方。PIE 時先量球的螢幕直徑；小於 100px
  就照 12.9 節的順序降 `FilamentCount` 22→14，**不要加粗**。

---

### 12.16 霧感遮罩的高度淡出（2026-11-14 第十輪）

團隊回饋：「外圍半透明遮罩有點太顯眼，希望越往穹頂頂點越透明，只有大概由地面往上 30%
是可視的白色霧感遮罩。」

#### 改動：`rim` 項乘上一個以局部上方向為基準的高度遮罩

`M_FX_NovaShell` 的 Custom 節點新增兩個輸入 `VeilLo` / `VeilHi`，對應兩個新的純量參數：

| 參數 | 預設 | 意義 |
|---|---|---|
| `VeilGroundZ` | **−0.16** | 這個局部高度（含）以下，霧是滿值 |
| `VeilFadeZ` | **+0.19** | 這個局部高度（含）以上，霧完全消失 |

```hlsl
float veil = 1.0 - smoothstep(VeilLo, max(VeilHi, VeilLo + 0.01), d.z);
...
float rim = pow(r, max(RimF, 0.05)) * Fill * (0.40 + 0.60 * saturate(Amt)) * veil;
```

**座標換算**：`d.z` 是 `normalize(LPos).z`，球底 −1、頂點 +1，**不是**世界高度。
換算需要 Actor 自己的擺放：`z_local = (h_world − 球心高度) / Radius`。
球心放在車身高度（約 65cm）、`Radius` 400 時，地面是 **−0.16**、
穹頂可見高度的 30% 是 **+0.19**——這就是預設值的由來。
**球心高度或 Radius 改了，這兩個值要跟著重算。**

> **這是整個殼層材質裡唯一以世界上方向為基準的項。** 其餘五項（`rim` 的 `r`、`edge`、
> `core`、`fil`、`gc`）全部活在視線基底上——這正是 12.15 節必改 1 講的：
> 這顆球從任何角度看都是同一個正圓，沒有任何線索把它綁在地面。
> 這個高度淡出讓殼層**第一次有了地面錨點**，算是附帶解掉一部分那個問題。

#### 驗證：用真實的 −60° 相機俯角，不是正俯視

`editor_query / capture_scene_preview` 可以指定 `camera: {location, rotation, fov}`，
所以這一輪第一次用遊戲真正的俯角拍（`rotation: [-60, 0, 0]`、
`location: [-160, 0, 277]`、`fov 60`、560×420、Loop 階段、`User.Radius` 110）。
`preview_system` 只有 front／top／three_quarter／side 四個預設，拍不出 −60°。

**機制正確**：關掉淡出 vs 開啟淡出，64,812 個像素改變、單點最大差 462。
逐列平均亮度（畫面上方＝世界高處）：

| 畫面列 | 關 | 開 | 差 |
|---|---|---|---|
| 60（接近頂點） | 59.0 | 28.9 | **−30.1** |
| 180 | 116.4 | 113.7 | −2.7 |
| 340 | 105.8 | 94.4 | −11.4 |
| 380（接近地面） | 90.2 | 92.5 | **+2.3** |

方向完全正確：頂點掉最多、地面不動。把 `FillGlow` 暫時拉到 0.6 再拍一張診斷圖，
梯度形狀一眼可見（底部濃、往上乾淨）——**形狀對，剩下的只是強度問題**。

#### 但是：使用者看到的「外圍遮罩」大部分不是這一層

把 `FillGlow` 設 0（殼層的霧完全關掉）重拍，**那層外圍霧幾乎沒有變**。拆出來是兩個來源：

1. **`GroundRing`**——12.15 節量到它佔全圖 46% 亮度。它的渲染器用
   `/Game/Assets/FX/Materials/M_FX_NovaRing`，是一個 **UV 平面環**
   （`p = UV*2-1`、`band = 1 - saturate(abs(length(p) - Rad)/Wid)`），
   躺在地面上。**它沒有「穹頂頂點」可言，所以「越往上越透明」對它沒有意義**，
   要收只能直接降它的亮度或寬度。
2. **Bloom**——近白的細絲與 242 亮度的邊界環餵出來的後處理光暈。

所以 `FillGlow` 停在 **0.15**（12.9 節定的值，沒有改）時，這一輪的改動在正式設定下
**是看得出來但幅度不大的**。要更接近「只有底部 30% 是可視白霧」，有兩個方向，
**都需要團隊決定，本輪未做**：

- **把 `FillGlow` 調高**（例如 0.4–0.6）。霧現在被限制在底部 30%，上半部不會再被填滿，
  所以 12.9 節「盤面會洗白開進去的車」的理由部分失效——**但只是部分**：
  底部 30% 正好是車子所在的高度帶，這個取捨必須在 PIE 看真場地底色才能判。
- **降 `GroundRing` 的亮度或寬度**。這是外圍霧的真正主力，但它是地面環，
  12.15 節剛確認它不是那條硬白邊界（那是殼層的 `edge`）。

#### 本輪驗證與狀態

**已驗證**：27 個輸入腳位逐一確認 `connected: true`（`inputs` 整組取代會全部斷線，
見 12.13 節）、`recompile_material` 無錯誤、`validate_material` 只剩既有的
`translucent_no_opacity` warning、`M_FX_NovaShell` 與 `MI_FX_NovaShell` 都已存檔。
上述 −60° 量測。`FillGlow` 已還原 0.15、`User.Radius` 400 / `Phase` 0 /
`PhaseElapsed` 0 已還原，`list_dirty_packages` 為 0。**全程未提交。**

**未驗證**：PIE 實際畫面與真實場地底色下的對比；本輪未重拍四張正俯視的正式截圖
（那四張是正俯視驗收用，這個改動在正俯視下會變成「中間透明、外圈一圈霧」，
不是這個效果該被判讀的角度——見 12.15 節）。

---

### 12.17 拿掉殼層的外圈白線（2026-11-14 第十一輪）

團隊要求：「不要顯示最外圍的那個白色圓形，也就是遮罩的輪廓。」

#### 改動：`MI_FX_NovaShell.RingBoost` 1.5 → **0**

那條線是殼層材質的 `edge` 項，12.15 節已經用兩個測試確認過歸屬：

```hlsl
float rbw  = max((1.0 - RingS) * 0.30, 0.004);
float edge = smoothstep(RingS, RingS + rbw, r) * RingB;
```

`RingBoost = 0` 直接關掉，不需要改 HLSL。

> **這和 12.15 節獨立評審的建議方向一致。** 它當時說：球殼剪影畫在**球心**高度、
> `GroundRing` 畫在**地面**高度，−60° 下兩者會分離成兩個半徑，
> 「讓 `GroundRing` 當唯一邊界，不要讓球殼剪影跟地面圈搶『半徑在哪』」。
> 團隊這個要求剛好把那件事做掉了。

#### 驗證：−60° 真實俯角，逐階段量邊界

沿通過環最寬處的水平掃描線量「峰值」與「剛好在外側的中位數」，差值即邊界階梯：

| 階段 | 邊界峰值位置 | 峰值 | 外側中位 | **階梯** |
|---|---|---|---|---|
| Loop（改前，有 `edge`） | x=108 / 449 | 243.5 | 45.8 | **198.2** |
| Loop（改後） | x=141 / 412 | 234.7 | 178.0 | **63.1** |
| Begin（改後） | x=110 / 449 | 206.8 | 50.8 | **157.2** |

**Begin 沒問題。** `GroundRing` 完整接手，階梯 157.2，而且位置 x=110/449 與改前的
`edge` 幾乎重合——在 −60° 下它讀起來是一個**躺在地面的橢圓**，那才是車子真正要閃的足跡。

**Loop 是問題所在。** 邊界階梯由 **198.2 掉到 63.1**，大約剩三分之一，
而且峰值位置向內移到 x=141/412——那已經不是邊界，是殼層本體與電弧。
原因是 Loop 的 `rim` 閘門全開、細絲與纏繞弧把能量堆在外圈，
內部中位衝到 202、外側中位 178，`GroundRing` 淹在裡面凸不出來。

> **Loop 正是碰到會中的階段**，AGENTS.md 第 12 節要求危險區域必須能被快速識別。
> **這一項尚未解決，本輪只記錄不自行處理**（團隊要的是拿掉那條線，不是換一條）。

#### 可用的修法（未做，待團隊決定）

- **收窄 `GroundRing` 的 `RingWidth`。** `M_FX_NovaRing` 的 band 是
  `1 - saturate(abs(r - Rad)/Wid)` 再平方，峰值恆為 1.0，**沒有亮度旋鈕**，
  所以只能靠收窄讓它更銳利來拉出對比。注意 `GroundRing` 的渲染器直接用母材質
  `M_FX_NovaRing`，**沒有 MI**，要嘛建一個實例、要嘛改母材質預設值。
- **給 `M_FX_NovaRing` 加一個亮度參數。** 比收窄更直接，但要動母材質。
- **把 `RingBoost` 設成一個很小的值（例如 0.3–0.5）而不是 0。** 保留一點剪影提示，
  但這與團隊這次的要求相反，列在這裡只是為了完整。

#### Over 段

`GroundRing` 在 Over 被 spawn 階段的 Phase 陣列關掉（12.15 節），`edge` 現在也沒了，
所以 Over 0.55 完全沒有邊界線，只剩塌縮中的電弧球。**這是合理的**——
場已經在關閉，不再需要告訴玩家閃哪裡，也符合 12.1 節「全部向中心塌縮歸零」。

#### 本輪驗證與狀態

**已驗證**：上述三階段的 −60° 擷取與量測（`capture_scene_preview`、
`rotation: [-60,0,0]`、`fov 60`、560×420、`User.Radius` 110）。
`MI_FX_NovaShell` 已存檔；`User.Radius` 400 / `Phase` 0 / `PhaseElapsed` 0 已還原，
`list_dirty_packages` 為 0。**全程未提交。**

**未驗證**：PIE 實際畫面；本輪未重拍四張正俯視的正式截圖（同 12.16 節的理由）。
**已知未解**：Loop 的邊界對比掉到 63.1。

---

### 12.18 收窄地面環，把 Loop 的邊界對比拉回來（2026-11-14 第十二輪）

處理 12.17 節留下的「已知未解」：拿掉殼層外圈白線之後，Loop 的邊界階梯掉到 63.1。

#### 改動 1：新增 `MI_FX_NovaRing`，`GroundRing` 的渲染器改指向它

`GroundRing` 原本**直接使用母材質** `M_FX_NovaRing`，是整個系統裡唯一沒有材質實例的一層
（其餘都有 `MI_FX_*`）。本輪補上實例並把渲染器改指向它，調參不再需要動母材質預設值。

#### 改動 2：`MI_FX_NovaRing.RingWidth` 0.16 → **0.06**

`M_FX_NovaRing` 的環是 UV 平面上的徑向帶：

```hlsl
float band = 1.0 - saturate(abs(r - Rad) / max(Wid, 0.001));
band = band * band;
```

`Rad = 0.8`、`Wid = 0.16` 等於環跨 **r = 0.64…0.96**——佔半徑的 ±20%，所以它在 Loop 被
電弧和霧淹掉時完全凸不出來。**`band` 的峰值恆為 1.0，這個材質沒有亮度旋鈕**，
所以唯一能拉對比的手段就是收窄。

#### 量測（−60° 真實俯角，沿通過環最寬處的水平掃描線）

| 階段 | `RingWidth` | 峰值 | 外側中位 | **階梯** |
|---|---|---|---|---|
| Loop | 0.16 | 241.1 | 178.0 | **63.1** |
| Loop | **0.06** | 241.5 | 111.6 | **129.9** |
| Loop | 0.035 | 241.5 | 107.0 | 134.5 |
| Begin | 0.16 | 207.8 | 54.9 | **152.9** |
| Begin | **0.06** | 209.6 | 53.3 | **156.3** |

**Loop 的階梯翻了一倍**（63.1 → 129.9），**Begin 完全沒有被犧牲**（152.9 → 156.3，
峰值位置 x=111/448 不變）。

> **停在 0.06 而不是更窄，是量出來的決定。** 0.035 只多拿到 4.6（129.9 → 134.5）——
> 因為那時外側剩下的 107 已經不是環的寬度，而是**電弧餵出來的 bloom**，收窄環對它無效。
> 同時環的半高寬從 53px 掉到 22px（Begin，環半徑約 170px）；再窄就要開始擔心
> 12.15 節記的 TAA 門檻。**0.06 拿到九成的好處，又留著安全餘裕。**

#### 三階段狀態

- **Begin**：地面環清楚，−60° 下是躺在地面的橢圓——車子真正要閃的足跡。
- **Loop**：地面環在球體下緣與左右兩側明顯分離出來，不再被殼層淹沒。
- **Over 0.55**：`GroundRing` 本來就被 spawn 階段的 Phase 陣列關掉（12.15 節），
  本輪前後比對只差 3,109 個像素、每像素平均 2.6——那是火花的隨機抖動，不是環。
  **Over 仍然沒有邊界線，這是刻意的**（場正在關閉，見 12.17 節）。

#### 本輪驗證與狀態

**已驗證**：上述量測；`validate_system` `valid: true`，警告仍是既有那七筆
`Particles.MeshScale`（12.12 節，不是誤報）；`MI_FX_NovaRing` 與 `NS_LightningNova`
都已存檔；`User.Radius` 400 / `Phase` 0 / `PhaseElapsed` 0 已還原，
`list_dirty_packages` 為 0。**全程未提交。**

**未驗證**：PIE 實際畫面與真實場地底色下的對比——**地面環是貼在地板上的，
它的對比最容易被場地底色吃掉，這一項在黑底預覽裡的數字樂觀**。
本輪未重拍四張正俯視的正式截圖（同 12.16 節理由）。

#### 現行值摘要（12.16–12.18 三輪的淨結果）

| 資產 | 參數 | 值 |
|---|---|---|
| `MI_FX_NovaShell` | `RingBoost` | **0**（外圈白線關閉） |
| `MI_FX_NovaShell` | `VeilGroundZ` / `VeilFadeZ` | **−0.16** / **+0.19** |
| `MI_FX_NovaShell` | `FillGlow` | 0.15（未改，12.16 節留的待決項） |
| `MI_FX_NovaRing`（新） | `RingWidth` | **0.06** |
| `MI_FX_NovaRing`（新） | `RingRadius` | 0.8（未改） |
