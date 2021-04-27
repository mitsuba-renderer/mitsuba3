@echo off

REM ***************************************************************
REM * This script adds Mitsuba to the current path on Windows.
REM * It assumes that Mitsuba is compiled within a subdirectory
REM * named 'build'.
REM ***************************************************************

set MITSUBA_DIR=%~dp0
set PATH=%MITSUBA_DIR%;%PATH%
set PYTHONPATH=%MITSUBA_DIR%/python;%PYTHONPATH%
