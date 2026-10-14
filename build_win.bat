@echo off
rem Build xfollowing (Release x64) with VS2026 + Qt 6.10.3 MSVC + CEF 154
rem Output is deployed to BOTH:
rem   D:\program\xfollowing\xfollowing
rem   D:\program\xfollowing\xfollowing-release

call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

set "PATH=D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
rem VS-bundled CMake 4.3.1 crashes in cmake_autogen (access violation); prefer standalone CMake when present
if exist "D:\program\xfollowing\tools\cmake\bin\cmake.exe" set "PATH=D:\program\xfollowing\tools\cmake\bin;%PATH%"

cd /d D:\program\xfollowing

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build build --target xfollowing
if errorlevel 1 exit /b 1

rem Bundle MSVC runtime DLLs so the portable folder runs on PCs without VC++ redistributable
set "OUT=D:\program\xfollowing\xfollowing"
set "OUT_RELEASE=D:\program\xfollowing\xfollowing-release"
if not exist "%OUT_RELEASE%" mkdir "%OUT_RELEASE%"
set "SYS32=%SystemRoot%\System32"
for %%F in (
  msvcp140.dll msvcp140_1.dll msvcp140_2.dll msvcp140_atomic_wait.dll msvcp140_codecvt_ids.dll
  vcruntime140.dll vcruntime140_1.dll concrt140.dll vccorlib140.dll
) do (
  if exist "%SYS32%\%%F" copy /Y "%SYS32%\%%F" "%OUT%\" >nul
)

if not exist "%OUT%\xfollowing.exe" (
  echo ERROR: missing %OUT%\xfollowing.exe
  exit /b 1
)

rem Sync program files to both dirs; keep user data in xfollowing-release intact
rem CMake POST_BUILD already fills %OUT%; mirror to %OUT_RELEASE%
robocopy "%OUT%" "%OUT_RELEASE%" /E /XD data userdata logs backups userlist /XF settings.ini *.rar /NFL /NDL /NJH /NJS /nc /ns /np
set "RC=%ERRORLEVEL%"
if %RC% GEQ 8 (
  echo ERROR: robocopy to xfollowing-release failed, code=%RC%
  exit /b 1
)

echo Deployed program files to:
echo   %OUT%
echo   %OUT_RELEASE%
echo BUILD_OK
