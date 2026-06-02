@echo off
setlocal

pushd "%~dp0" || exit /b 1

where clang >nul 2>nul
if errorlevel 1 (
    echo clang.exe was not found on PATH.
    echo Run this from a Visual Studio tools command prompt with clang installed.
    popd
    exit /b 1
)

clang ^
  -O3 ^
  -march=native ^
  -D_CRT_SECURE_NO_WARNINGS ^
  -Wall ^
  -Wextra ^
  -Wno-unused-parameter ^
  iterator_gdi.c ^
  iterator_mru_storage.c ^
  iterator_mru_session.c ^
  iterator_mru_bifview.c ^
  iterator_mru_cycle.c ^
  iterator_mru_stable_cycle.c ^
  -luser32 ^
  -lgdi32 ^
  -Xlinker /SUBSYSTEM:WINDOWS ^
  -o iterator_gdi.exe

set "rc=%errorlevel%"
if "%rc%"=="0" (
    echo Built iterator_gdi.exe
) else (
    echo Build failed with exit code %rc%.
)

popd
exit /b %rc%
