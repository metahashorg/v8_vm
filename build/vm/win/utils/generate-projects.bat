:: Copyright 2018 the AdSniper project authors. All rights reserved.
::
::

:: Command line parameters:
:: %1 is options for generating project (must be)
:: %2 is a destination folder (must be)
:: e.g. generate-projects.bat "is_debug=false target_cpu=\"x64\"" out\win\release_x64

@echo off

:: Check Windows 10 SDK
call %~dp0check-windows-sdk.bat

:: Check and get depots_tools
call %~dp0check-and-get-depot-tools.bat

:: Set DEPOT_TOOLS_WIN_TOOLCHAIN to 0
:: This tells depot_tools to use your locally installed version of Visual Studio
set DEPOT_TOOLS_WIN_TOOLCHAIN=0

:: Go into a root folder of source codes
cd %~dp0..\..\..\..

echo Generate project files (options: %1; folder: %2; %DATE% %TIME:~0,8%)...

:: Generate all projects
call gn gen --ide=vs --filters=//* %2 --args=%1
if %errorlevel% neq 0 (
  echo ------- Generation of projects failed ^(%DATE% %TIME:~0,8%^) -------
  exit %errorlevel%
)

echo ------- Generation of projects is successful ^(%DATE% %TIME:~0,8%^) -------

exit /b 0
