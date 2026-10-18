# ZSH LAB

普段のターミナルの中で、左に本物の zsh、右に課題を表示する練習アプリです。23 課題を、使い捨ての小さな Docker コンテナ内で体験できます。ホストにはネイティブの起動用バイナリを配置します。

## 起動・インストール

Docker の Linux エンジンを起動してください。macOS では Docker Desktop / OrbStack などを使えます。Python・tmux は利用時には不要です。

Nix は macOS Apple Silicon と Linux amd64 に対応しています。Docker CLI は Nix が揃えますが、動作中の Docker エンジンは別途必要です。

```sh
nix run 'git+https://github.com/shiroinock/zsh-lab'

nix profile add 'git+https://github.com/shiroinock/zsh-lab'
zsh-lab

# 更新
nix profile upgrade zsh-lab --refresh
```

プライベートリポジトリなので Git の GitHub 認証が必要です。`gh auth login` 済みなら `gh auth setup-git` で Git に設定できます。GitHub CLI と Nix の GitHub fetcher の認証は別です。

バージョンを固定する場合：

```sh
nix run 'git+https://github.com/shiroinock/zsh-lab?ref=refs/tags/v0.3.0'
```

Home Manager / nix-darwin でも、flake input を追加してパッケージを指定できます。

```nix
inputs.zsh-lab.url = "git+https://github.com/shiroinock/zsh-lab?ref=refs/tags/v0.3.0";
# inputs と system を渡したモジュール内
home.packages = [ inputs.zsh-lab.packages.${system}.default ];
```

macOS Apple Silicon では [Releases](https://github.com/shiroinock/zsh-lab/releases/latest) の ZIP にある `zsh-lab`、または `Start.command` からも起動できます。Docker CLI が PATH にある環境で実行してください。macOS 11 以降向けの arm64 バイナリです。

```sh
zsh-lab --prepare         # 対話画面を開かず、初回のイメージ作成だけを行う
zsh-lab --list
zsh-lab --lesson 9
zsh-lab --progress /path/to/progress.txt
zsh-lab --version
zsh-lab --help
```

画面は UTF-8、95 列 × 22 行以上を使ってください。

## 初回の環境作成

起動用バイナリには Dockerfile と教材・アプリのビルド用ソースが入っています。ソースのチェックアウトは不要です。初回に `zsh-lab-workshop:0.3.0` をローカルで作成し、次回以降は再利用します。初回のダウンロードとビルドにはネットワークが必要です。

ベースは Docker Hub 公式の Alpine Linux 3.22 で、マルチアーキテクチャの digest を固定しています。zsh・Vim 等は Alpine 公式パッケージ配布元から取得します。ビルド用のコンパイラ等は別ステージに置き、実行イメージには含めません。詳細は `container/Dockerfile` を参照してください。

Dockerfile を変更して開発中に同じバージョンのイメージを作り直す場合：

```sh
docker build --target runtime -t "zsh-lab-workshop:$(cat VERSION)" -f container/Dockerfile .
```

## 練習の進め方

右の課題に沿って左の zsh を操作します。指定の Widget と編集結果が一致すると「クリア」と表示し、次の課題へ進めます。Enter が不要な課題はそのまま進めます。

| キー | 操作 |
|---|---|
| Ctrl+] → n | クリアしたら次へ |
| Ctrl+] → h | ヒント表示 / 非表示 |
| Ctrl+] → r | 現在の課題をやり直す |
| Ctrl+] → p | 前の課題を復習 |
| Ctrl+] → q | 終了 |
| Ctrl+] → Ctrl+] | Ctrl+] を zsh に渡す |

`Esc → b` は Esc を押して離してから b を押すという意味です。Alt / Option を Meta として送れる端末なら Alt+B でも操作できます。エディタ課題ではコンテナ内の Vim を開きます。ホストのエディタ設定は使いません。

進捗はホストの `${XDG_STATE_HOME:-~/.local/state}/zsh-lab/progress.txt` に、完了した課題 ID だけを保存します。終了時にコンテナ側から認識済みの課題 ID だけを取り出します。ホストの進捗ファイルをコンテナにはマウントしません。起動時に以前の完了状況を引き継ぎ、最初の未完了課題から再開します。

## 練習環境

TUI と zsh の両方をコンテナ内で実行します。コンテナ作成に失敗した場合、ホストの zsh に切り替えることはありません。

- ホストのホーム・作業フォルダ・Docker ソケットはマウントしません。ホストのトークン・SSH ソケット・シェル設定は渡しません。
- HOME は `/home/learner`、作業場所は `/work`。`cd /` や `ls /` で見えるのはコンテナ自身のファイルです。
- 一般ユーザー UID 1000 で実行し、capability を全て落とし、権限昇格を禁止します。
- ネットワークは `none`。PID・IPC 等もホストと共有しません。
- ルートファイルシステムは読み取り専用。`/work`・HOME・`/tmp` はサイズ制限付きの一時領域です。
- CPU は 1、メモリは 256 MiB、プロセス数は 64、ファイルサイズは 1 MiB に制限します。
- 進捗の受け渡しにだけ匿名 Docker ボリュームを使い、終了時にコンテナと一緒に削除します。

`rm -rf ~` の対象もコンテナ内の HOME です。練習環境を壊した場合は終了して起動し直してください。強制終了や Docker の異常停止で残ったコンテナは `io.github.shiroinock.zsh-lab.session=true` ラベルで識別できます。Docker イメージとビルドキャッシュは次回起動用に残します。

## 実習の範囲

行頭・行末・単語移動、削除・yank・undo / redo、履歴・引数の再利用、書きかけの行の退避、bindkey、前方一致履歴検索、単語複製、magic-space、エディタ連携、Widget 自作、auto_cd / auto_pushd、プロンプト変更を扱います。

スライドの全ページを実習化したものではありません。zeno.zsh、外部補完、zsh-bench、複数タブの履歴共有などは未収録です。

## 開発・検証

C コンパイラ・make・Python 3・ncurses 開発ライブラリを使います。Python は Docker ビルド用ソースのバイナリへの埋め込みにだけ使います。Docker 内でアプリをビルドするため、ホストの zsh は不要です。

```sh
make host-check  # ホスト側ランチャーと進捗受信の境界チェック
make check       # 上記 + Docker 内の全課題 + ホストからの画面操作
make sanitize    # ホスト側の AddressSanitizer / UndefinedBehaviorSanitizer
make package     # macOS arm64 の署名・ZIP・展開・Docker 起動検証
```

```sh
nix flake check -L
nix build '.#zsh-lab^*'
./scripts/check-docker.sh ./result/bin/zsh-lab
./result-tests/bin/smoke-ui ./result/bin/zsh-lab
```

Nix ビルド内では Docker エンジンを使用せず、ホスト側のチェックと検証用バイナリの作成を行います。Docker 実行テストは完成した成果物で行います。

PTY と内部 VT パーサーで zsh の画面を表示し、ncurses で左右を描画します。ZLE の `LASTWIDGET`・`BUFFER`・`CURSOR` と、コマンド実行後の設定値を観測して達成を判定します。通常のターミナルエミュレータと同じ全機能を備えているわけではありません。

## Actions・リリース

main push・PR・手動実行で macOS arm64 のバイナリと両 OS の Nix パッケージをビルドします。Linux amd64 の Docker エンジンで全23課題、Vim、隔離、完成済みランチャーからの描画・達成・遷移・ヒント・リサイズ・進捗保存・終了を検証します。macOS ランナーには Docker エンジンがないため ZIP の Docker 起動チェックを省略し、ローカルの Apple Silicon + OrbStack でも検証します。

`VERSION` と一致する `v*` タグで、全ジョブ成功後にプライベート GitHub Release を作成します。ZIP と `SHA256SUMS.txt` を添付してから公開します。追加の PAT は不要です。macOS バイナリはアドホック署名で、Developer ID 署名・公証は含みません。

## 参照元・ライセンス

mozumasu / PartnerProp「[zsh から学ぶ最高の UX](https://partner-prop.github.io/slides/mozumasu/terminal-night-3/1)」Terminal Night 3、2026-10-15 を参考に独自の教材を作りました。公式アプリではなく、スライドの文章・画像は同梱していません。

- [Zsh Line Editor](https://zsh.sourceforge.io/Doc/Release/Zsh-Line-Editor.html)
- [Docker run](https://docs.docker.com/reference/cli/docker/container/run/)

MIT License。ソースは `src/`、ライセンスは `LICENSE` を参照してください。
