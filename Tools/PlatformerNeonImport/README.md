# Platformer 10 Neon 匯入（2026-10-23）

## 使用

- Unreal Content Browser：`/Game/Imported/Platformer10Neon/Prefabs`，分類拖入已組合Static Mesh。
- 原始模型：`/Game/Imported/Platformer10Neon/Meshes`。
- 材質／貼圖：同根下`Materials`、`Textures`。
- 總覽：`/Game/Imported/Platformer10Neon/Maps/Prefab_Overview`；來源部分Prefab很大，總覽間距依最大模型決定，可選取個別物件按F聚焦。

產物共267份：124原始模型＋124組合Prefab Static Mesh、10材質、8貼圖、1張總覽。Prefab組合保留可見子物件幾何、根縮放、相對變換、材質槽與放置原點；根世界位置／旋轉歸零。Unity Prefab轉為Static Mesh，不是Blueprint，沒有移植Unity MonoBehaviour、Collider、Animator或遊戲行為；4個SkinnedMesh Renderer烘焙為靜態姿態。動畫需求需另接Unreal。

材質保留底圖／色彩／UV、金屬度／粗糙度，額外轉接Emission Map×原HDR Emission Color；透明材質使用底圖Alpha×色彩Alpha。7個發光、5個透明材質經獨立重載檢查，Unity自訂Shader、動畫化霓虹、玻璃折射、Bloom與渲染管線不保證等效。未完成真實渲染觀感驗收。

網格碰撞採Complex As Simple供靜態場景使用。活動機關或模擬剛體須另建簡單碰撞與玩法；裝飾／玻璃也應按需求調整碰撞。

## 重現與證據

`prepare.py`只複製使用者指定的Textures/Materials/Meshes/Prefabs到Saved隔離Unity專案，拒絕既有Stage/目的根；不複製或執行來源腳本。使用Unity6000.4.10f1執行自有`PrefabExporter.Export`，之後用UE5.8.2官方Python匯入API與`verify_unreal.py`。沒有修改正式C++、Config或插件設定；所需Python插件以命令列臨時啟用。

所有248網格尺寸與槽數檢查通過，最大邊界誤差0.001563cm。另一進程重新載入全部模型／材質／貼圖，核對124個組合、位置及材質／發光／透明／靜態碰撞引用，0錯誤／0警告。匯入階段0錯誤／840警告，包含新資產建立前查詢、缺少平滑群組及退化切線，非無警告匯入。887份既有文件與546份來源文件的SHA256保持相同。

原始中間檔／日誌在`Saved/PlatformerNeonMigration`，不隨Git；共享摘要見[驗證JSON](../../Docs/Imports/Platformer10Neon_Verification_20261023.json)。未執行新玩法、打包、效能、真人視覺驗收，未Commit/Push。
