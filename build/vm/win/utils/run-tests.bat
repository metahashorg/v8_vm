:: Copyright 2018 the AdSniper project authors. All rights reserved.
::
::

:: Command line parameters:
:: The only one parameter is a folder for running test (must be)
:: e.g. run-tests.bat out\win\debug_x64_sharedlibs

@echo off

:: Check and get depots_tools
call %~dp0check-and-get-depot-tools.bat

:: Go into a root folder of source codes
cd %~dp0..\..\..\..

echo Run tests (folder: %1; %DATE% %TIME:~0,8%)...

:: Run tests
call python tools\run-tests.py --outdir %1
if %errorlevel% neq 0 (
  echo ------- Running of tests failed ^(%DATE% %TIME:~0,8%^) -------
  exit %errorlevel%
)

echo ------- Running of tests is successful (%DATE% %TIME:~0,8%) -------

exit /b 0
