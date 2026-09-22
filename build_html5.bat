echo off
cls

mkdir build_html5
cd build_html5

emcmake cmake .. -G Ninja

cd ..

cmake --build build_html5