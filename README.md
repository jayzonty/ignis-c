# Ignis
[Japanese (日本語版)](./README.ja-JP.md)

## Overview
Ignis is a C game library inspired by [Raylib](https://www.raylib.com/) API, with the Vulkan API as the backend renderer.<br/>
This project is a personal attempt of making a framework for creating games.

## Features
- Window creation
- Key/mouse input processing
- Primitive shape rendering (lines, circles, rectangles)
- Texture loading and rendering
- Font loading and text rendering
- Basic random number generation (int, float)
- Vector math API
- Bezier curves rendering

## Samples

Code for sample games can be found below:
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
