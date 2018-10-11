:: Copyright 2018 the MetaHash project authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

@echo off

:check_depot_tools
:: Check commands 'gn', 'ninja' and 'python' it's enough
call %~dp0check-command.bat gn
if %errorlevel% neq 0 goto :get_depot_tools

call %~dp0check-command.bat ninja
if %errorlevel% neq 0 goto :get_depot_tools

call %~dp0check-command.bat python
if %errorlevel% neq 0 goto :get_depot_tools

exit /b 0

:: Try to get depots_tools from a git repository
:get_depot_tools

:: Go into a root folder of project
cd %~dp0..\..\..\..\..

:: Try to add a depot_tools' folder and recheck
if [%checked_folder_for_clone%] equ [] set PATH=%CD%\depot_tools;%PATH%
if [%checked_folder_for_clone%] equ [] (
  set checked_folder_for_clone=true
  goto :check_depot_tools
)

:: Delete a depot_tools' folder because it's corrupted
call rd /s /q %CD%\depot_tools >nul 2>&1

:: Before to get depot_tools we need to check a command 'git'
call %~dp0check-command.bat git
if %errorlevel% neq 0 (
  echo Git hasn't been found
  echo NOTE: For building you need to install Git (https://git-scm.com/^) or
  echo       if you've done it you need to add a Git's path into a system ^
enviroment variable 'PATH'
  echo ------- Getting of depot_tools failed ^(%DATE% %TIME:~0,8%^) -------
  exit 1
)

:: Clone depot_tools
echo Clone depot_tools files...

call git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
if %errorlevel% neq 0 (
  echo ------- Cloning of depot_tools failed ^(%DATE% %TIME:~0,8%^) -------
  exit %errorlevel%
)

:: Update depot_tools to the latest version and get Windows dependencies
call %CD%\depot_tools\update_depot_tools.bat
if %errorlevel% neq 0 (
  echo ------- Update of depot_tools failed ^(%DATE% %TIME:~0,8%^) -------
  exit %errorlevel%
)

echo ------- Cloning of depot_tools is successful ^(%DATE% %TIME:~0,8%^) -------

:: Add depot_tools into 'PATH'
set PATH=%CD%\depot_tools;%PATH%

exit /b 0
