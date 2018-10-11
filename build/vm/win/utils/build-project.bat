:: Copyright 2018 the MetaHash project authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: Command line parameters:
:: %1 is a folder for building project (e.g. out\win\release_x64) (must be)
:: %2 is a project name. If it's omitted then all projects'll be built
:: e.g. build-project.bat out\win\release_x86_sharedlibs v8_vm

@echo off

:: Check and get depots_tools
call %~dp0check-and-get-depot-tools.bat

:: Set DEPOT_TOOLS_WIN_TOOLCHAIN to 0
:: This tells depot_tools to use your locally installed version of Visual Studio
set DEPOT_TOOLS_WIN_TOOLCHAIN=0

:: If second parameter is absent we'll build all projects
set project_name=%2
if [%project_name%] equ [] set project_name=All

:: Go into a root folder of source codes
cd %~dp0..\..\..\..

:: Build a project
echo Build project '%project_name%' (%DATE% %TIME:~0,8%)...
ninja -C %1 %2
if %errorlevel% neq 0 (
  echo ------- Building of '%project_name%' failed ^(%DATE% %TIME:~0,8%^) -------
  exit %errorlevel%
)

echo ------- Building of '%project_name%' is successful ^(%DATE% %TIME:~0,8%^) -------

exit /b 0
