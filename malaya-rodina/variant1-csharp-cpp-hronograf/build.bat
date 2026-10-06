@echo off
setlocal
cd /d "%~dp0"
echo.
echo  ==== HRONOGRAF: build C++ engine + run C# app ====
echo.

where dotnet >nul 2>nul
if errorlevel 1 (
  echo [!] .NET 8 SDK not found. Download: https://dotnet.microsoft.com/download/dotnet/8.0
  pause
  exit /b 1
)

set "OUT=Engine\bin\x64\Release"
if not exist "%OUT%" mkdir "%OUT%"

rem ---- 1) C++ engine: Visual Studio (MSVC) ----
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
)
if defined VSDIR goto :msvc

rem ---- 1b) C++ engine: MinGW g++ ----
where g++ >nul 2>nul
if not errorlevel 1 goto :mingw

echo [i] C++ compiler not found - using prebuilt engine Engine\prebuilt\win-x64
goto :csharp

:msvc
echo [1/2] C++ engine (MSVC)...
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /EHsc /std:c++17 /utf-8 /MT /LD Engine\engine.cpp /Fe:%OUT%\hronograf_engine.dll /Fo:%OUT%\ 
if errorlevel 1 goto :fail
goto :csharp

:mingw
echo [1/2] C++ engine (MinGW g++)...
g++ -std=c++17 -O2 -shared -static -o %OUT%\hronograf_engine.dll Engine\engine.cpp
if errorlevel 1 goto :fail
goto :csharp

:csharp
echo [2/2] C# app...
dotnet run --project App\Hronograf.csproj -c Release
exit /b 0

:fail
echo [!] C++ engine build failed.
pause
exit /b 1
