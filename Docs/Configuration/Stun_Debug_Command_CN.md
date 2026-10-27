# Stun Debug 指令

更新日期：2026-10-27。用於開發版本的人工測試，直接沿用共用 Stun 規則，不需要新增機關或藍圖。

## 使用步驟

1. 使用本次完整編譯的版本啟動遊戲。
2. Host／Client 進入 `MVPB_NetTest` 後，由主機開始比賽，進入 `Playing`。
3. 在**主機視窗**按 `~` 開啟控制台，執行 `HALGASStatus`。
4. 從輸出找到目標車輛的 `PlayerId`。
5. 輸入 `HALStun <PlayerId> <秒數>`。

例如目標 `PlayerId` 是 `257`，要給予 3 秒 Stun：

```text
HALStun 257 3
```

要改成 1.5 秒：

```text
HALStun 257 1.5
```

以上 ID 只是範例，請以該局 `HALGASStatus` 的輸出為準。這是 PlayerState 的實際 ID，不是玩家索引、車輛名稱或畫面排列順序。主機可以選自己或其他玩家；客戶端輸入此指令會被拒絕，不會送出任意目標的 Stun RPC。

## 效果與重複施加

指令生效後，目標立即落球，並禁止駕駛、發射、撿球、GAS能力發動及自救，保留慣性和外部碰撞；到期自動恢復。非GAS道具仍須由未來道具系統接上共用狀態查詢。

時長使用遊戲秒數，必須是大於0的有限值，`0`不是清除指令。重複施加採較晚結束時間：剩餘3秒時施加1秒不縮短，施加5秒則延長為由本次起5秒。

## 確認是否成功

控制台與畫面會顯示本次接受／拒絕結果。成功訊息包含：

```text
HALStun: Accepted=1 PlayerId=257 ... Requested=3.000s Stunned=1 Remaining=3.000s ...
```

`Remaining`可能比本次請求長，代表原本較長的Stun被保留。再次執行 `HALGASStatus` 可查看：

- `Stunned=1`：仍在Stun。
- `StunRemaining`：本端可取得的剩餘秒數。
- 遠端觀察者在Mixed複製下可能看到`StunRemaining=-1`，表示時長資料不可取得，不表示已解除；請以`Stunned`判斷狀態。

## 停用與拒絕條件

主機可輸入以下指令停用後續debug施加：

```text
hal.StunDebug 0
```

恢復使用：

```text
hal.StunDebug 1
```

停用不會清除已生效Stun，也不會禁止正式機關呼叫共用接口。開關只影響此debug指令，預設為1。

下列情況會拒絕施加：客戶端／非本地主機控制器、debug開關關閉、ID或秒數非法、比賽尚未開始、找不到唯一且配置就緒的目標車輛、目標未被控制、HP尚未就緒或已死亡、ASC與目前Avatar不匹配。

Shipping版本不執行此debug功能。未新增專用Stun觸發區、客戶端施加RPC或藍圖資產。

共用接口與調參說明見 [Stun 複用與參數調整指南](Stun_Reuse_And_Parameters_CN.md)。

## 本輪驗證

UE5.8.2 Editor／Game完整編譯成功；普通81/81、Listen Server雙端Host／Client各1/1，測試報告均0警告／失敗。普通測試涵蓋指定其他玩家、立即落球、無效時長／ID、Waiting、HP0、關閉開關及非本地控制器拒絕。雙端透過實際控制台字串對主機和遠端車施加，確認客戶端控制台拒絕及自然到期；客户端完成時40次實際重模擬、2次橋接回滾請求。詳見[驗證記錄](../Combat/MVPB_Stun_DebugCommand_Verification_20261027.json)。

本輪為NullRHI原生驗證，尚未以實際畫面檢查控制台輸入手感／屏幕文字，也不代表F4完整驗收。1157份保存資產前後相同；未提交／Push。
