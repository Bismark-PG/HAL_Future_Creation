# Platformer 11 Space 匯入（2026-10-23）

## 在 Unreal 使用

Content Browser 開啟 `/Game/Imported/Platformer11Space/Prefabs`，依原分類把 `SM_..._PF_...` 拖進場景。全部100個組合的總覽位於 `/Game/Imported/Platformer11Space/Maps/Prefab_Overview`。

本次產物：100原始模型、100個Prefab組合靜態網格、12材質、5貼圖、一張總覽地圖。來源僅為使用者指定的四資料夾；沒有執行來源專案腳本或改寫來源。

Prefab保留可見幾何、根縮放、子物件相對變換、材質槽與放置原點；根位置及旋轉歸零。這是Static Mesh，不是Blueprint。Unity腳本、動畫、燈光、粒子與Collider不移植；需要活動子物件／機關時須由團隊另建Unreal設定。材質近似底圖、色彩、UV、金屬度與粗糙度，自訂shader、透明及發光不保證一致。

碰撞使用Complex As Simple，只適合靜態環境。動態剛體需另設簡單碰撞；透明裝飾也應按用途檢查碰撞。

## 驗證

UE5.8.2實際匯入成功，200網格邊界及槽數通過；另一進程重載200網格、12材質、5貼圖，驗證100個組合引用及位置，0錯誤／0警告。邊界最大誤差0.000344cm。原有667文件、來源446文件SHA256保持相同。

匯入階段577項警告包含尚未建立的新資產查詢、缺少平滑群組與退化切線；沒有完成渲染外觀驗收。請開啟總覽，檢查正反面、UV、玻璃／發光／全息材質，並在實際地圖檢查尺寸及碰撞。未執行玩法、效能或打包驗收，無C++修改或新編譯。

## 重現及留檔

`prepare.py`建立Saved隔離Unity專案，拒絕覆蓋既有Stage。Unity6000.4.10f1批次執行`PrefabExporter.Export`；之後UE以PythonScriptPlugin與EditorScriptingUtilities臨時啟用參數，執行`import_unreal.py`及`verify_unreal.py`。工具只寫入新資產根，總覽存在時拒絕覆蓋。重新匯入須先檢查人工修改，不應直接刪除重跑。

中間檔和原始日誌在`Saved/PlatformerSpaceMigration`，不用於版本同步；資產、工具、文件應正常納入Git。本次未Commit／Push。

詳細證據：[驗證JSON](../../Docs/Imports/Platformer11Space_Verification_20261023.json)。
