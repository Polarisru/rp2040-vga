@echo off
setlocal

set BUILD_DIR=%~dp0build
set SRC_DIR=%~dp0.
set BOARD=pico
set JOBS=%NUMBER_OF_PROCESSORS%

:: Add MSYS2 MinGW64 before legacy MinGW in PATH
set PATH=C:\msys64\mingw64\bin;%PATH%

where cmake >nul 2>&1 || (echo ERROR: cmake not found & exit /b 1)
where ninja >nul 2>&1
if errorlevel 1 (
    set GENERATOR=MinGW Makefiles
) else (
    set GENERATOR=Ninja
)
where arm-none-eabi-gcc >nul 2>&1 || (echo ERROR: arm-none-eabi-gcc not found & exit /b 1)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo.
echo === Configuring ===
echo.

if defined PICO_SDK_PATH (
    echo Using local Pico SDK: %PICO_SDK_PATH%
    cmake -G "%GENERATOR%" -S "%SRC_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -DPICO_BOARD=%BOARD% -DPICO_SDK_PATH=%PICO_SDK_PATH%
) else (
    echo PICO_SDK_PATH not set - SDK will be fetched from GitHub automatically.
    cmake -G "%GENERATOR%" -S "%SRC_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -DPICO_BOARD=%BOARD%
)

if errorlevel 1 (echo. & echo ERROR: CMake configure failed. & exit /b 1)

echo.
echo === Building ===
echo.
cmake --build "%BUILD_DIR%" --config Release --parallel %JOBS%
if errorlevel 1 (echo. & echo ERROR: Build failed. & exit /b 1)

echo.
echo === Build complete ===
for %%F in ("%BUILD_DIR%\*.uf2") do echo UF2: %%F
echo.
echo Flash by holding BOOTSEL, plug in Pico, then copy the .uf2 file.

endlocal
