@echo off
REM Commit all changes mit Nachricht
set /p MSG="Commit message: "
git add -A
git commit -m "%MSG%"
pause 