<!-- Current rules updated 2026-10-29. Historical verification below remains historical. -->
# 成對電柱與電牆機關

日期：2026-10-27（Asia/Tokyo）。本輪起點為乾淨 `main / 0b28303d1bb02a51fd101901bbb52831baa27353`，本機 UE 5.8.2-56702186。先前共用 Stun 與 debug 指令已提交，本輪保留既有實作並新增機關。

後續方向修正：用戶要求往東撞時往西擊退，現行方向規則已改為反向進入位移；不是原先的牆面法線／柱子徑向。修正後Editor／Game完整構建、普通87/87、最終雙端各1/1通過。旋轉90°牆的雙端測試確認實際向西速度峰值125cm/s；團隊1158份保存資產全同，尚待真人方向／力度複測。首輪聯機夾具誤選地圖既有電牆而超時的記錄與修正保留，見[方向驗證](MVPB_Electric_Wall_DirectionFix_Verification_20261027.json)。第6節原86項為第一階段歷史，不當作修正後新驗證。

2026-10-29改為獨立Bounce Back。舊Impact Tier、Push Preset及Retrigger Interval已移除，現有Blueprint重新完整編譯後在Config調整Bounce Back Distance／Seconds即可。代理未修改資產；實體柱子的自然碰撞仍可能改變姿態，回彈本身不加旋轉。Stun與回彈時間獨立，回彈執行時暫停自訂駕駛／翻身扭矩，完成後恢復原狀態規則。最新驗證見[MVPB_Electric_Wall_BounceBack_Verification_20261029.json](MVPB_Electric_Wall_BounceBack_Verification_20261029.json)。

## 1 本輪功能

新增原生 C++ Actor `AHALElectricWallGimmick`。一個 Actor 管理兩根柱子與中間電牆，車輛接觸任一表面後，由伺服器依序執行：

```text
合法車輛接觸
→ 獨立 Bounce Back／Network Physics 水平回彈
→ 共用 ASC.ApplyStun
→ 標記 shocked 並通知表現元件
```

- 柱子具有簡單 Capsule 實體碰撞。柱子與電牆均優先按車輛進入前的水平移動方向反推：往東進入則向西推，往西則向東，斜向同樣反推；與車頭朝向或牆體旋轉無關。
- 優先使用兩次掃描間的位移，避免柱子自然碰撞已反轉當前速度而判錯方向。沒有可用位移時才參考水平速度；車輛完全靜止時才退回柱子向外／牆面側向的幾何預設。
- 同一次連續重疊保留進入方向且只觸發一次；確實離開後再進入才重新取方向。
- 電牆是 Query Only 觸發區。回彈開始時取消原本水平速度，沿來向反向後退並線性減速；不改垂直速度及角速度。距離是無阻擋的目標距離，碰撞、阻尼或其他外力可縮短實際距離，不傳送或穿越障礙。
- 不造成接觸傷害，不修改 HP／位置；水平速度由獨立權威物理動作控制。
- 整組機關對每輛車每次進入只觸發一次，持續重疊不重複。回彈完成且取得一次完全離開三個表面的掃描後，重新進入才能觸發。高速穿越後回彈穿回同一牆屬同一次互動，避免自行往返觸發。
- 只作用於目前被控制、配置／ASC／HP就緒且存活的玩家車輛。
- 有比賽 GameState 時僅在 Playing 生效；無比賽 GameState 的原生測試世界仍可驗證。
- 球穿過電牆不附加任何效果或傷害，球相關玩法本輪後置。球仍可能與實體柱子發生普通物理碰撞。

## 2 初始參數

在機關輕量 Blueprint 子類的 Class Defaults → Electric Wall → Config 調整。

| 欄位 | 初始值 | 意義 |
|---|---:|---|
| Pillar Spacing | 1000 cm | 兩柱中心距離，沿 Actor 本地 X 軸排列 |
| Pillar Radius | 60 cm | 柱子碰撞半徑 |
| Height | 300 cm | 柱子與電牆高度，必須至少為柱子直徑 |
| Wall Half Thickness | 25 cm | 電牆厚度的一半，沿本地 Y 軸 |
| Contact Margin | 10 cm | 柱子接觸判定容差，不擴大柱子的實體碰撞 |
| Scan Interval | 0.05 s | 伺服器檢查間隔，最小0.01秒 |
| Bounce Back Distance | 100 cm | 無阻擋目標回彈距離，範圍大於0至1000公分 |
| Bounce Back Seconds | 0.3 s | 線性減速回彈時間，範圍0.05至2秒；距離相同時越短越快 |
| Stun Seconds | 1 s | 每次接觸傳給共用Stun的遊戲秒數 |

以上為原生測試初值，不是最終平衡值。正有限尺寸／時間會做執行時校驗；柱距須大於兩倍半徑。無效配置會停用權威機關處理並記錄錯誤。

Stun沿用較晚結束時間：剩餘時間較長時不縮短。Impact免疫／抗性／降檔不影響Bounce Back，也不免除Stun。Stun本身會立即解除持球，即使衝擊被免疫也能落球。

實體柱子的普通 Chaos 碰撞仍存在。機關附帶的 `ContactImpactProfile` 使用 FixedTier／None，避免普通接觸再附加第二次規則衝擊；此元件只用來抑制普通柱碰撞額外附加的Impact；機關回彈不經CombatResolver／ImpactReceiver，也不讀取Impact檔位或免疫。請保留此元件的原生配置。

## 3 Unreal Editor 手動建立與放置

代理沒有修改或建立 `.uasset`／`.umap`。團隊完成下列操作：

1. 使用本輪完整編譯的版本重開 Editor。首次新增反射類別不使用 Live Coding 代替完整編譯。
2. 在 Content Browser 建立 Blueprint Class，選 All Classes，搜尋並選擇 `HALElectricWallGimmick` 作為父類。
3. 建議命名為 `BP_ElectricWall_Test`，放在團隊選定的機關資料夾。
4. 在 Class Defaults 調整 Config。優先使用欄位調整距離／高度，先以 Actor Scale `(1,1,1)` 測試；原點為兩柱中央的地面位置，本地 X 是連線方向，Y 是穿牆方向。
5. Components 的 `PillarVisualA`、`PillarVisualB` 指定柱子 Mesh；`WallVisual` 指定電牆 Mesh／材質。這三個 Mesh 僅供顯示，保持 No Collision、不模擬物理。
6. 以 `PillarA`／`PillarB` 的 Capsule 和 `ElectricField` 的 Box 為準，手動調整模型相對位置與縮放。模型不會依 Config 自動改變自身尺寸；柱子模型若原點在底部，可相對柱子中心向下偏移半個 Height 後再核對。
7. 不移動原生 Capsule／Box 來改核心尺寸；改 Config，確保顯示與權威幾何一致。不在 Event Graph 重寫觸發、回彈或Stun。
8. 將 Blueprint 放入測試地圖，讓 Actor 原點對齊地面，旋轉 Yaw 調整方向，編譯／儲存資產。
9. 初測可在機關 Class Defaults 開啟 `Draw Debug`，主機會畫出實際查詢範圍；確認後關閉。原生 Actor 未指定 Mesh 時沒有正式外觀，不要把無模型視為機關已完成美術接線。

同一個 Blueprint 實例就是一組，不需要另外放兩根互相尋找的柱子，也不依賴關卡Actor名稱。

`Configure(FHALElectricWallConfig)` 僅供伺服器 deferred spawn 在 BeginPlay 前設定。幾何／規則於本步作為固定機關使用，不在遊戲途中熱改尺寸或掃描頻率。

## 4 shocked 表現接口

新增 `UHALShockedPresentationComponent`，已由 `ATestVehiclePawn` 原生組裝。團隊現有車輛子類繼承後可看到 `ShockedPresentation` 元件。

- `IsShocked()`：目前表現狀態。
- `OnShockedChanged(bool bActive)`：狀態實際改變時通知一次。
- `true` 時可啟動電流粒子／材質；`false` 時關閉或淡出。只接表現，不從事件執行傷害、Stun或物理。
- 團隊在車輛 Blueprint 綁定事件後，先查一次 `IsShocked()` 設定初始表現，避免中途綁定時漏掉已存在的狀態。
- 目前沒有新增 Niagara、材質、動畫、UI或Event Graph，也不聲稱已有電流畫面。

邏輯上，電擊先施加 Stun，成功後呼叫 `ASC.MarkShockedForCurrentStun(Source)`。原生 `UHALShockedEffect` 只授予 `State.Visual.Shocked` 表現標籤，不增加任何操作限制或屬性變化。

標記在此次Stun最終結束／驅散時移除：

- 原本有更長Stun或後續其他來源延長Stun，shocked一起保留到最終Stun結束。
- 同一Stun中再次電擊不重複建立表現狀態，也不製造假的開始／結束通知。
- 單純Stun（例如 `HALStun` 指令）不會自動產生shocked；必須先有電擊標記。
- 機關被銷毀不會把已施加Stun提前解除；shocked繼續跟隨Stun。
- 換Avatar／退出沿用既有Avatar作用域清理，表現元件解除綁定時也恢復非shocked。

此標記以Avatar作用域Infinite GE表示，由Stun結束事件負責清除，不另外運行倒數計時器。GAS的複製標籤讓擁有者及遠端觀察者各自驅動表現；晚綁定先讀取當前標籤，物理重模擬不會重新施加GE或播放一次性觸碰事件。

## 5 伺服器接觸與同步

Actor不新增Tick。伺服器定時查詢三個表面的PhysicsBody重疊，僅接受玩家車輛；不依賴兩端都開啟Generate Overlap Events。兩次掃描間另以車輛實際碰撞形狀做Sweep，補查高速越過薄電牆的情形。每組保存車輛上一個位置、接觸鎖與回彈完成時間，失效引用會清理。

回彈使用獨立FVehicleBounceBackAction及可回溯FVehicleBounceBackState，走可靠Authority Network Physics動作；Stun走既有GAS與運動歷史；shocked走表現標籤。客戶端不自行命中、不回報傷害、不施加Stun。Config及接受次數診斷會複製，晚加入可重建幾何；本輪未做實際晚加入／重新相關性的完整真人驗收。

除錯控制台：

```text
hal.ElectricWallLog 1
HALGASStatus
```

前者記錄來源、目標、接觸面、回彈距離／時間／方向、Stun剩餘時間及shocked是否成功；`HALGASStatus`繼續顯示Stun。回彈不產生Impact物理日誌。

## 6 歷史驗證與目前待測

完整構建、測試與保存資產證據見 [驗證記錄](MVPB_Electric_Wall_Verification_20261027.json)。原生回歸涵蓋配置／方向、三表面去重、免疫與Stun獨立、重疊／完整高速穿越、遊戲時間後重觸發、shocked跟隨延長／驅散／換Avatar及來源銷毀。

本輪實際為UE5.8.2 Editor／Game完整構建成功，普通86/86、實際雙端Host／Client各1/1，報告均0警告／失敗。雙端夾具的兩次機關接觸都為Light＋Short、HP100不變，兩端Y速度峰值125cm/s，Stun與shocked自然結束；Client完成點37次實際重模擬及3次橋接回滾請求。主機出站80ms注入、60FPS上限與空中隔離只驗證原生鏈路，並非實測RTT或正式駕駛距離。1157份保存資產前後逐份SHA-256相同；兩專用聯機程序已退出。本輪未提交／Push。

團隊接線後仍須做實際地圖與畫面測試：

1. Host與Client分別以低速、高速、斜向、倒車接觸兩柱／兩側電牆；確認方向可讀、小幅擊退、Stun及落球。
2. 同時碰柱與牆、持續停留超過Stun時長，確認只回彈一次；回彈完成並完全離開後再進入，確認能再次觸發。
3. Impact免疫／降檔不應改變回彈；既有加速與長Stun同時存在時，確認回彈與狀態各自正確。
4. 確認shocked開始／結束在本車與觀察車都正確，晚綁定不殘留；實際視覺資產仍由團隊製作與驗證。
5. 球通過電牆沒有強化；柱子普通碰撞、牆角與車輛連續碰撞仍需真人確認。

本輪不包含最終美術、球穿牆加成、GAS Stun免疫新規則、動態機關生成、大廳功能或F4完整網路擾動／四機驗收。

## 7 Bounce Back 調整與手測（2026-10-29）

重新開啟完整編譯後的Editor，打開既有BP_ElectricWallGimmick → Class Defaults → Electric Wall → Config：

- Bounce Back Distance：後退目標距離，預設100公分。
- Bounce Back Seconds：後退並減速所需時間，預設0.3秒。相同距離下時間越短越有彈性。
- Stun Seconds：操作封鎖時間，獨立於回彈時間。預設1秒。

舊Impact Tier、Push Preset、Retrigger Interval不再是這個機關的調參入口。團隊手動Compile／Save Blueprint，確認擺在地圖的實例繼承新欄位；不要用Live Coding遷移這次反射結構。

先以100公分／0.3秒測低速、高速、斜撞、倒車，再以50／150公分比較距離；空中接觸檢查原有下墜／上升仍持續，回彈不額外翻車。距離從權威回彈開始時的位置計算，不是距離牆面的最終淨空；掃描間隔可能讓高速車先進入或穿過薄牆，沒有傳送回接觸點。真實碰撞／阻尼可使實際距離較短。實體柱子造成的自然旋轉仍保留，須在正式地圖做真人確認。
