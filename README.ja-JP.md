# Ignis

[英語版 (English)](./README.md)

## 概要
本プロジェクトでは、[Raylib](https://www.raylib.com/)のAPI設計を参考にしながら、バックエンドレンダラーにVulkanを使用してC言語で2Dゲームフレームワークを作ってみました。

## 機能
1. 2Dバッチ描画
   - ドローコールを抑えるため、スプライトや図形をバッチする。
   - スプライトや図形を特定の深度／レイヤーに描画できるよう深度バッファを利用。
2. Raylib風の描画ループ
   - シンプルなイミディエイトモード風の描画フロー（`BeginDrawing`、`Draw...`、`EndDrawing`）。
   - フレームレート制限に対応。
   - 毎フレームのデルタタイムおよび総経過時間の取得。
3. 基本図形と曲線の描画
   - 塗りつぶし矩形および枠線矩形の描画（サイズ、色、原点ピボットを指定可能）。
   - 線の太さを指定できる線分描画や円の描画。
   - 事前計算したパスカルの三角形を用いてベジェ曲線を計算・描画。
   - ベジェ曲線に沿った接線ベクトルの算出（移動方向の計算等に利用可能）。
4. テクスチャとテキスト描画
   - 画像（PNG、JPG、BMP）を読み込み、Vulkanテクスチャへの転送。
   - TTFフォントから文字グリフを実行時テクスチャアトラスに焼き込み。
   - 整列やUIレイアウトのためのテキストサイズ計測。
5. 2D数学と当たり判定
   - Vector2演算（加算、減算、スカラー倍、内積、正規化、距離、射影、反射）。
   - 矩形同士（AABB vs AABB）、円同士（Circle vs Circle）、円と矩形（Circle vs AABB）の重なり判定。
6. 入力とユーティリティ
   - キーボードおよびマウスボタンの状態取得（`IsKeyPressed`、`IsKeyDown`、`IsKeyReleased`、`IsKeyUp`）。
   - マウスカーソル座標の取得。
   - シード指定可能な乱数生成（整数・浮動小数点数）。

## サンプルゲーム
`sandbox/`ディレクトリに、本ライブラリを使った2つのサンプルゲームを収録しています。
- Pong ([sandbox/pong.c](./sandbox/pong.c))<br/>
  パドル操作、ボールの跳ね返り、スコア表示を備えたシンプルな2人対戦ゲーム。
  ![Pong](./doc/pong.gif)
- Breakout ([sandbox/breakout.c](./sandbox/breakout.c))<br/>
  パドルでの打ち返し、矩形ブロックとの衝突・破壊、残機管理を備えたブロック崩しゲーム。
  ![Breakout](./doc/breakout.gif)

## 技術スタックと依存関係
- **言語**: C11
- **グラフィックスAPI**: Vulkan
- **ウィンドウ / 入力**: [GLFW](https://www.glfw.org/) 3.3.8（`src/deps`に同梱）
- **画像読み込み**: [stb_image](https://github.com/nothings/stb)
- **フォント描画**: [stb_truetype](https://github.com/nothings/stb)
- **シェーダーコンパイラ**: [glslangValidator](https://github.com/KhronosGroup/glslang)（GLSLからSPIR-Vへ変換）
- **ビルドシステム**: CMake (3.10以上)

## プロジェクト構成

```text
|-- include/
|   |-- ignis/
|   |   |-- ignis.h              # ライブラリの公開API（ウィンドウ、入力、図形、テキスト、テクスチャ）
|   |   |-- ignis_bezier.h       # ベジェ曲線の計算と接線算出
|   |   |-- ignis_math.h         # Vector2演算、当たり判定、ユーティリティ
|-- src/
|   |-- deps/
|   |   |-- glfw-3.3.8/          # 同梱されているGLFWのソースコード
|   |-- dynamic_array.c          # シンプルな動的配列の実装
|   |-- dynamic_array.h          # 動的配列のヘッダー
|   |-- ignis.c                  # エンジンコア、入力処理、フォントアトラス管理
|   |-- ignis_bezier.c           # パスカルの三角形を用いたベジェ曲線の実装
|   |-- ignis_bezier_internal.h  # ベジェ曲線の内部定義
|   |-- ignis_graphics.c         # Vulkan 2Dバッチレンダラーと描画パイプライン
|   |-- ignis_graphics.h         # 描画サブシステムのヘッダー
|   |-- ignis_math.c             # ベクトル演算と当たり判定の実装
|   |-- ignis_vulkan.c           # Vulkanバッファ・イメージ作成等の補助関数
|   |-- ignis_vulkan.h           # Vulkan補助関数のヘッダー
|   |-- stb_image.h              # 画像読み込みライブラリ
|   |-- stb_truetype.h           # TTFフォントラスタライザ
|   |-- CMakeLists.txt           # ライブラリビルド設定
|-- sandbox/
|   |-- resources/
|   |   |-- fonts/               # フォントファイル（FiraMono-Regular.ttf）
|   |   |-- shaders/             # GLSLシェーダーコード
|   |-- breakout.c               # ブロック崩しのサンプルコード
|   |-- CMakeLists.txt           # サンドボックスビルド設定
|   |-- pong.c                   # Pongのサンプルコード
|-- build.bat                    # Windows向けビルドスクリプト
|-- build.sh                     # Linux向けビルドスクリプト
|-- CMakeLists.txt               # ルートCMakeビルド設定
```

## ビルドと実行

### 前提条件
- CMake 3.10 以上
- C11 対応コンパイラ（GCC、Clang、または MSVC）
- Vulkan SDK（ヘッダー、ローダー、`glslangValidator`）
- Linux（Fedora）の場合:
  ```bash
  sudo dnf install cmake clang glslang vulkan-headers vulkan-loader-devel vulkan-validation-layers-devel vulkan-tools libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
  ```
- Linux（Ubuntu / Debian）の場合:
  ```bash
  sudo apt install cmake clang glslang-tools libvulkan-dev vulkan-validationlayers-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
  ```

### ビルド手順

#### Linux / macOS / WSL の場合:
```bash
./build.sh
cd build/sandbox
./pong
# または
./breakout
```

#### Windows (CMake & Visual Studio / MinGW) の場合:
```bat
build.bat
cd build\sandbox
pong.exe
# または
breakout.exe
```
