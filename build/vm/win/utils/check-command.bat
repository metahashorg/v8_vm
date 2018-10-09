:: Copyright 2018 the AdSniper project authors. All rights reserved.
::
::

:: Command line parameters:
:: The only one parameter is a command for checking (must be)
:: e.g. check-command.bat git

@echo off

call where %1 >nul 2>&1
if %errorlevel% neq 0 exit /b 1

exit /b 0
