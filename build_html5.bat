echo off
cls

mkdir build_html5
cd build_html5

cmake .. ^
-DEMSCRIPTEN_ROOT_PATH="C:\Dev\emsdk\upstream\emscripten" ^
-DCMAKE_TOOLCHAIN_FILE="C:\Dev\emsdk\upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake" ^
-G "MinGW Makefiles" 

cd ..

cmake --build build_html5