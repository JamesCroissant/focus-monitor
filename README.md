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

`q` キーまたは ESC キーで終了します。

以下のオプションで既定値を変更できる(カスケードパスの指定は従来通り第1引数):

```sh
./build/focus_monitor [cascade_path] \
    [--camera N] \
    [--grace-period SECONDS] \
    [--alert-delay SECONDS] \
    [--alert-interval SECONDS]
```

- `--camera N`: 使用するカメラのデバイス番号(既定 0)。
- `--grace-period SECONDS`: 顔検出が途切れてから Not focused と判定するまでの
  許容時間(既定 2 秒)。
- `--alert-delay SECONDS`: Not focused になってから最初のアラートまでの時間
  (既定 10 秒)。
- `--alert-interval SECONDS`: 最初のアラート以降、Not focused が続く間の
  繰り返し間隔(既定 30 秒)。

## テスト

`FocusTracker`(グレースピリオド・集中率の計算)、`AlertScheduler`(アラートの
発火タイミング)、`Config`(コマンドライン引数のパース)は、カメラや OpenCV に
依存しない純粋なロジックなので、外部テストフレームワークを追加せず、
`tests/test_framework.h` の小さな自作ハーネスと CTest だけでテストしている。

```sh
cd build
ctest --output-on-failure
```
