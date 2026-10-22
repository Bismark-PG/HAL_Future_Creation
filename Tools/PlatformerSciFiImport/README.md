# Platformer 14 Sci-Fi 匯入（2026-10-22）

來源：`D:/Unity Shit/Game_Jam_2026_07/MAP_Making/Assets/ithappy/Platformer_14_Sci-Fi` 的 Prefabs、Textures、Materials、Meshes。來源只讀；沒有開啟或改寫原 Unity 專案。

## 在 Unreal 使用

1. Content Browser 開啟 `/Game/Imported/Platformer14SciFi/Prefabs`。
2. 依原分類選擇 `SM_..._PF_...` 靜態網格，直接拖到你的地圖。材質已指定，可重複放置、旋轉、縮放。
3. `/Game/Imported/Platformer14SciFi/Maps/Prefab_Overview` 是全部 139 個組合的總覽。
4. 原始 139 個模型在 `Meshes`，15 個近似材質在 `Materials`，9 張貼圖在 `Textures`。

Prefab 輸出是合併可見幾何的 Static Mesh，不是 Blueprint。保留 Prefab 原點、根縮放、可見子物件相對變換與材質槽；根的位置與旋轉歸零，方便放置。子物件不再能各自活動。Unity Collider、MonoBehaviour、Animator、Light 和其他非網格行為不轉移；詳細省略項目見驗證 JSON。未讀入指定四資料夾之外的動畫或腳本依賴，因此 MissingScript 是隔離匯出的預期限制。

如果需要會動的門、陷阱、收集物，須另行建立 Unreal 邏輯。依 AGENTS.md，代理不建立或修改 Blueprint；團隊可手動建立 Actor Blueprint，新增 Static Mesh Component，指定這裡的網格，再以 C++ 基類提供玩法。

## 材質與碰撞限制

材質使用 Unity 序列化的基礎色、底圖、UV 縮放／偏移、金屬度、粗糙度近似；自訂 Shader、透明特效、發光強度、動畫材質不保證一致。OBJ 匯入保留法線，但日誌有平滑群組／退化切線警告；需在實際鏡頭確認外觀。

新網格採用 Complex As Simple，適合靜態場景碰撞；不能直接當模擬物理車輛／動態剛體的碰撞根。透明裝飾也可能有幾何碰撞，正式關卡要依用途停用或重設。未進行駕駛碰撞、效能或打包驗收。

## 工具及重新執行

- `prepare.py`：只複製指定四資料夾與 meta 至 Saved 的隔離專案，並記錄 SHA256。已存在 Stage 時拒絕覆蓋。
- `PrefabExporter.cs`：Unity 6000.4.10f1 批次執行 `PrefabExporter.Export`，匯出 OBJ、材質描述、Prefab 組合與省略項目。
- `import_unreal.py`：UE 5.8.2，以 PythonScriptPlugin、EditorScriptingUtilities 暫時啟用參數執行。只寫新資產根目錄，不修改專案插件設定。總覽已存在時拒絕重建，避免覆蓋人工修改。
- `verify_unreal.py`：重新載入並檢查所有資產，根據來源 renderer 對照指定原始模型材質。三個無直接 Mesh 物件對應的來源，以同名且頂點／三角形／材質槽數一致的單體 Prefab 對照。
- `preview_unreal.py`：實際 Editor 渲染總覽，不儲存預覽相機。

中間資料／原始日誌在 `Saved/PlatformerSciFiMigration`，不納入 Git。首次驗證因假設同名原始模型與 Prefab 頂點數相同而失敗；改為實際 Mesh 引用後最終驗證通過，首次失敗日誌保留。

本次沒有修改 C++、沒有新增編譯或玩法測試。狀態記錄中的其他設備為 UE5.8.3，本機實際 UE5.8.2；此差異已記錄，團隊仍應統一版本。

詳細驗證：[Platformer14SciFi_Verification_20261022.json](../../Docs/Imports/Platformer14SciFi_Verification_20261022.json)。資產與工具需一起納入團隊正常版本控制；本次不自動 Commit 或 Push。

## 本次驗證結果

278網格、15材質、9貼圖皆已重新載入；139個總覽組合的位置／引用檢查通過。模型邊界最大誤差0.000161 cm。原有364份資產與來源612檔案SHA256未變。兩次背景渲染程序在輸出截圖前退出，原因尚未確定；沒有完成本次畫面驗收，請在Editor開啟總覽檢查。匯入日誌的平滑群組／切線警告仍保留。
