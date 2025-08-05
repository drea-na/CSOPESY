@echo off
echo Starting MachineProject test...
echo.
echo Test scenario:
echo 1. Type 'scheduler-start'
echo 2. Wait 10 seconds
echo 3. Type 'scheduler-stop'
echo 4. Wait 30 seconds
echo 5. Type 'vmstat'
echo 6. Check backing store files
echo.
echo Expected: High 'num paged in' and 'num paged out' values
echo.
pause
echo Starting program...
MachineProject.exe 