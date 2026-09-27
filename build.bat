@echo off
rem MinGW (nuwen) + CMake, from a Windows cwd; run from WSL as: cmd.exe /c build.bat
call C:\MinGW\set_distro_paths.bat >nul
set "CMAKE=C:\Program Files\CMake\bin\cmake.exe"
"%CMAKE%" -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release || exit /b 1
"%CMAKE%" --build build || exit /b 1
echo built build\hotswap.exe
