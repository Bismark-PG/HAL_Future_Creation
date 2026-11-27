# 擴散式治療區域

2026-11-24；接續 `main/1cf84e6`，本機引擎 UE **5.8.2 / CL 56702186**。

**狀態：C++ 與測試已撰寫，尚未編譯或執行通過。** 本次統一構建入口與同基線診斷均被目前執行環境的引擎目錄存取限制擋住；不能引用既有打滑區或柱子測試結果作為新功能證據。詳見本文驗證段及 [本次記錄](Expanding_Healing_Area_Verification_20261124.json)。

## 2026-11-27 每次回血的 Debug Text

依用戶要求，開啟場上治療區的 **Debug Draw** 後，該範圍內車輛每次被區域 HoT 實際治療，會在車輛中心上方 180 cm 顯示綠色文字，例如 `+5.00 HP (60.00 / 100.00)`，持續 0.8 秒、帶陰影。98→100 顯示 `+2.00 HP`，滿血沒有增加不顯示。這沿用同一個 Debug Draw 開關，與 Show Editor Gizmo 無關，不需要新 BP 接線；本輪沒有修改或保存既有 BP／地圖。

來源由 `UHALHealingAreaReceiver::HealthChanged` 的 GAS Attribute callback 辨識：須為這個接收元件建立的 `UHALHealingAreaEffect`，且 CurrentHP 的實際正向變更；基本血量設定、換車／重置、其他技能回血不冒充區域回血。若目前登記的任何一個治療區開了 Debug Draw，按伺服器 Actor ID 選一個區域發送，重疊區域不重複發文字。退出最後來源或停止實際治療後不再發送；已顯示文字會自然結束。

區域使用 **Unreliable NetMulticast** 傳遞一次性除錯結果，各端以 Unreal `DrawDebugString` 加入當地 HUD 的 DebugTextList。客戶端不依 HP 複製差值猜測回血，也不執行 HP 修改。Shipping 不發送或繪製這些提示；沒有新增 Tick、HP 儲存、正式 UI 或 Gameplay Ability。除錯 RPC 在封包遺失時可略過，不能當作可靠戰鬥紀錄；需有效本機 PlayerController／HUD 才會顯示文字。

新增原生測試 `DebugTextUsesActualAreaHealOnly`，用真正 World／GAS 週期和引擎 HUD DebugTextList 驗證：Debug 關閉仍回血但不顯字；雙區重疊只一份；實際 +5；接近滿血實際 +2；滿血不發；手動 HP 設定和獨立 GE 治療不發；關閉後停止提示。現有 `HAL.NetworkSmoke.HealingAreas` 增加雙區 Debug Draw 和客戶端 HUD 文字檢查。測試入口更新為至少 **18 項原生（治療 6＋MovementAreas 7＋ArenaGenerator 5）**及主客各 1 項；這些是待驗證的數量，並非本輪通過結果。

本輪接續 `main/69429df`，核對實際引擎仍是 **5.8.2-56702186**，與狀態文件部分團隊的 5.8.3 記錄有差異。`BuildAndLaunch.cmd -NoLaunch -NoPopup` 在 Engine Build.bat 的 Source 前置檢查停止，exit 999，**未進入 C++／UHT／連結，原生、雙端與實際畫面均未執行**。日誌 `Saved/BuildTool/BuildAndLaunch_20261127_150419_466_a29cdf`。完成 `git diff --check` 和測試腳本 PowerShell AST 語法解析；兩份原有未提交資產 SHA256 前後保持。詳細見[本次驗證記錄](Expanding_Healing_Area_Debug_Text_Verification_20261127.json)。沒有提交／Push。

團隊接續：保存並關閉 Editor／Live Coding，於正常終端執行 `BuildAndLaunch.cmd -NoLaunch -NoPopup`，成功後執行 `Tools/TestHealingAreas.ps1 -EngineRoot 'D:\Unreal Shit\UE_5.8'`。重開 Editor，用現有 BP 勾 Debug Draw，先受傷再駛入，核對每次 +5、98→100 只 +2、滿血不發字、離區停止、兩區不重複；主機和客戶端各看一次。這一版沒有為正式遊戲製作補血飄字 UI。

## 2026-11-24 編輯器範圍 Gizmo

依用戶追加要求，新增 `UHALHealingAreaGizmoComponent` 編輯器專用錨點和 Editor 模組的 `FComponentVisualizer`。選取治療區 Actor 時，顯示**最大半徑的綠色中心圓、青色上下邊界／圓柱側線，以及垂直高度軸**。綠色中心圓位於 Actor 實際中心高度，青色上下邊界分別位於 `Z ± Half Height`，沒有投影到地板，用於定位實際圓柱範圍。每次視窗繪製直接讀 Settings，改半徑／半高可更新；始終按世界 XY／Z 繪製，不隨 Actor 或父層級的 Scale／Rotation 縮放或傾斜。

`Healing Area → Debug → Show Editor Gizmo` 預設勾選；取消勾選或取消選取即可關閉。`G` 開啟 Game View 時也隱藏。錨點透過 `CreateEditorOnlyDefaultSubobject` 建立，不新增碰撞／物理／Overlap／Replication／Tick，無需修改 BP 或 Event Graph。Editor 模組在啟動／卸載時註冊／移除可視化器；獨立遊戲不載入該 Editor 模組，Cook 排除錨點。

這個編輯器預覽與原有 **Debug Draw（PIE／遊戲中目前半徑的貼地網格）** 分開控制：前者是最大範圍擺場參考，後者顯示遊戲中擴張的實際大小。要在遊戲中看圈，仍需勾 Debug Draw；Show Editor Gizmo 不會替它開啟。

**本次驗證：** 核對 `main/1cf84e6` 及引擎 5.8.2-56702186；保留前輪程式與團隊新增的 `Content/Maps/MVPB_Match.umap` 未提交修改，地圖前後 SHA256 均為 `8C2AE3518AAA13935716707201AB73D3DB6136B50DC4B2DE0A40B6DED8310403`。`git diff --check` 通過；對照本機引擎的 ComponentVisualizer 註冊／繪製介面和 DrawWireCylinder 簽名。統一入口 `BuildAndLaunch.cmd -NoLaunch -NoPopup`（僅行程 TEMP／TMP 指向允許的 Temp）再試仍於引擎 Build.bat 的 Source 前置檢查停止，**exit 999、未進入 C++／UHT／連結**，日誌 `Saved/BuildTool/BuildAndLaunch_20261124_173028_368_b06845`。本輪沒有新增或執行自動化、沒有實際 Editor 畫面驗收；先前任何測試均不是本次 Gizmo 證據。

**下一步手動驗收：** 正常團隊終端完成統一入口編譯，重開 Editor；選取 `BP_ExpandingHealingArea` 實例，確認 Show Editor Gizmo 勾選後出現圓柱，調整 Maximum Radius／Half Height 看範圍更新；改 Actor Scale 為 `(2, .5, 3)` 並旋轉，確認圓柱仍維持世界尺寸與世界 Z 軸；上下移動 Actor 看中心和高度同步移動；取消選取、關閉勾選與 Game View 各能隱藏。PIE 再確認 Debug Draw 青／綠貼地網格仍依目前半徑變化，選取 Gizmo 不會影響回血。沒有自動保存 BP／關卡、提交或 Push。

## 玩法與預設值

`AHALExpandingHealingArea` 是獨立、伺服器權威的圓柱區域。伺服器每 **0.02 遊戲秒**檢查車輛中心；生成時先檢查一次。擴張階段已有治療資格，車子可自行駛入，也可被擴張覆蓋。所有有效、存活且已綁定 ASC 的車都能接受，包括生成者；不做隊伍或生成者免疫。

| 欄位 | 預設 | 單位／含義 |
|---|---:|---|
| Expansion Seconds | 2 | 秒；由 1 cm 種子半徑線性擴張，0 表示立即全範圍 |
| Duration Seconds | 5 | 秒；擴張結束後的維持時間，總壽命為兩者相加 |
| Maximum Radius | 500 | 世界 cm，不隨 Actor Scale／Rotation 改變 |
| Half Height | 200 | 世界 cm，以 Actor 位置為圓柱中心 |
| Healing → Heal Per Tick | 5 | 每個完整週期恢復的 HP，0.01～10000 |
| Healing → Period Seconds | 1 | 遊戲秒，0.1～60 |
| Debug Draw | false | 調試地面網格：擴張青色、維持綠色；Shipping 無顯示 |

進區施加一份 Infinite、Periodic 的 `UHALHealingAreaEffect`，**進區不立即回血**，等完整第一週期；離開最後一區後移除該 GE、停止未來週期，已回復的 HP 保留。快速出入不會刷立即回血，也不會累計未完成週期。滿血時仍保留 HoT 與週期，後續受傷後照原節奏回血。現有 `UHALHealthAttributeSet` 負責把 HP 限制在 0～MaxHP，沒有新增另一份 HP。

離區停止以伺服器下一次取樣為準，通常不超過 0.02 遊戲秒，低幀率仍受伺服器排程影響。採車輛中心而非車身邊緣；不保證極小區域的高速穿越連續檢測。範圍不投影地形，翻滾／飛離高度範圍視為退出；斜坡、多層地圖需配置中心高度和 Half Height。

## GAS、重疊與清理

`UHALHealingAreaReceiver` 隨車輛建立，使用既有 `BindVehicleAbilitySystem`／`UninitializeAbilitySystem`／`RetireFromGameplay` 接點綁定、解除與清理。核心規則無 Tick、無 Gameplay Ability；區域用 Timer，Actor Tick 只負責可關閉的調試繪製。

- 多來源共享接收元件擁有的**一份** HoT；不把每區的回血量相加。
- 不同配方取 `HealPerTick / PeriodSeconds` 最大者；同速率取較短週期，與容器遍歷順序無關。
- 配方相同時，重複登記、加入另一區、移除其中一區均保留原有 GE／週期。切到不同配方時建立新 GE，重新等待一個完整週期，沒有立即回血。
- 移除時只使用接收元件自己保存的 `FActiveGameplayEffectHandle`，不按類別或寬泛 Tag 清除，保留技能等獨立來源的治療。
- 區域 EndPlay／銷毀和接收元件 EndPlay 都會清除來源。HP 降至 0 時以 Attribute delegate 同步撤除 HoT 並解除綁定，後續區域取樣拒絕重新施加，避免下一次週期復活淘汰車。
- `Effect.Scope.Avatar` 沿用 ASC 換 Avatar 清理機制；延遲到來的舊車清理不能移除新車 GE。普通解除綁定保留有效來源供同 Avatar 重綁；正式離場／淘汰清空來源。
- 遵守現有 `HALGameFlow::Permissions(...).bWorldInteractions`；不可互動的車輛在下一次區域取樣退出。這不另行改寫練習場、房間或比賽規則。

新增原生 Tags：`State.Healing.Area`（可供 UI／表現觀察）、`Effect.Vehicle.HealingArea`、`Data.Healing.AreaAmount`。GE 使用正值 CurrentHP Additive Modifier，沒有用 CombatResolver 的傷害入口偽裝負傷害，沒有新增 Execution Calculation。

區域不持有 ASC。Settings、StartedAt 與 Debug Draw 複製給客戶端，客戶端用 GameState 伺服器時鐘重建半徑；HP／狀態由既有 ASC 複製。客戶端不施加 GE、不登記來源、不提交 HP／命中結果。`GetCurrentRadius` 提供唯讀半徑；這一版未製作正式 Niagara／材質／回血圖示，只提供調試顯示和狀態 Tag。

## 與打滑區共用部分

`HALExpandingAreaGeometry` 只抽出半徑、世界 XY 圓柱包含測試和既有貼地調試網格。打滑區原本的反射欄位、序列化配置與效果接收機制保持原樣；其參數和地面投影行為需由既有 MovementAreas 回歸重新驗證。貼地射線仍忽略區域本身、Instigator 與 Owner 生成柱，物理模擬物件不能被當成支撐地面。

本階段未把治療區加入 `EHALArenaEffect`／柱子隨機池；這與先完成獨立區域的授權範圍一致。柱子接入、圖示和正式演出留待後續環節。

## 團隊 Editor 操作

先保存並關閉 Unreal Editor／Live Coding，在工程根目錄執行：

```powershell
.\BuildAndLaunch.cmd -NoLaunch -NoPopup
```

編譯成功後重開 Editor：

1. Content Browser 新建 Blueprint Class，於 All Classes 搜尋 `HALExpandingHealingArea`，建立輕量子類，例如 `BP_ExpandingHealingArea`。代理本輪沒有建立或修改 `.uasset`／`.umap`。
2. Class Defaults 搜尋 `Healing Area`；調整 Settings 的 Timing／Shape／Healing，先勾選 Debug Draw，Compile／Save。不需要 Event Graph。
3. 將 BP 放到測試場；中心高度對齊車輛物理根。調試網格會投影到地板，但**不會移動實際高度判定**，不要用地面顯示高度反推權威圓柱中心。
4. 若要遊戲中生成：Project Settings → Game → HAL Debug → Healing Area → Default Healing Area Class 選上面 BP，Healing Area Forward Distance 預設 350 cm（0 可生成在車中心），保存設定後重開測試進程。
5. 在自己的遊戲控制台輸入 `HALSpawnHealingArea`。伺服器決定類別、距離和位置，保留所選 BP 的預設；客戶端只發無參數請求。原生類可直接生成，但預設 Debug Draw 關閉，畫面不會自動有圈。明確清空類別或配置非法會拒絕生成，不暗中回退。命令在 Shipping 禁用。
6. 先讓車受傷，駛入看 HP 每秒 +5、出區停止；靜止在外側等待擴張覆蓋，再測多圈重疊、先後離開／銷毀、滿血後受傷、車輛淘汰／換車。Listen Host／Client 各自核對 HP、範圍和 Tag；`hal.HealingAreaLog 1` 可開權威配方日誌。

## 自動化與本輪驗證限制

目前有六項 `HAL.FutureCreation.HealingAreas` 原生測試（第六項於 2026-11-27 加入）：

| 測試 | 驗證內容 |
|---|---|
| ConfigurationAndSharedGeometry | 預設、非法值、範圍邊界與打滑擴張一致 |
| PeriodOverlapExitReentryAndCap | 真正 World／GAS Timer 週期、重疊不加倍／不重置、離區／重進、MaxHP、來源銷毀 |
| StrongestRecipeSelectiveCleanupAndAvatar | 配方選擇與平手、獨立同類 GE 保留、解除／重綁、換 Avatar、舊車延遲清理、Owner 銷毀 |
| DeathPreventsRevival | 首次週期前 HP 歸零，同步清理且後續週期不復活 |
| ExpandingVehicleLifecycleAndDebugCommand | 已保存車輛 BP 唯讀載入、原生接收元件、生成者、擴張覆蓋、離區／高度／到期、調試生成與空類不回退 |
| DebugTextUsesActualAreaHealOnly | 真正 GAS 週期與 HUD DebugTextList，檢查 Debug 開關、實際治療量、部分治療、滿血、重疊和獨立回血來源 |

另加 `HAL.NetworkSmoke.HealingAreas`：兩個獨立進程、80 ms 主機出站模擬延遲、兩個重疊區域、一份回血、移除一區繼續回血、最後一區自然到期、HP／雙端 Tag／區域配置與半徑複製、客戶端不寫來源。此專項不代表晚加入／四人／真實 RTT／畫面驗收。

完成統一入口編譯後，一次執行原生＋MovementAreas＋ArenaGenerator，再跑雙端：

```powershell
.\Tools\TestHealingAreas.ps1 -EngineRoot 'D:\Unreal Shit\UE_5.8'
```

可加 `-SkipNetwork` 只跑原生；預設報告寫入全新 `Saved/HealingArea/Tests_時間`，不覆蓋舊證據。Native 預期至少 18 項（治療 6＋現有 MovementAreas 7＋ArenaGenerator 5），同時檢查治療測試確實存在，避免舊 DLL 漏掉新測試仍顯示成功。雙端各 1 項。腳本只結束它自己建立的測試行程，不停止團隊既有 Editor。

**本輪實際執行：** `git diff --check` 通過，測試腳本 PowerShell AST 語法解析通過；源碼對照本機 5.8.2 API 檢查了 Period、週期首次執行開關、GE Handle 和 Attribute delegate 接點。這些都不能替代 C++ 編譯與玩法實測。

**未完成：** C++／UHT／連結、五項新原生與既有回歸、雙端、新畫面、Game Target、Cook、晚加入、四人及團隊實測。沒有運行舊 DLL 冒充新功能測試，也沒有沿用之前 12/12 的區域測試結果。

構建受阻過程：

1. 統一入口 `BuildAndLaunch.cmd -NoLaunch -NoPopup`，沙箱 TMP 含 `#!sandbox...`，Build.bat 停在鎖檔重試；只中止了本輪自己的命令。
2. 僅把該行程 TEMP／TMP 指向允許的 `C:\Users\USER\AppData\Local\Temp` 後，仍用統一入口；Engine 5.8.2／Editor Win64 Development 基線不變。Engine Build.bat 回報找不到相對 Engine/Source，exit 999；未進入 C++ 編譯。正式日誌 `Saved/BuildTool/BuildAndLaunch_20261124_170932_320_36fbcc`。
3. 為診斷同一入口錯誤，曾直接用 cmd 呼叫原 Build.bat，保留 `HAL_Future_CreationEditor Win64 Development -Project=... -WaitMutex -NoHotReload -NoLiveCoding -Log=...`；同樣在前置檢查失敗。
4. 再以引擎自帶 dotnet 直接進入相同 UBT Target／參數，確認是 `UnauthorizedAccessException: Access to the path 'D:\Unreal Shit\UE_5.8\Engine\Source' is denied`，於 UBT 設定工作目錄時就停止。未修改引擎、未清快取、未要求或繞過權限。

僅診斷所用的完整直接 UBT 命令（不是新的日常構建入口）：

```powershell
$env:TEMP = 'C:\Users\USER\AppData\Local\Temp'
$env:TMP = $env:TEMP
$env:DOTNET_ROOT = 'D:\Unreal Shit\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64'
$env:DOTNET_MULTILEVEL_LOOKUP = '0'
$env:DOTNET_ROLL_FORWARD = 'LatestMajor'
& 'D:\Unreal Shit\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe' 'D:\Unreal Shit\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll' HAL_Future_CreationEditor Win64 Development '-Project=D:\Unreal Shit\HAL_Future_Creation\HAL_Future_Creation.uproject' -WaitMutex -NoHotReload -NoLiveCoding '-Log=D:\Unreal Shit\HAL_Future_Creation\Saved\HealingArea\20261124\BuildDiagnostic\UBT_Direct.log'
```

下一入口：在具備引擎存取權的正常團隊終端用統一入口編譯，再執行上述測試腳本，依新結果修正和更新狀態文件；之後手動建立 BP／擺場測試。所有新原始碼、測試、腳本、本文與狀態文件仍需正常 Git 提交才能跨設備同步；本輪沒有自動提交或推送。
