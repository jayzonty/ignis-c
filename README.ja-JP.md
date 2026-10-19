# Ignis
[英語版 (English)](./README.md)

## 概要
「Ignis」は、Vulkan APIをバックエンドレンダラーとして使用し、[Raylib](https://www.raylib.com/) APIを参考にしたC言語ゲームライブラリです。
本プロジェクトは、ゲーム制作向けフレームワークの構築を目的とした個人プロジェクトです。

## 機能
- ウィンドウの作成
- キーボードのキーとマウスボタンのインプット処理
- プリミティブ形状の描画（線, 円, 長方形）
- テクスチャーの読み込み・描画
- フォントの読み込みおよびテキスト描画
- 基本的な乱数生成（int、float）
- ベクトル演算API
- ベジェ曲線の描画

## サンプルゲーム

サンプルゲームのコードは以下になります。
- [Pong](./sandbox/pong.c)
- [Breakout](./sandbox/breakout.c)

<!--
## Setup

Fedora

    sudo dnf install cmake clang

## Dependencies

Fedora

    sudo dnf install glslang vulkan-headers vulkan-loader-devel vulkan-validation-layers-devel vulkan-tools libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
-->
