echo off

IF NOT EXIST build (
    mkdir build
)

cmake -B ./build . -DCMAKE_C_COMPILER="C:/msys64/mingw64/bin/gcc.exe" -DCMAKE_CXX_COMPILER="C:/msys64/mingw64/bin/g++.exe" -DCMAKE_MAKE_PROGRAM="C:/msys64/usr/bin/make.exe" -G "MSYS Makefiles"

xcopy .\build\compile_commands.json .\compile_commands.json /Y

cd build
make

cd ..
glslangValidator -S vert -e main -o resources/shaders/main_vs.spv -V resources/shaders/main_vs.glsl
glslangValidator -S frag -e main -o resources/shaders/main_fs.spv -V resources/shaders/main_fs.glsl
cp -r resources build/
