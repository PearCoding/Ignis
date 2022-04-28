@echo off

REM ***************************************************************
REM * This script adds Ignis to the current path on Windows.
REM * It assumes that Ignis is either compiled within a subdirectory named 'build'
REM * or within a subdirectory named 'build/Release'.
REM ***************************************************************

set IGNIS_DIR=%~dp0
set IGNIS_DIR=%IGNIS_DIR:~0,-1%
set PATH=%PATH%;%IGNIS_DIR%\build\bin;%IGNIS_DIR%\build\Release\bin
set PATH=%PATH%;%IGNIS_DIR%\build\lib;%IGNIS_DIR%\build\Release\lib
set PYTHONPATH=%PYTHONPATH%;%IGNIS_DIR%\build\api;%IGNIS_DIR%\build\Release\api
