@echo off
rem PurrGO: OSM -> optimizer -> map package V4 with search index.
rem
rem Usage: compile-v4.cmd input.osm [output_dir]
rem
rem Output folder default: <this script's folder>\<input name>-v4
rem The map name (map.name) is the input file name without extension.
rem For .osm.pbf convert first: osmium cat in.osm.pbf -o in.osm
setlocal EnableExtensions

if "%~1"=="" goto usage
if not "%~3"=="" goto usage
if not exist "%~1" goto notfound

set "DIR=%~dp0"
set "INPUT=%~f1"
set "NAME=%~n1"
if /I "%~x1"==".pbf" goto pbf
set "OUT=%DIR%%NAME%-v4"
if not "%~2"=="" set "OUT=%~f2"

set "PY=python"
if not "%PYTHON%"=="" set "PY=%PYTHON%"

set "WORK=%TEMP%\purrgo-v4-%RANDOM%%RANDOM%"
mkdir "%WORK%" || exit /b 1

echo [1/2] Optimizing OSM
"%PY%" "%DIR%purrgo_osm_optimizer.py" "%INPUT%" "%WORK%\%NAME%.osm"
if errorlevel 1 goto fail

echo [2/2] Compiling map V4 with search index -^> "%OUT%"
"%PY%" "%DIR%purrgo_map_compiler.py" "%WORK%\%NAME%.osm" --format 4 --search -o "%OUT%"
if errorlevel 1 goto fail

rmdir /S /Q "%WORK%"
echo Done: "%OUT%"
exit /b 0

:fail
set "RC=%ERRORLEVEL%"
rmdir /S /Q "%WORK%"
exit /b %RC%

:notfound
echo Error: %~1 not found 1>&2
exit /b 1

:pbf
echo Error: .pbf is not supported here, convert with "osmium cat" first 1>&2
exit /b 1

:usage
echo Usage: %~nx0 input.osm [output_dir] 1>&2
exit /b 2
