# CinematicADV

**CinematicADV** is a Sequencer-based ADV (adventure game) plugin for Unreal Engine 5.
It adds click-to-advance and skip to Sequencer cinematics. More ADV features are planned (see Roadmap).

## Features

- **Click Wait Track** — pause (Stop) or loop (Loop) a part of the sequence until the player clicks / presses a key
- **ADV-style clicks** — while a line is still being typed (Sequencer Subtitles typewriter), the first click shows the whole line
  and the next click advances. A click outside a wait jumps to the next wait (can be turned off)
- **Auto mode** — waits continue by themselves after the line has been read: until the voice ends, or a delay
  based on the number of characters. Toggle with a key or from Blueprint
- **Backlog** — read past lines (kept while the game runs) and replay their voices. Built-in screen, or your own UMG
  from the recorded data. The sequence pauses while it is open
- **Fast-forward / skip read text** — hold a key (or toggle) to play fast and pass waits at once. Stops at lines not read
  yet (players can choose to skip them too). The read history is kept in its own save file
- **Save / Load** — save the position (at a wait), the backlog and your own variables (text, numbers, flags), with a
  thumbnail. Loading opens the saved level and resumes the sequence. Blueprint functions for your own save screen
- **Voice** — voice Sound Class `SC_Voice` included; right-click sounds → **Set as Voice**. Player voice volume setting
- **Hold to skip** — hold the skip key to fade out and stop the whole sequence (with a circular gauge)
- **Setup without Blueprint** — one DataAsset for the input mapping and actions
- Works in sub-sequences (shots) and across level travel
- Works with the [Sequencer Subtitles](https://github.com/kokagefujieda/SequencerSubtitles) plugin

## Roadmap

In this order:
1. ~~First click shows the whole text, second click advances~~ (done)
2. ~~Auto mode~~ (done)
3. ~~Backlog~~ (done)
4. ~~Fast-forward / skip read text~~ (done)
5. ~~Save / Load~~ (done)
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
- **オートモード** — 台詞を読み終えたら自動で次へ。ボイスがあればボイスの終わりまで、なければ文字数に応じた時間だけ待つ。
  キーまたは Blueprint で ON/OFF
- **バックログ** — 過去の台詞を読み返し、ボイスを再生し直せる（ゲームの起動中は保持）。組み込みの画面のほか、
  記録を取り出して自作の UMG でも表示できる。開いている間はシーケンスを一時停止
- **早送り・既読スキップ** — キーを押している間（または切り替え）高速再生し、待機はすぐに通過。未読の台詞で止まる
  （プレイヤーの設定で未読も飛ばせる）。既読は専用のセーブファイルに保存
- **セーブ / ロード** — 位置（待機）、バックログ、ゲーム側の変数（文字列・数値・フラグ）をサムネイル付きで保存。
  ロードすると保存したレベルを開いてシーケンスを再開。セーブ画面は Blueprint 関数で自作
- **ボイス** — ボイス用サウンドクラス `SC_Voice` を同梱。サウンドを右クリック →**Set as Voice** で割り当て。プレイヤー向けのボイス音量設定
- **長押しスキップ** — スキップキーの長押しで、フェードアウトしてシーケンス全体を停止（円形ゲージ付き）
- **Blueprint 不要のセットアップ** — 入力の割り当ては DataAsset 1 つで設定
- サブシーケンス（ショット）の中や、レベル移動後も動作
- [Sequencer Subtitles](https://github.com/kokagefujieda/SequencerSubtitles) プラグインと連携

## ロードマップ

次の順で追加予定です。
1. ~~1 回目のクリックで文章を全部表示、2 回目で次へ~~（対応済み）
2. ~~オートモード~~（対応済み）
3. ~~バックログ~~（対応済み）
4. ~~早送り・既読スキップ~~（対応済み）
5. ~~セーブ / ロード~~（対応済み）
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
