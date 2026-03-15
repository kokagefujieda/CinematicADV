# CinematicADV

**CinematicADV** は Unreal Engine 5 向けの Sequencer ベース ADV（アドベンチャーゲーム）エンジンプラグインです。
Sequencer で制作したシネマティックに、クリック送り・バックログ・スキップ・オートモード・セーブ/ロードを付加します。

## 特徴

- クリック/キー入力による **シーン送り**
- **バックログ**（既読テキスト履歴の表示）
- **スキップモード**（既読シーン早送り）
- **オートモード**（自動進行）
- **セーブ / ロード** 対応
- [Sequencer Subtitles](https://github.com/kokagefujieda/SequencerSubtitles) プラグインと連携

## 動作環境

- Unreal Engine 5.7+
- 依存プラグイン: **Sequencer Subtitles**, Enhanced Input

## インストール

1. `Plugins/CinematicADV` と `Plugins/SequencerSubtitles` をプロジェクトの `Plugins/` にコピー
2. UE エディタを起動し、両プラグインを有効化
3. サンプルレベル `Content/Levels/L_CinematicADV_Sample` で動作確認

## ライセンス

[MIT License](LICENSE)
Copyright (c) 2024–2026 kokage (Fujieda Shinji)
