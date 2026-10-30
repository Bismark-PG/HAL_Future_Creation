<!-- Current rules updated 2026-10-30. Historical verification below remains historical. -->
# 成對電柱與電牆機關

日期：2026-10-27（Asia/Tokyo）。本輪起點為乾淨 `main / 0b28303d1bb02a51fd101901bbb52831baa27353`，本機 UE 5.8.2-56702186。先前共用 Stun 與 debug 指令已提交，本輪保留既有實作並新增機關。

後續方向修正：用戶要求往東撞時往西擊退，現行方向規則已改為反向進入位移；不是原先的牆面法線／柱子徑向。修正後Editor／Game完整構建、普通87/87、最終雙端各1/1通過。旋轉90°牆的雙端測試確認實際向西速度峰值125cm/s；團隊1158份保存資產全同，尚待真人方向／力度複測。首輪聯機夾具誤選地圖既有電牆而超時的記錄與修正保留，見[方向驗證](MVPB_Electric_Wall_DirectionFix_Verification_20261027.json)。第6節原86項為第一階段歷史，不當作修正後新驗證。

2026-10-29改為獨立Bounce Back。舊Impact Tier、Push Preset及Retrigger Interval已移除，現有Blueprint重新完整編譯後在Config調整Bounce Back Distance／Seconds即可。代理未修改資產；實體柱子的自然碰撞仍可能改變姿態，回彈本身不加旋轉。Stun與回彈時間獨立，回彈執行時暫停自訂駕駛／翻身扭矩，完成後恢復原狀態規則。最新驗證見[MVPB_Electric_Wall_BounceBack_Verification_20261029.json](MVPB_Electric_Wall_BounceBack_Verification_20261029.json)。

## 2026-10-30 Client本地回彈預測（代碼與原生驗證完成）

用戶回報Client自己撞牆會先繼續前進再彈回，並要求解決。現僅Client的LocallyControlled車輛掃描同一中間ElectricField；柱子及遠端車輛不做本地預測。車輛／HP／比賽與Network Physics時序就緒後，按本地進入方向把BounceBack以PredictedAutonomousOnly動作排入下一物理幀，不在GT直接改速度，不傳送「命中／傷害」RPC給Host。

Host仍用自身Overlap／Sweep獨立判定並可靠發布權威BounceBack，GAS Stun／shocked與脫球仍只由Host產生。每座牆由Host配發複製的PhysicsSourceId，避免Legacy網路模式對動態Actor沒有Iris穩定雜湊的限制。Client依最近收到的權威BounceBack序號產生下一個候選，來源碼與序號共同用於既有動作配對／去重；同一車未收到涵蓋候選幀的權威歷史前，不排第二個未解決候選。State Compare與ApplyData只觀察真正接收的權威歷史，不把本地重播當作Host確認；錯誤方向／不存在的接觸可由原BounceBack歷史比較及回滾恢復。

Client的本次接觸鎖定至少涵蓋max(BounceBackSeconds,StunSeconds)加掃描間隔，並仍需完全離開樣本才重設，防止丟包／位置校正造成同次接觸重複預測。這不是預測GAS Stun，也不會本地修改球權；Stun與電擊視覺依舊可能晚於物理回彈抵達。沒有有效Config／PhysicsSourceId／物理時序時，保持等待權威結果的回退。

本機UE5.8.2-56702186最終Editor／Game完整構建成功；最終普通106/106。150ms固定延遲的接受／誤判撤銷均雙端通過後，增加正式.05s掃描、Host出站150ms、±40ms抖動、10%丟包，最終兩組四份雙端報告均各1/1、0測試警告／失敗／未運行。接受夾具Client Count1／Early1／Velocity1、兩車Host接觸2次、最終序號1且inactive；拒絕夾具Client曾有實際反向速度，Host接觸0次，最終序號0且inactive，HP100。最終Accept Ack566／PredFrame428，Reject Ack609／PredFrame464。60FPS／NullRHI、空中隔離夾具；接受測試將Client運動刻意提前.2個伺服器時鐘秒以代表本地模擬領先，檢查在收到Stun之前的實際負向速度，不是實測RTT／真人駕駛或延遲百分位基準。

首輪動態Actor來源雜湊Ensure後中止，改為複製來源碼；首輪丟包接受測試Client候選Count2違反嚴格斷言，補上上述本地接觸鎖定後原Count1斷言通過。失敗日誌／報告保留，未放寬測試。1172份保存資產相對柱子環節基線逐份SHA256全同；無BP／地圖改動，未改History線上序列化格式。詳見[預測驗證證據](MVPB_ElectricWall_Prediction_Verification_20261030.json)。編譯既有C4305等警告保留。未提交／Push。

團隊下一步：兩端使用本次同版程式並重新啟動遊戲，用Client駕車碰中間電場，比較初次回彈、連續貼牆／離開再進入、純柱與柱牆交界；再測多牆及持球。無需新增藍圖節點。很高延遲、初始資料未送達或兩端碰撞判斷不同時仍可能校正，不宣稱所有網路情況完全無感；多牆同時接觸／Avatar更換／極端丟包斷線／四機與F4仍待後續矩陣。

以下柱子段落保留當時環節紀錄；其中「預測尚未實作」由本節後續實作取代。

## 2026-10-30 柱子改為一般牆壁（編譯與原生驗證完成）

用戶確認本地回彈預測方向，並指定兩根柱子只當一般牆壁，不觸發shock。本輪先調整接觸規則：只有中間ElectricField可觸發專屬Bounce Back、Stun與shocked；柱子保留Capsule實體阻擋，恢復共用一般環境碰撞的量測檔位／上限。沿用既有ContactImpactProfile子元件名稱，在BeginPlay覆寫舊保存的FixedTier／None為AutomaticCap及共用EnvironmentTierCap，無需代理修改BP資產。

伺服器Overlap／高速Sweep均只收中間ElectricField；ApplyContact也拒絕柱子，避免旁路誤觸發。純柱子不消耗電牆進入鎖；車體同時碰到中間電場時，仍屬合法電牆接觸。普通牆壁碰撞可能產生原有姿態／衝擊／脫球，無電牆專屬回彈、Stun或shocked。Contact Margin保留舊序列化欄位但已無柱子電擊用途，避免保存資產遷移破壞。

新增純柱子Overlap／Sweep不電擊、直接兩柱入口拒絕、碰撞阻擋與共用環境檔位檢查，保留電牆快速穿越及每次進入去重。此項不等於Client本地預測完成：本地預測仍須接觸配對／確認或撤銷／回滾測試，目前尚未實作。本機UE5.8.2-56702186 Editor／Game完整編譯成功、普通106/106、真實Host／Client各1/1，3份報告均0測試警告／失敗／未執行。普通覆蓋兩柱入口拒絕／純柱Overlap與Sweep不電擊／實體阻擋及共用環境檔位；雙端沿用空中原生電場夾具，每車接觸一次、HP100、Stun／shocked結束，Client33次重模擬／3次橋接回滾。60FPS／NullRHI、Host出站80ms注入不是實測RTT，也不是新柱子真人畫面驗收。保存資產1172份前後哈希全同，未修改BP／地圖；編譯既有C4305警告保留。詳見[本輪驗證](MVPB_ElectricWall_Pillars_Verification_20261030.json)。未提交／Push。

## 1 本輪功能

新增原生 C++ Actor `AHALElectricWallGimmick`。一個 Actor 管理兩根柱子與中間電牆，車輛接觸中間電牆後，由伺服器依序執行：

```text
合法車輛接觸
→ 獨立 Bounce Back／Network Physics 水平回彈
→ 共用 ASC.ApplyStun
→ 標記 shocked 並通知表現元件
```

- 柱子具有簡單 Capsule 實體碰撞。只有中間電牆的專屬回彈按車輛進入前的水平移動方向反推：往東進入則向西推，往西則向東，斜向同樣反推；與車頭朝向或牆體旋轉無關。
- 優先使用兩次掃描間的位移，避免柱子自然碰撞已反轉當前速度而判錯方向。沒有可用位移時才參考水平速度；車輛完全靜止時才退回柱子向外／牆面側向的幾何預設。
- 同一次連續重疊保留進入方向且只觸發一次；確實離開後再進入才重新取方向。
- 電牆是 Query Only 觸發區。回彈開始時取消原本水平速度，沿來向反向後退並線性減速；不改垂直速度及角速度。距離是無阻擋的目標距離，碰撞、阻尼或其他外力可縮短實際距離，不傳送或穿越障礙。
- 不造成接觸傷害，不修改 HP／位置；水平速度由獨立權威物理動作控制。
- 整組機關對每輛車每次進入只觸發一次，持續重疊不重複。回彈完成且取得一次完全離開中間電場的掃描後，重新進入才能觸發。高速穿越後回彈穿回同一牆屬同一次互動，避免自行往返觸發。
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
| Contact Margin | 10 cm | 舊序列化欄位保留；柱子已無電擊接觸容差用途 |
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
