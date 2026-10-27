# Stun 複用與參數調整指南

更新日期：2026-10-27。適用於目前的共用 Stun C++ 實作。

人工測試可在主機控制台使用 `HALStun <PlayerId> <秒數>`，先用 `HALGASStatus` 查ID；步驟與限制見 [Stun Debug 指令](Stun_Debug_Command_CN.md)。

## 1. 使用方式概覽

機關、道具或技能在**伺服器確認觸發／命中合法之後**，取得目標車輛的 ASC，呼叫：

```cpp
TargetASC->ApplyStun(StunDurationSeconds, SourceActor);
```

每個來源可以設定不同時長，共用同一套 Stun 規則。來源 Actor 不需要擁有 ASC，也不需要為每個來源建立新的 Stun GA 或 GE。

生效後會自動：

- 禁止油門、煞車／倒車、轉向、漂移、球發射、撿球、GAS 能力發動及自救翻身。
- 立即讓目標目前控制的球落下，回到 `Free`，不額外增加拋飛衝量或發射後坐力。
- 保留車輛的慣性、重力、外部碰撞與受擊；不清零速度或凍結剛體。
- 到期後解除限制；換車／失去 Avatar 時沿用既有清理流程。

目前沒有道具庫存／使用框架。未來若道具不是透過 GAS Ability 執行，仍須在道具的伺服器執行入口加入第 5 節的查詢。

## 2. 在機關或道具的 C++ 中接入

以下是加入**既有來源 Actor** 的片段，並非已經建立的新機關類別。

### 標頭：暴露來源自己的時長

將以下宣告放進來源 Actor 類別內：

```cpp
// 預設 3 遊戲秒，僅為這個來源的示例值。
UPROPERTY(EditDefaultsOnly, Category = "Stun",
    meta = (ClampMin = "0.01", Units = "s"))
float StunDurationSeconds = 3.0f;

bool ApplyStunToTarget(AActor* TargetVehicle);
```

`EditDefaultsOnly` 代表可在來源藍圖的 **Class Defaults** 調整，不是在每個關卡實例上個別調整。若來源已有 Data Asset 配置，亦可將此欄位放進既有配置，再把讀到的時長傳給共用入口。

### 實作：取得目標 ASC 並施加

在來源的 `.cpp` 加入：

```cpp
#include "AbilitySystemGlobals.h"
#include "HALAbilitySystemComponent.h"
```

將下列 `AYourMechanism` 替換成實際來源類別名稱：

```cpp
bool AYourMechanism::ApplyStunToTarget(AActor* TargetVehicle)
{
    if (!HasAuthority() || !IsValid(TargetVehicle)
        || !FMath::IsFinite(StunDurationSeconds)
        || StunDurationSeconds <= 0.0f)
    {
        return false;
    }

    auto* TargetASC = Cast<UHALAbilitySystemComponent>(
        UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetVehicle));

    // 限定為此 ASC 目前綁定的車輛，避免誤用舊 Pawn 引用。
    if (!TargetASC || TargetASC->GetAvatarActor() != TargetVehicle)
    {
        return false;
    }

    return TargetASC->ApplyStun(StunDurationSeconds, this);
}
```

呼叫位置應是來源既有的「合法觸發／合法命中」邊界。誰能被命中、每次攻擊是否只算一次、機關冷卻、距離與碰撞檢查，仍由來源自己的 C++ 規則處理；`ApplyStun` 不代替這些判定。

不要在 Tick 中持續施加。如果每幀都傳入 3 秒，結束時間會不斷向後延長，效果可能一直不結束。需要週期施加的機關應明確設定自己的觸發間隔與重複命中規則。

## 3. 可調參數與調整位置

| 項目 | 設定位置 | 說明 |
|---|---|---|
| Stun 時長 | 各來源的 `StunDurationSeconds`，或來源既有 Data Asset | 傳入 `ApplyStun` 的秒數，是目前唯一的 Stun 效果參數 |
| 來源 Actor | `ApplyStun` 第二個引數 | 用於效果來源資訊，通常傳入機關／道具 Actor 的 `this`，不是時長或強度參數 |
| 觸發頻率、冷卻、作用範圍 | 來源自己的配置 | 不屬於共用 Stun 參數，由未來機關／道具實作 |

### 在 Unreal Editor 調整時長

來源完成上述 C++ 接入並編譯後，由團隊手動操作：

1. 開啟實際機關／道具的藍圖類別。
2. 點選 **Class Defaults**。
3. 搜尋 `Stun Duration Seconds`，或展開 `Stun` 分類。
4. 輸入時長，例如 `2.5`，表示 2.5 遊戲秒。
5. 編譯並儲存藍圖，重新開始遊戲確認結果。

**目前尚未建立這個來源類別或欄位，所以現有 Editor 不會憑空出現上述調參位置。** 這些步驟是來源完成接入後的操作方式；共用 Stun 本身已可由 C++ 傳入不同秒數。

### 合法值

- API 接受大於 `0` 的有限秒數，例如 `0.5`、`3`、`8`。
- `0`、負值、NaN 和無限值會被拒絕；傳入 `0` 不表示解除 Stun。
- 範例欄位的 `ClampMin = "0.01"` 是 Editor 輸入限制，並非 API 的額外下限。來源仍應做執行時校驗。
- 目前沒有統一的最長時長限制；平衡範圍由來源設計決定。
- 時間使用遊戲世界計時，不應當作固定牆鐘秒數或固定數量的物理幀。

不要靠修改 `HALStunEffect.cpp` 裡的預設 `1` 秒調整各機關。共用入口會以本次傳入的時長覆蓋 GE Spec 時長，因此該預設值不是機關的調參入口。

## 4. 多來源與重複施加

統一規則為：

```text
新結束時間 = max(原結束時間, 本次施加時間 + 本次時長)
```

例如原本 Stun 剩餘 3 秒：

| 本次傳入時長 | 本次之後的剩餘時間 |
|---|---|
| 1 秒 | 保持剩餘 3 秒 |
| 3 秒 | 由本次起至少再維持 3 秒 |
| 5 秒 | 延長為由本次起 5 秒 |

此規則跨來源共用。機關 A 與道具 B 不會累加出更強的暈眩，也不會把 `剩餘 3 秒 + 新的 5 秒` 算成 8 秒。

`ApplyStun` 的返回值：

- `true`：本次效果施加成功，或已有更長的剩餘時間足以覆蓋本次請求。
- `false`：例如不在伺服器、目標 ASC 沒有有效 Avatar、時長非法，或 GE 施加失敗。

因此不能用 `true` 推斷一定產生新的狀態開始事件。延長期間狀態不中斷，不會重複落球。

## 5. 查詢狀態與阻止未來道具使用

已持有 ASC 時：

```cpp
const bool bStunned = TargetASC->IsStunned();
```

只有目標 Actor 時：

```cpp
const bool bStunned = UHALAbilitySystemComponent::IsActorStunned(TargetVehicle);
```

未來非 GAS 道具的**伺服器執行入口**應在消耗庫存、開始冷卻、生成物件或施加效果之前檢查：

```cpp
if (UHALAbilitySystemComponent::IsActorStunned(UsingVehicle))
{
    return; // 拒絕使用，不消耗道具、不開始本次冷卻。
}

// 通過其他使用條件後，才執行道具。
```

這個查詢對找不到 ASC 的 Actor 會返回 `false`，所以它不能取代使用者、車輛、存活、比賽階段或庫存合法性檢查。

若道具經由現有 GAS Ability 激活，ASC 已有 Stun 激活阻擋。既有加速／免疫等已生效 GE 繼續各自計時，Stun 不會自動驅散它們；未來若有長時間執行中的道具 Ability，其「中途被 Stun 是否取消」仍須由該道具定義，不能把激活阻擋當成自動取消所有既有行為。

## 6. 剩餘時間與表現通知

```cpp
#include "HALGameplayTags.h"

const float RemainingSeconds = TargetASC->GetEffectTimeRemaining(
    HALGameplayTags::State_Stun);
```

| 返回值 | 含義 |
|---|---|
| `0` | 沒有這個標籤，或查到的剩餘時間已到零 |
| 大於 `0` | 本端可查到的剩餘遊戲秒數 |
| `-1` | 本端沒有可用的完整時長資料，或存在無限效果來源；不能解讀成已解除 |

目前玩家 ASC 使用 Mixed 复制。遠端觀察者可能知道對方有 `State.Stun`，但查不到完整 GE 時長。判斷是否仍被 Stun 應使用 `IsStunned()`／`IsActorStunned()`，不要用 `RemainingSeconds <= 0` 取代狀態判斷。

UI／特效可在 C++ 訂閱 ASC 的 `State.Stun`、`EGameplayTagEventType::NewOrRemoved` 事件；綁定時先讀取目前狀態，解除綁定／結束時移除委派。重複延長不會產生假的結束／開始通知。這份指南沒有新增 UI、特效或藍圖 Event Graph。

## 7. 需要提前解除時

若未來有清除 Stun 的設計，可在**伺服器確認解除合法後**，使用現有 GAS 移除接口：

```cpp
TargetASC->RemoveActiveEffects(
    FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(
        FGameplayTagContainer(HALGameplayTags::Effect_Stun)));
```

上例僅移除帶有 `Effect.Vehicle.Stun` 效果標籤的活動 GE，不應使用「移除所有效果」來解除暈眩。這是接入範例，沒有新增驅散道具或玩家可發送的解除 RPC。

## 8. 接入注意事項

- 呼叫目標車輛對應的 ASC，而不是來源自己的 ASC。玩家 ASC 在 PlayerState 上，建議經由車輛的 Ability System Interface 取得，如第 2 節所示。
- 不直接添加／移除 `State.Stun` loose tag，也不另設一份 `bStunned` 或獨立倒數計時器，避免狀態來源不一致。
- 不為不同來源各建一套 GE 並繞過 `ApplyStun`，以免破壞共用時長合併規則。
- 不直接修改 `HALMovementEffectBridge`、物理快照或球約束來施加 Stun；共用狀態已接好相關限制。
- 目前 `ApplyStun`／查詢接口是原生 C++ 方法，未標記 `BlueprintCallable`，不能直接在現有 Blueprint Event Graph 搜尋並呼叫。核心接入由 C++ 完成，藍圖只調來源欄位與表現資產。
- 客戶端不直接施加 Stun。需要玩家使用道具時，提交合法的道具使用請求，由伺服器自行判定命中並呼叫共用入口。
- 所有聯機端使用相同版本完整編譯。Stun 已將運動時間線格式升至 2，不與舊程式混連。

## 9. 最小手動驗證

來源接入後，在 Host 與 Client 分別驗證：

1. 持球時中 3 秒 Stun：立即落球，不能駕駛、發射、拾取新球、發動能力或自救。
2. 車子仍會滑行、落下或被撞走；Stun 本身不扣血。
3. 尚剩 3 秒時施加 1 秒：不縮短。再施加 5 秒：從本次起維持 5 秒，不重複落球。
4. 到期後可以恢復操作。持續按住油門會恢復駕駛，Stun 期間按過的發射／能力不會自動補發。
5. 換車／退出後不殘留；其他車仍能爭奪掉落的球。

這些是來源接入後要做的測試，尚未因本文件新增而執行。既有共用 Stun 的規則與原生驗證見 [Stun 實作說明](../Combat/MVPB_Stun_State_CN_v0.1.md)及[2026-10-27 驗證記錄](../Combat/MVPB_Stun_Verification_20261027.json)。
