# Claude 向けメモ（CinematicADV）

## 「Claude でオンラインでいじった前のところまで戻して」と言われたら

- **戻す先:** Claude Code（オンライン）で編集する前の公開版。
  - コミット **`a2aa574`**（2026-09-30 時点の `main`。タグはなし）。
- **前提:** Claude の変更はすべて `claude/*` ブランチで行い、2026-09-30 時点では `main` にマージしていない。
- **例外:** `main` の `0fa8b57`（`Content/Audio/SC_Voice.uasset` の追加）は、**ユーザー自身のコミット**（2026-09-30）。
  - Claude のコードが使うアセットだが、`main` の公開版の動作には影響しない。
  - 戻すときに、これも消すかどうかはユーザーに確認する。

### 戻し方

1. **状態を確認する**
   - `git fetch origin main`
   - `git log --oneline a2aa574..origin/main`
2. **何も出ない場合、または `0fa8b57`（ユーザーの SC_Voice）だけの場合**
   - `main` はすでに公開版のまま。そのことをユーザーに伝える。
3. **マージ済みの場合**
   - 履歴は消さない。公開版の中身に戻すコミットを新しく作る。
     ```sh
     git restore --source=a2aa574 --staged --worktree -- :/
     git commit -m "revert: Claude で編集する前の公開版に戻す"
     ```
   - 作業ブランチに push して PR にする。
4. **禁止事項:** `main` への force-push、履歴の書き換え。
   - どちらも、必ずユーザーに確認してから。

## その他

- レビュー内容、決定事項、ロードマップは `docs/REVIEW_NOTES.md` にある。
- SequencerSubtitles（依存プラグイン）の作業メモは、そちらのリポジトリの `docs/` にある。
