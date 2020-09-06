@echo off

REM ***************************************************************
REM * This script adds Mitsuba to the current path on Windows.
REM * It assumes that Mitsuba is either compiled within the 
REM * source tree or within a subdirectory named 'build'.
REM ***************************************************************

set MITSUBA_DIR=%~dp0
set MITSUBA_DIR=%MITSUBA_DIR:~0,-1%
set PATH=%MITSUBA_DIR%;%PATH%
set PYTHONPATH=%MITSUBA_DIR%/python;%PYTHONPATH%
