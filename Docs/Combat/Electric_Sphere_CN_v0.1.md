# 球形電擊區域：三階段生命週期

2026-11-10，接續 `main/e12a1d9`（起始工作區乾淨）。本機引擎 UE 5.8.2-56702186，其他設備的 5.8.3 驗證不視為本輪結果。

## 規則與預設

新增 `AHALElectricSphereGimmick`，Actor 原點是球心；固定位置的 Query Only 電場，不是可控球、不模擬物理、不阻擋車輛。本次不做生成器。

| 階段 | 預設時間 | 行為 |
|---|---|---|
| Begin | 1 秒 | 生成預告，不電擊 |
| Loop | 5 秒 | 電場啟用；接觸回彈、眩暈與 shocked；不扣 HP |
| Over | 1 秒 | 關閉電場，播放消散，結束後由伺服器銷毀 |

半徑預設 300 cm；時長使用遊戲秒。有效持續時長從 Loop 起算，總時長為三階段相加。Begin／Over 可為零，Loop 至少 0.01 秒。零時長階段可能直接跳過；不要求播放零長度特效。

沿用電牆 Config 的 Scan Interval、Bounce Back Distance／Seconds、Stun Seconds。回彈沿水平進入方向反推；靜止且原本在區域內時向球外推，球心水平位置完全重合則向世界 +X。不增加傷害、球強化或脫離區域後解除眩暈的規則。電場關閉／來源銷毀不會提前移除已結算的眩暈。

## 實作邊界

球體繼承電牆的接觸掃描、實際車體高速 Sweep、連續接觸鎖、GAS 眩暈／電擊標記及擁有者回彈預測。電牆僅新增可覆寫的接觸表面、啟用條件、幾何與推送方向，既有電牆預設啟用規則保持。

為保留既有原生子元件身份，球體仍繼承牆的元件，但全部牆／柱碰撞與顯示都關閉；只有 SphereField 可參與電擊查詢。不要在球體設定電牆的 PillarSpacing、Height 等幾何欄位；球體形狀只看 Sphere Config → Radius。保持 Actor Scale 為 1，直接改 Radius。

伺服器決定生成時間和銷毀，各端根據複製的配置、開始時間與 GameState 伺服器時鐘求當前階段。晚加入／重新相關可直接進入 Loop 或 Over，不重播舊預告。使用定時器，不新增 Actor Tick、傷害 RPC 或生成器。

C++ 生成入口為 deferred spawn → ConfigureSphere（球體參數）／Configure（電牆接觸參數）→ FinishSpawning。只接受伺服器 BeginPlay 前配置；一般 SpawnActor 也可直接使用類別預設。

## 團隊 Editor 接線

1. 完整編譯後開啟 Editor，建立以 `HALElectricSphereGimmick` 為父類的輕量 Blueprint，例如 `BP_ElectricSphereGimmick`。
2. Class Defaults → Electric Sphere → Sphere Config 設定 Radius、Begin Seconds、Loop Seconds、Over Seconds。
3. Electric Wall → Config 只調接觸掃描、回彈距離／時間及眩暈時間。
4. 在 Event Graph 加入 `On Stage Changed`，依 New Stage 分流 Begin／Loop／Over。先停止上一階段特效，再啟用新階段特效；不得在這裡修改碰撞、傷害或時長。
5. 事件提供 Stage Age Seconds、Stage Duration Seconds、Radius Cm。特效用 Age 接續晚到階段，用 Radius 對齊範圍。尚無特效也能完整運行。
6. 特效元件掛在此 Actor 下，避免 Auto Activate，讓 Actor 銷毀時一起清除；若額外在世界生成獨立特效，必須在 EndPlay 自行清理。
7. 測試時將球心放到車身高度，Scale 保持 1；可暫時手動擺在地圖驗證，一開始就會自行倒數。生成器日後再接。

代理未修改 Blueprint、Niagara、DA 或地圖資產。

## 驗證與團隊實測

本輪原生 `HAL.FutureCreation` 自動化 R2：**189/189 通過，0 測試警告／失敗／未執行**，包含新增球體兩項與既有電牆回歸。球體測試覆蓋三階段、已有車輛啟用即命中、去重、消散後銷毀，以及實際車體完整高速穿越。

首輪原生 R1 為 188 通過／1 失敗：生命週期測試同一引擎幀多次 Tick 測試世界，TimerManager 不逐次更新；改用既有跨幀 latent 夾具後通過，未改生產生命週期規則。中間 Editor R3 測試夾具誤用 SharedRef 的 ToSharedRef 編譯失敗，修正後 R4／R5 成功；最初 Editor 被開啟中的 Editor 鎖住模組而連結失敗，團隊保存關閉後完整重編。

雙端 R1 逾時；R2 診斷確認球體較寬，原沿用電牆的初始 -600cm 擺位已與實際車體重疊，造成正式觸發前兩次命中、總數4，無法滿足原每車一次的嚴格條件。R2 定位後僅停止本輪專用兩進程，保留日誌。夾具起點改為 -1200cm，沒有放寬2次接觸斷言。最終球體雙端 R3：Host／Client 各 **1/1 通過，0 測試警告／失敗／未執行**。伺服器兩次命中均為 SphereField，方向 -X，眩暈1秒；兩端確認 Contacts=2、HP=100、Stun=0／Shocked=0，Client 記錄43次重模擬／3次回滾請求。使用原生空中夾具、人工擺位與注入速度，Host 出站80ms、NullRHI；不冒充真人實駕、實測 RTT 或正式特效畫面。

本機原始日誌／報告位於 `Saved/ElectricSphere/20261110/`：Native_R1／Native_R2、Sphere_R1／Sphere_R2 及各輪編譯記錄。Saved 不隨 Git 同步，本文件保存可交接的結論與限制。

團隊需檢查：預告期間進出不受擊；啟用時已在球內會受擊；低速／高速／斜向／倒車穿越；持續停留不連續重彈；回彈結束並離開後可再次觸發；兩個球體各自獨立；Loop 結束後不再新增電擊；Over 特效播完才消失。Host、Client、旁觀另一車各測，另測延遲與晚加入。真人手感、正式特效畫面與不同引擎補丁互連未驗收。


最終 Editor R5、Game R3 均完整編譯通過（UE 5.8.2）。既有相機測試的 C4305 警告在前輪完整編譯中保留，沒有為此修改相機。189項原生報告來自 Editor R4，之後只改雙端測試起點／診斷及尾端空白，生產代碼與原生斷言保持。球體雙端 R3 使用最終 Editor R5。

目前代碼與文件留在工作區，未提交／Push；團隊下一步按第3節手動建立 Blueprint、接特效並實測。未建立生成器，未代理修改任何保存資產或 Config。

既有電牆雙端回歸 `HAL.NetworkSmoke.ElectricWall`：Host／Client 各 **1/1 通過，0 測試警告／失敗／未執行**。報告在同一留檔目錄的 Wall_Regression。

## 2026-11-10 調試範圍顏色補充

勾選 Electric Wall → Debug → Draw Debug 後，球體 Begin／Over 顯示紅色範圍，Loop 顯示藍色範圍；使用既有生命週期定時器繪製單幀線條，不新增 Tick。電牆原有青色方框保持。團隊新建 BP 與 MVPB_NetTest 地圖改動保留，代理不編輯資產。本次為調試顯示修改，原189項與雙端結果是前次玩法驗證，不當作本次顏色畫面驗收。

本次顏色修改的 Editor／Game Target 均於 UE5.8.2 編譯通過，日誌為 DebugColor_Editor.log／DebugColor_Game.log。未重跑玩法自動化或聯機；請團隊在場景勾選 Draw Debug 後檢查紅→藍→紅，消失後不殘留線條。
