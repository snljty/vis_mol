@echo off

set CMAKE_PREFIX_PATH=C:/freeglut-3.8.0;C:/uthash;C:/glad
cmake . -G "MinGW Makefiles" -B build -D CMAKE_INSTALL_PREFIX=%~dp0 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUSE_HIGH_RESOLUTION=ON -LH
rem cmake --build build -j
rem cmake --install build 
