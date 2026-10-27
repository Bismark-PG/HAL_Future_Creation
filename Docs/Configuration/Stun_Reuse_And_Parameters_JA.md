# Stunの再利用とパラメータ設定ガイド

更新日 2026年10月27日

手動テストでは、試合開始後にHostのコンソールで`HALGASStatus`から対象のPlayerIdを確認し、`HALStun <PlayerId> <秒数>`を実行できます。例は`HALStun 257 3`です。Clientからの実行は拒否します。`hal.StunDebug 0`でこのデバッグ付与のみを無効化し、`1`で再度有効にできます。通常のギミックからの付与には影響しません。

本書は、既存の共通Stun機能をギミック、アイテム、スキルから利用する方法と、継続時間の設定方法を説明します。対象は現在のC++実装です。新しいギミックやStun Zoneはまだ作成していません。

## 1 利用の概要

サーバーが発動や命中の正当性を確認した後、対象車両のAbility System Component（ASC）を取得し、次の関数を呼び出します。

```cpp
TargetASC->ApplyStun(StunDurationSeconds, SourceActor);
```

発生元ごとに異なる継続時間を指定できます。発生元ActorにASCを付けたり、ギミックごとにStun用のGameplay Ability（GA）やGameplay Effect（GE）を作ったりする必要はありません。

Stunが有効になると、共通処理で次の動作を行います。

- アクセル、ブレーキ／後退、ステアリング、ドリフト、ボールの発射と取得、GASの能力発動、自力復帰を禁止します。
- 保持中のボールを直ちに離し、`Controlled`から`Free`に戻します。追加の放出インパルスや発射反動は与えません。
- 車両の慣性、重力、外部からの衝突や衝撃を維持します。速度をゼロにしたり、剛体を固定したりしません。
- 時間切れで制限を解除します。車両の変更やAvatarの解除時には、既存のライフサイクル処理で効果を除去します。

現在、アイテムの所持・使用システムは未実装です。将来、GAS Abilityを使わないアイテムを追加する場合は、第5節の判定をそのサーバー実行処理に組み込んでください。

## 2 C++からの組み込み

以下は既存の発生元Actorへ追加するコード例です。新規ギミックの実装がすでに存在することを示すものではありません。

### ヘッダーで継続時間を公開する

発生元Actorのクラス内に追加します。

```cpp
// この発生元における設定例。初期値はゲーム時間で3秒。
UPROPERTY(EditDefaultsOnly, Category = "Stun",
    meta = (ClampMin = "0.01", Units = "s"))
float StunDurationSeconds = 3.0f;

bool ApplyStunToTarget(AActor* TargetVehicle);
```

`EditDefaultsOnly`なので、Blueprintクラスの **Class Defaults** から調整できます。レベルに配置した各インスタンスで個別編集する設定ではありません。発生元に既存のData Asset設定がある場合は、その設定に継続時間を追加して読み込む方法も使えます。

### 対象のASCを取得して付与する

発生元の`.cpp`に次のヘッダーを追加します。

```cpp
#include "AbilitySystemGlobals.h"
#include "HALAbilitySystemComponent.h"
```

`AYourMechanism`は、実際の発生元クラス名に置き換えてください。

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

    // 古いPawn参照ではなく、このASCの現在のAvatarを対象とする。
    if (!TargetASC || TargetASC->GetAvatarActor() != TargetVehicle)
    {
        return false;
    }

    return TargetASC->ApplyStun(StunDurationSeconds, this);
}
```

この処理は、発生元が合法な発動・命中を確定した時点で呼び出してください。対象の選定、攻撃単位の重複命中防止、クールダウン、距離や衝突の確認は、発生元のC++側で管理します。`ApplyStun`がこれらを代行するわけではありません。

Tickから毎フレーム付与しないでください。毎フレーム3秒を指定すると終了時刻が更新され続け、Stunが終わらなくなる可能性があります。周期的に付与する設計では、発生元に発動間隔と再命中ルールを明示します。

## 3 パラメータと設定場所

| 項目 | 設定場所 | 内容 |
|---|---|---|
| Stunの継続時間 | 発生元の`StunDurationSeconds`、または既存のData Asset | `ApplyStun`へ渡す秒数。現在のStun効果で調整するパラメータは継続時間のみです |
| 発生元Actor | `ApplyStun`の第2引数 | 効果の発生元情報です。通常はギミックやアイテムActorの`this`を渡します |
| 発動間隔、クールダウン、効果範囲 | 発生元の設定 | 共通Stunのパラメータではなく、個々のギミックやアイテム側で実装します |

### Unreal Editorで継続時間を変更する

上記のC++組み込みとコンパイルを終えた後、チームメンバーが手動で操作します。

1. 対象のギミックまたはアイテムのBlueprintクラスを開きます。
2. **Class Defaults** を選択します。
3. `Stun Duration Seconds`を検索するか、`Stun`カテゴリを開きます。
4. 秒数を入力します。例えば`2.5`ならゲーム時間で2.5秒です。
5. Blueprintをコンパイルして保存し、ゲームを再開始して確認します。

現時点では発生元クラスやこの設定項目をまだ作成していないため、既存のEditorに上記の項目が自動追加されるわけではありません。共通Stun自体は、C++から異なる秒数を渡して利用できます。

### 指定できる値

- `0`より大きい有限の秒数を受け付けます。例は`0.5`、`3`、`8`です。
- `0`、負数、NaN、無限大は拒否します。`0`を渡してもStun解除にはなりません。
- 例の`ClampMin = "0.01"`はEditor入力の下限です。API自体の追加下限ではなく、実行時の値の検証も必要です。
- 現在、全発生元に共通する最大継続時間は設けていません。ゲームバランス上の範囲は発生元の設計で決めます。
- 計時はゲームワールド時間を使います。実時間の秒数や、固定の物理フレーム数と同一視しないでください。

ギミックの継続時間を変えるために、`HALStunEffect.cpp`の既定値である1秒を変更する必要はありません。共通入口が、呼び出し時の値でGE Specの継続時間を上書きします。

## 4 再付与と複数の発生元

共通ルールは次のとおりです。

```text
新しい終了時刻 = max(現在の終了時刻, 今回の付与時刻 + 今回の継続時間)
```

例えばStunが残り3秒の場合、次の結果になります。

| 今回指定する時間 | 再付与後の残り時間 |
|---|---|
| 1秒 | 残り3秒を維持 |
| 3秒 | 今回の付与時刻から3秒を確保 |
| 5秒 | 今回の付与時刻から5秒へ延長 |

このルールは発生元をまたいで適用されます。ギミックAとアイテムBの効果を受けても強度は加算せず、「残り3秒＋新規5秒」を8秒にはしません。

`ApplyStun`の戻り値は次の意味です。

- `true`：効果を付与できた、または既存の残り時間が今回の要求をすでに満たしています。
- `false`：サーバー以外からの呼び出し、有効なAvatarがない、継続時間が不正、GEの付与失敗などです。

`true`でも、新しいStun開始イベントが発生したとは限りません。延長時は状態を途切れさせず、ボールを離す処理も繰り返しません。

## 5 状態の照会とアイテム使用の制限

ASCを取得済みの場合は次の関数を使います。

```cpp
const bool bStunned = TargetASC->IsStunned();
```

対象Actorのみを持つ場合は次の関数を使います。

```cpp
const bool bStunned = UHALAbilitySystemComponent::IsActorStunned(TargetVehicle);
```

将来追加する非GASアイテムでは、サーバー実行処理で、所持数の消費、クールダウン開始、Actor生成、効果付与よりも前に確認してください。

```cpp
if (UHALAbilitySystemComponent::IsActorStunned(UsingVehicle))
{
    return; // 使用を拒否。所持数を消費せず、今回のクールダウンも開始しない。
}

// 他の使用条件も確認してからアイテムを実行する。
```

ASCが見つからないActorに対して、この照会は`false`を返します。利用者、車両、生存状態、試合フェーズ、所持数などの正当性確認は別途必要です。

既存のGAS Ability経由で発動する場合、ASC側のStun判定で新規発動を阻止します。すでに有効な加速や免疫などのGEは、それぞれの計時を継続します。実行時間が長いアイテムAbilityを将来追加する場合は、「途中でStunになったら中断するか」をそのアイテム側で定義してください。新規発動の禁止は、実行中の全処理を自動キャンセルする機能ではありません。

## 6 残り時間と表示用の通知

```cpp
#include "HALGameplayTags.h"

const float RemainingSeconds = TargetASC->GetEffectTimeRemaining(
    HALGameplayTags::State_Stun);
```

| 戻り値 | 意味 |
|---|---|
| `0` | タグがない、または取得した残り時間がゼロ |
| `0`より大きい値 | この端末で取得できた残りゲーム秒数 |
| `-1` | 完全な継続時間の情報がない、または無限の効果が存在する。解除済みという意味ではありません |

プレイヤーASCはMixed Replicationを使用しています。ほかのプレイヤーを観察するクライアントは、`State.Stun`の存在を把握できても、GEの完全な継続時間を取得できない場合があります。状態判定には`IsStunned()`または`IsActorStunned()`を使い、`RemainingSeconds <= 0`で代用しないでください。

UIやエフェクトは、C++からASCの`State.Stun`に対する`EGameplayTagEventType::NewOrRemoved`イベントを購読できます。登録時に現在の状態を確認し、解除時・終了時にはデリゲートも解除します。継続時間の延長では、見かけ上の終了・再開始イベントは発生しません。現在の実装には専用UI、エフェクト、Blueprint Event Graphは追加していません。

## 7 時間切れ前の解除

将来、Stunを解除する効果を追加する場合は、サーバーが解除の正当性を確認してから既存のGAS関数を使用します。

```cpp
TargetASC->RemoveActiveEffects(
    FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(
        FGameplayTagContainer(HALGameplayTags::Effect_Stun)));
```

この例では`Effect.Vehicle.Stun`の効果タグを持つ活動中GEのみを除去します。Stunを解除するために全効果を一括除去しないでください。これは接続方法の例であり、解除用アイテムやクライアントから呼べる解除RPCを追加したものではありません。

## 8 組み込み時の注意点

- 呼び出すのは対象車両のASCです。発生元自身のASCではありません。プレイヤーASCはPlayerStateにあるため、第2節の例のように車両のAbility System Interface経由で取得します。
- `State.Stun`のloose tagを直接追加・削除したり、別の`bStunned`や独立タイマーを作ったりしないでください。
- 発生元ごとに独自のGEを作成して`ApplyStun`を迂回すると、共通の継続時間ルールを維持できなくなる可能性があります。
- Stun付与のために`HALMovementEffectBridge`、物理スナップショット、ボールの拘束を直接操作する必要はありません。
- 現在の`ApplyStun`および状態照会はC++関数で、`BlueprintCallable`ではありません。既存のBlueprint Event Graphから直接検索・実行することはできません。ルールの接続はC++で行い、Blueprintでは発生元の設定値と表示用アセットを調整します。
- クライアントからStunを直接付与しないでください。アイテム使用要求を受けたサーバーが命中を判定し、共通入口を呼び出します。
- オンライン参加端末は同じコードとエンジンパッチで完全ビルドしてください。運動タイムラインの形式はバージョン2に更新済みで、旧プログラムと混在接続しません。

## 9 組み込み後の手動確認

HostとClientの両方で、以下を確認します。

1. ボール保持中に3秒のStunを受けると直ちにボールを離し、運転、発射、取得、能力発動、自力復帰ができなくなる。
2. 車両は滑走、落下、外部衝突による移動を続ける。Stun自体でHPは減少しない。
3. 残り3秒に1秒を再付与しても短縮しない。5秒を付与すると今回から5秒になり、落球処理を繰り返さない。
4. 時間切れで操作が戻る。アクセルを押し続けている場合は運転が再開するが、Stun中に押した発射・能力入力は自動で後から実行しない。
5. 車両変更・退出後に効果が残らず、ほかの車両は落ちたボールを奪える。

これらは発生元の組み込み後に実施する確認で、本書の作成によって新たに実施されたものではありません。共通Stunの既存検証では、UE 5.8.2のEditor／Gameビルド、通常自動テスト80件、Listen ServerのHost／Client各1件が成功しています。新しいギミックでの実際のプレイ確認は未実施です。

プロジェクト内の関連資料：

- [中国語版の利用ガイド](Stun_Reuse_And_Parameters_CN.md)
- [共通Stunの実装説明](../Combat/MVPB_Stun_State_CN_v0.1.md)
- [2026年10月27日の検証記録](../Combat/MVPB_Stun_Verification_20261027.json)

Googleドキュメントへ転記する際、上記の相対リンクはプロジェクト内の参照です。Google Drive上のファイルを指すリンクではないため、共有先に合わせて参照先を差し替えてください。
