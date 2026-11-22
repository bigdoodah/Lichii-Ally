@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0lichii.exe" (
  echo Keep this launcher beside Performance9's lichii.exe.
  pause
  exit /b 1
)
set "EDEN_NX2_SAMPLE="
set "EDEN_NX2_SAMPLE_PROFILE="
set "EDEN_NX2_DEBUG_SERVER="
set "EDEN_NX2_TIMELINE="
set "EDEN_NX2_TIMELINE_DETAIL="
set "EDEN_NX2_TIMELINE_GUEST="
set "EDEN_NX2_EXP49=1"
set "EDEN_NX2_GPU_PROFILE=1"
set "EDEN_NX2_LIST_WAIT_TRACE=1"
set "EDEN_DKB_FRAME_TRACE=1"
echo Performance9 GPU wait tracing: play and close normally.
start "" "%~dp0lichii.exe"
endlocal
