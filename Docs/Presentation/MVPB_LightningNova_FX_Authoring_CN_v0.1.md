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
| `User.Color` | Linear Color | 主色 |

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

## 6. 夾具的能力邊界（已判定，不是缺陷）

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

結論：**本環節沒有發現 Blueprint 或 Niagara 的接線缺陷**，但也還沒有任何證據顯示特效真的會動。
值的驗證只能在 PIE 內做，見第 8 節。測試因此不斷言這些值，只鎖住名稱、型別與「沒有多出未宣告參數」。

## 7. 移除 Event Tick（團隊在 Editor 執行）

AGENTS.md 第 13.7 節要求能由事件／狀態變更驅動時就不新增 Tick。目前 `User.PhaseElapsed` 由
`Event Tick` 每幀推一次，可以改成只在階段轉換時推一次相位起點，讓 Niagara 自己算經過時間。

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
