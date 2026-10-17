# ZSH LAB

普段のターミナルの中で、本物の zsh を使ってステップごとに練習するアプリです。左に zsh、右に課題とヒントを表示します。

**単体バイナリで動きます。Python・tmux・追加パッケージのインストールは不要です。**

## 起動

[Releases](https://github.com/shiroinock/zsh-lab/releases/latest) の ZIP を展開し、同梱の `zsh-lab` を実行してください。`Start.command` を開いても起動できます。ソースから利用する場合は下記のビルド手順を使います。

```sh
./zsh-lab
```

- 配布バイナリ：macOS 11 以降、Apple Silicon / Intel 対応（Universal Binary）
- 必要なもの：ターミナル、zsh（macOS 標準搭載）
- 画面サイズ：95 列 × 22 行以上、UTF-8
- 実行ファイル単体を別のフォルダへ移しても動きます。教材はバイナリに内蔵しています。

```sh
./zsh-lab --list           # 課題一覧
./zsh-lab --lesson 9       # 指定課題から開始
./zsh-lab --help
```

## 練習の進め方

1. 右に表示される課題に沿って、左の zsh を操作します。
2. 指定の Widget と編集結果が一致すると「クリア」と表示します。Enter が不要な課題は、そのまま次へ進めます。
3. **Ctrl+] を押して離し、n** で次へ進みます。

| キー | 操作 |
|---|---|
| Ctrl+] → n | クリアしたら次へ |
| Ctrl+] → h | ヒント表示 / 非表示 |
| Ctrl+] → r | 現在の課題の入力をやり直す |
| Ctrl+] → p | 前の課題を復習 |
| Ctrl+] → q | 終了 |
| Ctrl+] → Ctrl+] | Ctrl+] を zsh に渡す |

`Esc → b` は Esc を押して離してから b を押す、という意味です。Alt / Option を Meta として送れる端末なら Alt+B でも操作できます。

エディタの課題では `$VISUAL` / `$EDITOR` を使い、未設定なら vi を開きます。エディタを保存して閉じてから次へ進んでください。任意の外部エディタの全機能への互換性は未検証ですが、macOS の vi で実際の全画面編集と復帰を検証しています。

進捗は `${XDG_STATE_HOME:-~/.local/state}/zsh-lab/progress.txt` に保存し、次回は最初の未完了課題から再開します。`--progress PATH` で保存先を指定できます。課題ごとの初期入力はやり直せますが、練習中に自分で変更したキー設定・関数・オプションは同じ zsh 内に残ります。アプリを新しく起動すると初期状態に戻ります。

## 実習の範囲

23 課題を用意しています。

- 行頭・行末・単語単位の移動
- 削除、yank、undo / redo
- 直前の引数の再利用、さらに前の引数の呼び出し
- 書きかけの行の退避
- bindkey による確認・割り当て、前方一致の履歴検索
- 単語の複製、magic-space
- 同梱 Widget とエディタ連携
- BUFFER / CURSOR を使った Widget 自作
- auto_cd / auto_pushd、プロンプト変更

この初版はスライドの全ページを実習化したものではありません。zeno.zsh の導入・スニペット・外部補完、zsh-bench、複数タブの履歴共有などは未収録です。

## 練習環境

利用者の `.zshrc` は読み込まず、一時的な `ZDOTDIR`、履歴、作業ディレクトリを使って起動します。練習用のキー設定・関数は利用者の設定ファイルには保存しません。進捗ファイルには完了した課題 ID だけを保存します。操作の観測データは一時ディレクトリに置き、通常終了時に削除します。

**OS のサンドボックスではなく、通常の zsh です。** 任意に入力したコマンドは利用者の権限で実行されます。システムの `/etc/zshenv` は zsh の仕様上読み込まれます。起動した外部エディタは、そのエディタ自身の設定に従って動きます。

## 開発・ビルド

アプリは C で実装し、外部ライブラリの取得やコード生成は不要です。教材とシェル初期化コードは `src/` に含まれます。

```sh
make             # 開発環境のアーキテクチャ向けにビルド
make release     # macOS Universal Binary（macOS 上で実行）
make test        # 実際の対話 zsh / vi を PTY から操作するテスト
```

macOS のビルドには Command Line Tools の C コンパイラと make を使います。リンク先は OS 標準の libSystem / libncurses だけです。Linux 向けの条件分岐も含みますが、Linux では curses 開発用パッケージが必要になる場合があり、動作は未検証です。

`forkpty` で起動した zsh の VT 出力を内部の端末画面に反映し、ncurses で左右の領域を描画します。リサイズ、UTF-8 の全角文字、基本的な色、スクロール領域、エディタの代替画面を扱います。一般用途のターミナルエミュレータと同じ全機能を備えているわけではありません。

ZLE の `zle-line-pre-redraw` から `LASTWIDGET` / `BUFFER` / `CURSOR` を観測し、標準 Widget を置き換えずに判定します。設定課題はコマンド実行後の設定値で判定します。課題を切り替える際は観測データの世代を分け、前の課題の操作を誤って判定に使わないようにしています。

検証した内容：

- 23 課題すべての達成経路、手入力だけではクリアしないこと
- 実際の vi での全画面編集・保存・zsh への復帰
- キー操作とペーストの分離、履歴、再試行、リサイズ、進捗保存、一時ディレクトリの片付け
- UTF-8 / 制御シーケンスの分割受信と不規則な入力
- AddressSanitizer / UndefinedBehaviorSanitizer による検査
- 配布する単体バイナリでの画面描画・達成表示・課題遷移・ヒント・縮小と復元・終了

ローカルの Apple Silicon と、GitHub Actions の macOS 15（Apple Silicon / Intel）の両方で、23 課題と画面操作・配布 ZIP の展開検証を実行しています。バイナリはアドホック署名済みで、Developer ID による署名・公証は行っていません。

## GitHub Actions とリリース

`Build and release` ワークフローで、main への push・Pull Request・手動実行ごとに Apple Silicon / Intel のビルドと実習テストを実行します。Apple Silicon ではメモリ検査も実行します。各アーキテクチャのランナーで Universal Binary の ZIP を作成し、展開した実行ファイルを OS 標準ツールだけの環境で起動して課題操作まで検証します。Apple Silicon で作成した ZIP と SHA-256 は Actions の `macos-release` 成果物から取得できます。

バージョンの正本は `VERSION` です。次のリリースでは `VERSION` を変更して main に push し、同じバージョンのタグを push してください。

```sh
git tag v0.1.0
git push origin v0.1.0
```

`v*` タグでは、タグと `VERSION` の一致を検証し、両方のランナーが成功してから GitHub Release を作成します。ZIP と `SHA256SUMS.txt` のアップロードが完了してから下書きを公開します。再実行時には同じタグの Release を再利用します。追加の PAT や署名シークレットは不要で、Actions 組み込みの `GITHUB_TOKEN` を使います。配布バイナリはアドホック署名で、Apple の Developer ID 署名・公証は含みません。

ローカルでも `make package` で同じ ZIP の作成・展開検証・チェックサム作成を実行できます。

## 参照元・ライセンス

次の発表を参考に、課題文と練習環境を独自に作成しました。発表者・発表元による公式アプリではありません。スライドの文章・画像は同梱していません。課題ごとに参考となるスライド番号を表示します。

- mozumasu / PartnerProp「[zsh から学ぶ最高の UX](https://partner-prop.github.io/slides/mozumasu/terminal-night-3/1)」Terminal Night 3、2026-10-15
- [zsh: Zsh Line Editor](https://zsh.sourceforge.io/Doc/Release/Zsh-Line-Editor.html)

MIT License。ソースは `src/`、ライセンスは `LICENSE` を参照してください。
