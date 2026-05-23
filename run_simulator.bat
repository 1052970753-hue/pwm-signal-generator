@echo off
cd /d "%~dp0"
echo ========================================
echo   PWM Signal Generator - Simulator
echo ========================================
echo.
pip install PyQt6 pyserial 2>nul
python simulator\run_simulator.py
pause
