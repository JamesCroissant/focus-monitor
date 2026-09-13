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
