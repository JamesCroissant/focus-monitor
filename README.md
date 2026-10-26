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
  バックする。
- `FocusTracker`/`AlertScheduler` はカメラ・OpenCV に依存しないロジックのみなので、
  単体テストを用意している(`tests/`、詳細は下記「テスト」節)。
- push / pull request のたびに GitHub Actions でビルド＋テストを自動実行する
  (`.github/workflows/build-and-test.yml`)。カメラが無い CI 環境でも `focus_monitor`
  本体のビルド確認と、カメラに依存しない単体テストの実行はできる。
- カメラ番号・グレースピリオド・アラートの delay / repeat interval はコマンドライン
  オプションで変更できる(`src/config.h`、詳細は下記「実行方法」節)。パース処理は
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

以下のオプションで既定値を変更できる(カスケードパスの指定は従来通り第1引数):

```sh
./build/focus_monitor [cascade_path] \
    [--camera N] \
    [--grace-period SECONDS] \
    [--alert-delay SECONDS] \
    [--alert-interval SECONDS] \
    [--session-log PATH] [--no-session-log] \
    [--min-face-size PX]
```

- `--camera N`: 使用するカメラのデバイス番号(既定 0)。
- `--grace-period SECONDS`: 顔検出が途切れてから Not focused と判定するまでの
  許容時間(既定 2 秒)。
- `--alert-delay SECONDS`: Not focused になってから最初のアラートまでの時間
  (既定 10 秒)。
- `--alert-interval SECONDS`: 最初のアラート以降、Not focused が続く間の
  繰り返し間隔(既定 30 秒)。

  (`--grace-period`/`--alert-delay`/`--alert-interval` はいずれも 0 以上の
  有限な数値のみ受け付ける。負の値や `nan`/`inf` は時間として意味を持たず、
  内部で `std::chrono::duration` に変換する際未定義動作になり得るため、
  指定した時点でエラーとして拒否する。)
- `--session-log PATH`: セッション終了時にサマリ(CSV)を追記するファイルのパス
  (既定 `focus_sessions.csv`)。
- `--no-session-log`: セッションログの記録を無効化する。
- `--min-face-size PX`: 顔検出の最小サイズ(ピクセル、正方形の一辺、既定 60)。
  0 以下の値はエラーになる。

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

## テスト

`FocusTracker`(グレースピリオド・集中率の計算)、`AlertScheduler`(アラートの
発火タイミング)、`Config`(コマンドライン引数のパース)、`session_logger`
(セッションサマリの CSV フォーマットとファイル追記)、`EscapeForShell`
(`notify-send` コマンド文字列へ埋め込む前のシェルエスケープ)、`ShutdownFlag`
(`SIGINT`/`SIGTERM` ハンドラが実際にフラグを立てること)は、カメラや
OpenCV に依存しない純粋なロジックなので、外部テストフレームワークを追加せず、
`tests/test_framework.h` の小さな自作ハーネスと CTest だけでテストしている。
`EscapeForShell` のテストは実際にシェル経由でコマンドを実行し、エスケープ後の
文字列が元の文字列へ正しくラウンドトリップすること、および悪意ある入力(`"`を
含む文字列)がダブルクオートを抜け出してコマンドインジェクションを起こさない
ことまで確認する。

```sh
cd build
ctest --output-on-failure
```
