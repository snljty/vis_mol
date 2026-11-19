@echo off

set CMAKE_PREFIX_PATH=C:/freeglut-3.8.0;C:/glad-4.6;C:/uthash-2.4.0;
cmake . -G"MinGW Makefiles" -Bbuild -DCMAKE_INSTALL_PREFIX=%~dp0 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUSE_STATIC_FREEGLUT=ON -DUSE_HIGH_RESOLUTION=ON -LH
rem cmake --build build -j
rem cmake --install build 
