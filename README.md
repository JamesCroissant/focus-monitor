# focus-monitor

C++ / OpenCV を用いて、カメラ映像から人の顔を検出し、パソコンにきちんとフォーカスしているかをチェックするアプリ。

## 現状

- カメラ映像を取得してウィンドウに表示するだけの最小構成。
- 顔検出・フォーカス判定ロジックは未実装（今後追加予定）。

## 必要環境

- CMake 3.16 以上
- C++17 対応コンパイラ
- OpenCV (`libopencv-dev` など)

## ビルド方法

```sh
mkdir build && cd build
cmake ..
cmake --build .
```

## 実行方法

```sh
./build/focus_monitor
```

`q` キーまたは ESC キーで終了します。
