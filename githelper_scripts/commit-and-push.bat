@echo off
REM Commit all changes und pushe auf den aktuellen Branch
set /p MSG="Commit message: "
git add -A
git commit -m "%MSG%"
git push
pause 