@echo off
set ROOT=%~dp0
set QT=C:\Qt\6.11.2\mingw_64
set MINGW=C:\Qt\Tools\mingw1310_64\bin
set NINJA=C:\Qt\Tools\Ninja
set PATH=%MINGW%;%NINJA%;%QT%\bin;%PATH%
cmake -S "%ROOT%" -B "%ROOT%build" -G Ninja -DCMAKE_PREFIX_PATH=%QT% -DCMAKE_BUILD_TYPE=Debug
cmake --build "%ROOT%build"
