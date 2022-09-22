#!/bin/sh

# Create build directory if it doesn't exist yet
if [ ! -d ./build ]; then
    mkdir build
fi

# CMake
cmake -B ./build .

# Copy compile_commands.json from the build folder to the working directory
cp ./build/compile_commands.json ./compile_commands.json

cd build
make

cd ..
glslangValidator -S vert -e main -o resources/shaders/main_vs.spv -V resources/shaders/main_vs.glsl
glslangValidator -S frag -e main -o resources/shaders/main_fs.spv -V resources/shaders/main_fs.glsl
cp -r resources build/
