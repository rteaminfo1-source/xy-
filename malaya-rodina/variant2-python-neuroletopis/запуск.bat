@echo off
cd /d "%~dp0"
where py >nul 2>nul && (py -3 main.py & exit /b)
where python >nul 2>nul && (python main.py & exit /b)
echo Python not found. Install Python 3.8+ from https://www.python.org/downloads/ (check "Add to PATH").
pause
