@echo off

set ProjectDir=%~dp0
set Inc=%ProjectDir%include\
set Lib=%ProjectDir%lib\

set Libraries=..\..\Libraries\

::============================================
:: Create links
::============================================

call :Link_LIB      		IRremote            .
call :Link_LIB      		NYG

call :Link_NYG              NYG
call :Link_NYG              Defines
call :Link_NYG              Enums
call :Link_NYG              Types
call :Link_NYG              SystemFacilities
call :Link_NYG              Logger

call :Link_NYG              ITextOutput
call :Link_NYG              Timer
call :Link_NYG              IOutput
call :Link_NYG              RGB_Led
call :Link_NYG              Toggler
call :Link_NYG              StdIR
call :Link_NYG              Cfg
call :Link_NYG              Random
call :Link_NYG              RGB_Led

pause

goto :eof


::---------------------------
:Link_LIB
::---------------------------
if exist %Lib%%1        rmdir  /s/q %Lib%%1
if exist %Inc%%1        rmdir  /s/q %Inc%%1

mklink /J %Lib%%1       %Libraries%%1
mkdir     %Inc%%1

if "%2"==""         goto :eof

set SOURCE_DIR=%Libraries%%1\
set TARGET_DIR=%Inc%%1\

if not "%2"=="."    (
    set SOURCE_DIR=%SOURCE_DIR%%2\
    set TARGET_DIR=%TARGET_DIR%%2\
    )

if not exist %TARGET_DIR%   mkdir %TARGET_DIR%

for %%a in (%SOURCE_DIR%*.h) do (
    call :Link_H %%a
    )

goto :eof

::---------------------------
:Link_H
::---------------------------
set SOURCE=%1

For %%A in ("%SOURCE%") do (
    Set TARGET=%TARGET_DIR%%%~nxA
)

mklink /H %TARGET%  %SOURCE%

goto :eof

::---------------------------
:Link_NYG
::---------------------------
dir %Libraries%NYG\%1.h
mklink /H %Inc%NYG\%1.h   %Libraries%NYG\%1.h

goto :eof

::---------------------------
:MkLink_INC
::---------------------------
call :MkLink    %Inc%%1     %2
goto :eof

::---------------------------
:MkLink
::---------------------------
if exist %Libraries%%2   (
    mklink %1   %Libraries%%2
    ) else (
    echo %Libraries%%2 not found.
    )
goto :eof

