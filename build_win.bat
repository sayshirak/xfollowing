@echo off
rem Build xfollowing (Release x64) with VS2026 + Qt 6.10.3 MSVC + CEF 154
rem Output goes to D:\program\xfollowing\xfollowing

call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

set "PATH=D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

cd /d D:\program\xfollowing

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=D:/program/xfollowing/xfollowing
if errorlevel 1 exit /b 1

cmake --build build
if errorlevel 1 exit /b 1

echo BUILD_OK
