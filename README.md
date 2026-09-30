# CinematicADV

**CinematicADV** is a Sequencer-based ADV (adventure game) plugin for Unreal Engine 5.
It adds click-to-advance and skip to Sequencer cinematics. More ADV features are planned (see Roadmap).

## Features

- **Click Wait Track** — pause (Stop) or loop (Loop) a part of the sequence until the player clicks / presses a key
- **ADV-style clicks** — while a line is still being typed (Sequencer Subtitles typewriter), the first click shows the whole line
  and the next click advances. A click outside a wait jumps to the next wait (can be turned off)
- **Hold to skip** — hold the skip key to fade out and stop the whole sequence (with a circular gauge)
- **Setup without Blueprint** — one DataAsset for the input mapping and actions
- Works in sub-sequences (shots) and across level travel
- Works with the [Sequencer Subtitles](https://github.com/kokagefujieda/SequencerSubtitles) plugin

## Roadmap

In this order:
1. ~~First click shows the whole text, second click advances~~ (done)
2. Auto mode
3. Backlog
4. Fast-forward / skip read text
5. Save / Load
6. Choices (branching)

## Installation

1. Copy `Plugins/CinematicADV` and `Plugins/SequencerSubtitles` into your project's `Plugins/` directory
2. Launch the UE editor and enable both plugins
3. Create a `CinematicADVConfig` DataAsset and set it in **Project Settings → Plugins → CinematicADV → Config Asset**
   (required for packaged builds)
4. Try the sample level at `Content/Levels/L_CinematicADV_Sample`

---

**CinematicADV** は Unreal Engine 5 向けの Sequencer ベース ADV（アドベンチャーゲーム）プラグインです。
Sequencer で制作したシネマティックに、クリック送りとスキップを付加します。ADV 向けの機能は順次追加予定です（ロードマップ参照）。

## 特徴

- **Click Wait Track** — シーケンスの一部を、クリック／キー入力まで一時停止（Stop）またはループ（Loop）
- **ADV 式のクリック** — 台詞がタイプライターで表示途中なら、1 回目のクリックで全文を表示し、2 回目で次へ。
  待機の外でクリックすると、次の待機位置へジャンプ（設定で OFF 可）
- **長押しスキップ** — スキップキーの長押しで、フェードアウトしてシーケンス全体を停止（円形ゲージ付き）
- **Blueprint 不要のセットアップ** — 入力の割り当ては DataAsset 1 つで設定
- サブシーケンス（ショット）の中や、レベル移動後も動作
- [Sequencer Subtitles](https://github.com/kokagefujieda/SequencerSubtitles) プラグインと連携

## ロードマップ

次の順で追加予定です。
1. ~~1 回目のクリックで文章を全部表示、2 回目で次へ~~（対応済み）
2. オートモード
3. バックログ
4. 早送り・既読スキップ
5. セーブ / ロード
6. 選択肢（分岐）

## インストール

1. `Plugins/CinematicADV` と `Plugins/SequencerSubtitles` をプロジェクトの `Plugins/` にコピー
2. UE エディタを起動し、両プラグインを有効化
3. `CinematicADVConfig` の DataAsset を作り、**Project Settings → Plugins → CinematicADV → Config Asset** に設定
   （パッケージ版で必要です）
4. サンプルレベル `Content/Levels/L_CinematicADV_Sample` で動作確認

---

## Supported Versions / 動作環境

- Unreal Engine 5.7+
- Dependency / 依存プラグイン: **Sequencer Subtitles**, Enhanced Input

## Documentation / ドキュメント

https://milkemist.com/sqs/

## License / ライセンス

[MIT License](LICENSE)
