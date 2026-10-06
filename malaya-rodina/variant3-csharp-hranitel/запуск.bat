@echo off
cd /d "%~dp0"
where dotnet >nul 2>nul
if errorlevel 1 (
  echo .NET 8 SDK not found. Download: https://dotnet.microsoft.com/download/dotnet/8.0
  pause
  exit /b 1
)
dotnet run -c Release
