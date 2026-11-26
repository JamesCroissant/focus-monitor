# focus-monitor

C++ / OpenCV を用いて、カメラ映像から人の顔を検出し、パソコンにきちんとフォーカスしているかをチェックするアプリ。

## 現状

- カメラ映像を取得し、Haar Cascade (`haarcascade_frontalface_default.xml`) で顔を検出して
  枠を表示する。
- 顔の検出有無をもとに「Focused / Not focused」を判定し、現在の状態が続いている秒数と
  セッション全体の集中率（Focus rate）を画面に表示する（`src/focus_tracker.h`）。
  検出のチラつきを吸収するため、顔が見えなくなってから既定のグレースピリオド（既定2秒）
  経過するまでは Focused のまま扱う。
- 現時点ではあくまで「カメラに顔が写っているか」ベースの判定であり、顔の向き・視線方向の
  推定は行っていない（今後の拡張候補）。
- 非集中（Not focused）状態が既定10秒続くとアラートを1回発火し、その後も状態が続く限り
  既定30秒おきに繰り返す（`src/alert_scheduler.h`）。フォーカスが戻るとリセットされる。
  アラートは端末ベル音＋ログ出力に加え、`notify-send` が利用可能な環境（Linuxデスクトップ、
  `libnotify-bin` など）ではデスクトップ通知も送る（`src/notifier.h`）。`notify-send` が
  無い環境（macOS、Windows、ヘッドレス環境など）ではログ＋ベル音のみに自動的にフォール
  バックする。`notify-send` の呼び出しはバックグラウンドで実行され、呼び出し元
  (カメラ撮影ループと同じスレッド)をブロックしない -- D-Bus や通知デーモンが
  一時的に応答しなくなった場合でも、フレーム取得やプレビューウィンドウの描画、
  `q`/ESC/Ctrl-C への応答がそれに引きずられて止まることはない。
- `FocusTracker`/`AlertScheduler` はカメラ・OpenCV に依存しないロジックのみなので、
  単体テストを用意している(`tests/`、詳細は下記「テスト」節)。
- push / pull request のたびに GitHub Actions でビルド＋テストを自動実行する
  (`.github/workflows/build-and-test.yml`)。カメラが無い CI 環境でも `focus_monitor`
  本体のビルド確認と、カメラに依存しない単体テストの実行はできる。`ctest` は
  main.cpp 自体の制御フロー(`--help`/`--stats`/ディスプレイチェック/
  カスケード読み込み/カメラオープンの順序)を一切検証しないため、実際にビルドした
  バイナリで `--help`(終了コード `0`)、ディスプレイ無し実行(終了コード `1`、
  "no display available" というエラー文言)、`--stats`(ログが無い場合の終了コード
  `1`、実際のログファイルを読んだ場合に出力される3行の集計結果がちょうど
  `ReadSessionLogStats` の長さ重み付け平均と一致すること)、不正な引数
  (終了コード `1`、`Error: ` で始まるエラー文言と使用方法の両方が出力される
  こと)を直接確認するスモークテストも追加で実行する。
- カメラ番号・グレースピリオド・アラートの delay / repeat interval をはじめ、
  以下に挙げるセッションログ・顔検出の最小サイズ・カメラ解像度・`--stats` も
  含めて、カスケードパス以外の設定はすべてコマンドラインオプションで変更できる
  (`src/config.h`、完全な一覧は下記「実行方法」節を参照)。パース処理は
  OpenCV に依存しないので、こちらも単体テスト済み。
- セッション終了時（`q`/ESC での終了、カメラ切断による終了のいずれでも）、セッションの
  長さと集中率を CSV 形式で `focus_sessions.csv`（既定）に1行追記する
  (`src/session_logger.h`)。ログ出力先は `--session-log` で変更、`--no-session-log`
  で無効化できる。ファイルが書き込めない場合も警告を出すだけでセッション自体は正常に
  終了する。
- `Ctrl-C`（`SIGINT`）や `kill`（`SIGTERM`）で終了した場合も、デフォルトハンドラに
  よる即時終了ではなく `q`/ESC と同じ正常終了パスを通る(`src/shutdown_flag.h`)。
  これにより `camera.release()`・ウィンドウの破棄に加え、上記のセッションログも
  確実に書き込まれる。
- 顔検出の最小サイズ(`detectMultiScale` の `minSize`)もコマンドラインオプション
  `--min-face-size` で変更できる(既定 60px)。カメラの解像度やユーザーとの距離に
  よって最適な値は変わるため、固定値のままでは検出精度が環境依存になってしまう。
- カメラの解像度自体も `--width`/`--height` で指定できる(既定はカメラの
  デフォルト解像度のまま)。上記の `--min-face-size` の最適値が解像度に
  依存する以上、解像度そのものを変更する手段が無いのは片手落ちだった。
- `detectMultiScale` のもう2つのパラメータ、`scaleFactor`(`--scale-factor`、
  既定 1.1)と `minNeighbors`(`--min-neighbors`、既定 4)もコマンドラインから
  変更できる。これまで main.cpp 内に固定値として埋め込まれていたが、
  `--min-face-size` と同じ理由(カメラ・照明条件によって最適値が変わる)で
  再コンパイルなしに調整する手段が無いのは同じ片手落ちだった。
- `detectMultiScale` の `maxSize`(`--max-face-size`)もコマンドラインから
  指定できる(既定は無制限)。`minSize`(`--min-face-size`)側はずっと
  調整できたのに、上限側だけ main.cpp が一切指定しておらず常に無制限のまま
  だった -- フレームに映り込む大きな非顔オブジェクトを顔と誤検出し続ける
  ような環境でも、これまでは除外する手段が無かった。`--max-face-size` を
  `--min-face-size` より小さい値に設定すると `detectMultiScale` の検索範囲
  (`[minSize, maxSize]`)が空になり何も検出されなくなるため、その矛盾は
  起動時のエラーとして拒否する。
- カメラから一時的に空フレームを受け取っても即座には終了せず、最大30回連続
  (`src/consecutive_failure_guard.h`、既定の再試行間隔10msで約300ms相当)まで
  再試行してから本当にカメラが切断されたと判断する。健全なカメラでも起こり得る
  1フレームだけの取得失敗で、セッション全体を打ち切ってしまうことを防ぐ。
- セッション中にカメラが切断されて終了した場合は、`q`/ESC や `Ctrl-C`/`kill` に
  よる終了とは異なる終了コード(`2`)を返す。終了コード一覧は下記「終了コード」
  節を参照。
- `--session-log` に書き続けてきた CSV は追記されるだけで、これまで
  `focus_monitor` 自身がそれを読み返すことはなかった。`--stats` でこれまでの
  セッション履歴(セッション数・合計追跡時間・長さで重み付けした平均集中率)を
  集計して表示できる(`src/session_logger.h` の `ReadSessionLogStats`)。
  カメラは一切開かない。
- `DISPLAY`/`WAYLAND_DISPLAY` のいずれも未設定のときは、プレビューウィンドウを
  開く前にエラーを出して終了コード `1` で終了する(`src/display_availability.h`)。
  これが無いと、systemd のユニットやデスクトップセッションの無い SSH 接続から
  起動した場合、OpenCV の Qt/GTK バックエンドが `cv::namedWindow` 内で
  catch 不可能な `abort()` を呼んでクラッシュしてしまう(C++ の例外ではなく
  シグナルなので、`main` 側の try/catch では捕まえられない)。

## 必要環境

- CMake 3.16 以上
- C++17 対応コンパイラ
- OpenCV (`libopencv-dev` など)

Ubuntu/Debian の場合:

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends libopencv-dev opencv-data
```

(`--no-install-recommends` を付けない場合、環境によっては mesa / gphoto2 / libinput
などの不要な推奨パッケージが原因でインストールに失敗することがあります。
`opencv-data` は Haar Cascade のデータファイルを含むパッケージです。)

顔検出用のカスケードファイルはリポジトリの `data/haarcascades/` に同梱済みなので、
`data/` を含むプロジェクトルートから実行すれば追加のダウンロードは不要です。
見つからない場合はシステムの標準インストール先（`/usr/share/opencv4/haarcascades/` など）
も自動的に探索します。任意のパスを使いたい場合は実行時の第1引数で指定できます。

## ビルド方法

```sh
mkdir build && cd build
cmake ..
cmake --build .
```

## 実行方法

プロジェクトルートから実行してください（同梱のカスケードファイルを解決するため）。

```sh
./build/focus_monitor
```

`q` キーまたは ESC キーで終了します。`Ctrl-C`/`kill` による終了でも同じ
正常終了パスを通ります。

### 終了コード

- `0`: 正常終了。`q`/ESC、`Ctrl-C`/`kill` による終了はもちろん、`--help`/`-h`
  や `--stats` のようにカメラを開かずに完了する実行も含む。
- `1`: 起動時のエラー(引数のパース失敗、`--stats` 実行時にログファイルが
  存在しない、ディスプレイが利用できない、カスケードファイルの読み込み失敗、
  カメラのオープン失敗)。
- `2`: セッション中にカメラが切断された(連続空フレームのしきい値を超えた)
  ことによる終了。`0`/`1` とコードを分けているのは、systemd の
  `Restart=on-failure`＋`SuccessExitStatus` や監視スクリプトなどから、
  「ユーザーが意図して終了した」場合と区別して自動再起動などの対応を
  できるようにするため。実際にこの組み合わせを使った systemd ユーザーユニットの
  サンプルを `packaging/systemd/focus-monitor.service` に用意している
  (`SuccessExitStatus=1` により、再起動が必要なのは終了コード `2` の
  ときだけになる)。`systemd-analyze verify` で構文は確認済み。

### systemd ユーザーユニットでの自動実行(任意)

GUI のプレビューウィンドウを表示するにはログインセッション自身の
`DISPLAY`/`WAYLAND_DISPLAY` が必要なため、システム全体のユニットではなく
ユーザーユニット(`~/.config/systemd/user/`)として登録する。

```sh
cp packaging/systemd/focus-monitor.service ~/.config/systemd/user/
# ExecStart / WorkingDirectory のパスを実際のチェックアウト先に合わせて編集
systemctl --user enable --now focus-monitor.service
```

`DISPLAY`/`WAYLAND_DISPLAY` がそのユーザーセッションに自動的には渡っていない
環境(ログインマネージャの設定によってはよくある)では、`focus_monitor` は
クラッシュするのではなく上記の「ディスプレイが利用できない」エラーで
終了コード `1` を返す。`SuccessExitStatus=1` によりこれは「成功」扱いとなり、
再起動を無限に繰り返すことはない。



```sh
./build/focus_monitor [cascade_path] \
    [--camera N] \
    [--grace-period SECONDS] \
    [--alert-delay SECONDS] \
    [--alert-interval SECONDS] \
    [--session-log PATH] [--no-session-log] \
    [--min-face-size PX] [--max-face-size PX] [--width PX] [--height PX] \
    [--scale-factor FACTOR] [--min-neighbors N] [--stats] [--help]
```

- `--camera N`: 使用するカメラのデバイス番号(既定 0)。負の値はエラーになる。
- `--width PX` / `--height PX`: カメラの解像度を指定する(既定はカメラ
  オープン時のデフォルト解像度のまま)。`cv::VideoCapture::set` へそのまま
  渡すベストエフォートのリクエストであり、ドライバ側が対応していない
  解像度を指定した場合は無視されたり別の値になったりすることがある
  (その場合でも検出処理自体は実際に得られたフレームサイズに対して
  正しく動作する)。0 以下の値はエラーになる。
- `--grace-period SECONDS`: 顔検出が途切れてから Not focused と判定するまでの
  許容時間(既定 2 秒)。
- `--alert-delay SECONDS`: Not focused になってから最初のアラートまでの時間
  (既定 10 秒)。
- `--alert-interval SECONDS`: 最初のアラート以降、Not focused が続く間の
  繰り返し間隔(既定 30 秒)。

  (`--grace-period`/`--alert-delay`/`--alert-interval` はいずれも 0 以上
  10^9 秒(約 31.7 年)以下の有限な数値のみ受け付ける。負の値や `nan`/`inf`
  は時間として意味を持たず、極端に大きい値も同様に、内部で
  `std::chrono::duration` に変換する際未定義動作になり得るため、
  指定した時点でエラーとして拒否する。上限は実用上あり得る値より
  はるかに大きく取っているだけで、通常の使用でこの上限に達することはない。)
- `--session-log PATH`: セッション終了時にサマリ(CSV)を追記するファイルのパス
  (既定 `focus_sessions.csv`)。
- `--no-session-log`: セッションログの記録を無効化する。
- `--min-face-size PX`: 顔検出の最小サイズ(ピクセル、正方形の一辺、既定 60)。
  0 以下の値はエラーになる。
- `--max-face-size PX`: 顔検出の最大サイズ(ピクセル、正方形の一辺、既定は
  無制限)。0 以下の値はエラーになる。`--min-face-size` より小さい値を
  指定した場合もエラーになる(検索範囲が空になり、何も検出されなくなるため)。
- `--scale-factor FACTOR`: `detectMultiScale` が探索ウィンドウを縮小していく
  倍率(既定 1.1)。値を大きくするほど探索するスケールの段階数が減り検出は
  速くなるが、サイズが段階の間に収まる顔を見落としやすくなる。1.0 以下の値は
  探索が縮小しなくなる(進行しなくなる)ため、有限かつ 1.0 より大きい値のみ
  受け付ける。
- `--min-neighbors N`: `detectMultiScale` が1つの候補領域を実際の顔として
  採用するために必要な近傍矩形数(既定 4)。値を大きくするほど誤検出は
  減るが検出漏れが増えやすくなる。1 以上の整数のみ受け付ける。
- `--stats`: カメラを一切開かず、`--session-log`(既定 `focus_sessions.csv`)の
  これまでの履歴を集計して表示し、終了する。セッション数・合計追跡時間・
  平均集中率(各セッションの長さで重み付けした平均。10分@90%のセッションは
  10秒@10%のセッションより平均への影響が大きい)を表示する。ログファイルが
  存在しない場合はエラーになるため、`--no-session-log` と併用はできない。

  ```sh
  $ ./build/focus_monitor --stats
  Sessions recorded: 12
  Total tracked time: 8640s
  Average focus rate: 78.3%
  ```
- `--help`/`-h`: 使用方法を標準出力に表示して終了する(終了コード `0`)。
  コマンドライン上のどこに書かれていても、他のオプションの妥当性に関わらず
  常に優先される(例: `--camera` に値を渡さずに `--help` も渡した場合でも、
  値不足のエラーではなく使用方法が表示される)。

### セッションログ (`focus_sessions.csv`)

各セッション終了時に以下の形式で1行追記される(ファイルが存在しない場合はヘッダ行も
書き込む):

```
ended_at,duration_seconds,focus_percent
2026-10-22T01:23:45Z,312.4,87.6
```

- `ended_at`: セッション終了時刻(UTC、ISO 8601)。
- `duration_seconds`: セッションの長さ(秒)。
- `focus_percent`: セッション全体に占める Focused 時間の割合(0-100%)。

`duration_seconds` と `focus_percent` は必ず同じ時間区間(セッション開始から
実際の終了時刻まで)を指す。`focus_percent` の計算に使う内部の集計は実際には
最後に処理したフレームの時点までしか進んでいないため、ログ書き込み直前に
その時点の状態を保ったまま終了時刻まで集計を進めてから `duration_seconds`
を算出している。

## テスト

`FocusTracker`(グレースピリオド・集中率の計算)、`AlertScheduler`(アラートの
発火タイミング)、`Config`(コマンドライン引数のパース)、`session_logger`
(セッションサマリの CSV フォーマットとファイル追記、および `--stats` が使う
`ReadSessionLogStats` による集計・長さによる重み付け平均・壊れた行のスキップ。
数値として全く読めない行だけでなく、数値の後に余分な文字列が続く行(`std::stod`
は先頭の数値部分だけを読み取って残りを無視してしまうため)も壊れた行として
スキップされることを含む)、
`EscapeForShell`
(`notify-send` コマンド文字列へ埋め込む前のシェルエスケープ)、`BuildNotifySendCommand`
(組み立てたコマンド文字列がバックグラウンド実行(末尾の `&`)と出力のリダイレクトを
含むこと、および実際に `notify-send` の代わりとなるコマンドに差し替えて実行し、
それが長時間応答しなくても呼び出しがすぐに返ってくること)、`ShutdownFlag`
(`SIGINT`/`SIGTERM` ハンドラが実際にフラグを立てること)、`ResolveCascadePath`
(カスケードファイルの探索順)、`ConsecutiveFailureGuard`(連続失敗回数のしきい値
判定)、`display_format`(画面オーバーレイの2行 -- `FormatStatusLine`/
`FormatRatioLine` -- のフォーマット。パーセント表示は四捨五入ではなく切り捨て
であることも含む)、`DisplayAvailable`(`DISPLAY`/`WAYLAND_DISPLAY` のどちらかが
空でない値で設定されていれば利用可能と判定するロジック)は、カメラや OpenCV に
依存しない純粋なロジックなので、外部テスト
フレームワークを追加せず、`tests/test_framework.h` の小さな自作ハーネスと
CTest だけでテストしている。もともと `ResolveCascadePath` は
`cv::utils::fs::exists` を使っていたため main.cpp 内に未テストのまま残って
いたが、`std::filesystem::exists` に置き換えて `src/cascade_resolver.h` へ
切り出すことで OpenCV 依存を切り、テスト可能にした。`display_format` も同様に、
もとは main.cpp 内にインラインで書かれていてテストが無かったロジックを
`src/display_format.h` へ切り出したもの。
`EscapeForShell` のテストは実際にシェル経由でコマンドを実行し、エスケープ後の
文字列が元の文字列へ正しくラウンドトリップすること、および悪意ある入力(`"`を
含む文字列)がダブルクオートを抜け出してコマンドインジェクションを起こさない
ことまで確認する。

```sh
cd build
ctest --output-on-failure
```
