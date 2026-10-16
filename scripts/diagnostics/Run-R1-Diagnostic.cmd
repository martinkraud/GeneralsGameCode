@echo off
rem X64.41 R1 diagnostic recording (human step). Put this file next to generalsv_r1diag.exe (the staged game folder) and double-click it.
rem It starts the game with a CRC every logic frame and a per-frame / per-object CRC trace, and keeps the replay afterwards.
rem Then: start a Skirmish, select the dozer, build ONE structure, wait about 40 seconds, Exit Game, quit. Nothing else is needed.
setlocal
cd /d "%~dp0"
if not exist generalsv_r1diag.exe (
  echo generalsv_r1diag.exe was not found next to this file.
  pause
  exit /b 1
)
set GENERALS_REPLAY_CRC_INTERVAL=1
set GENERALS_CRC_TRACE=%~dp0r1_live.trace
set GENERALS_CRC_TRACE_OBJECTS=1
if exist "%~dp0r1_live.trace" del /q "%~dp0r1_live.trace"
if exist "%~dp0r1_live.rep" del /q "%~dp0r1_live.rep"
generalsv_r1diag.exe
set REPDIR=%USERPROFILE%\Documents\Command and Conquer Generals Data\Replays
if exist "%REPDIR%\00000000.rep" (
  copy /y "%REPDIR%\00000000.rep" "%~dp0r1_live.rep" >nul
  echo Saved: r1_live.rep and r1_live.trace in %~dp0
) else (
  echo No "Last Replay" was found in %REPDIR%
)
pause
