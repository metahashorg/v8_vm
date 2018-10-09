:: Copyright 2018 the AdSniper project authors. All rights reserved.
::
::

:: Command line parameters:
:: First parameter is project name for building or may be 'all' (must be)
:: debug/release - building type (default: release)
:: shared - build project with shared libraries (default: static)
:: x86/x64 - target CPU (default: x64)
:: wg - the only to build projec, don't generate (default: false)
:: wb - the only to generate project files, don't build (default: false)
:: rt - after all to run tests
:: e.g. generate-and-build.bat v8 shared x86 rt

@echo off

:: We need at least one parameter and it is a project name
:: 'All' means to build all projects
set project_name=%1
set visible_project_name=%1
if /i %1 equ all (
  set project_name=
  set visible_project_name=All
)

if [%visible_project_name%] equ [] (
  echo Component for build isn't provided!!!
  exit 1
) else (
  echo Building '%visible_project_name%' ^(%DATE% %TIME:~0,8%^):
)

:: Set default parameters of build
set build_flag=true
set build_type=release
set debug_flag=false
set generation_flag=true
set shared_library=false
set target_cpu=x64
set run_tests=false

:: Parse command line
for %%i in (%*) do (
  if /i %%i equ debug (
    set debug_flag=true
    set build_type=debug
  )

  if /i %%i equ shared (
    set shared_library=true
  )

  if /i %%i equ x86 (
    set target_cpu=x86
  )

  if /i %%i equ wg (
    set generation_flag=false
  )

  if /i %%i equ wb (
    set build_flag=false
  )

  if /i %%i equ rt (
    set run_tests=true
  )
)

:: Create a build folder name
set build_folder=out\win\%build_type%_%target_cpu%
if %shared_library% equ true set build_folder=%build_folder%_sharedlibs

:: Generate project files
if %generation_flag% equ true (
  call %~dp0generate-projects.bat ^
"is_debug=%debug_flag% is_component_build=%shared_library% target_cpu=\"%target_cpu%\"" %build_folder%
)

:: Build project
if %build_flag% equ true (
  call %~dp0build-project.bat %build_folder% %project_name%
)

:: Run tests
if %run_tests% equ true (
  call %~dp0run-tests.bat %build_folder%
)

if %generation_flag% equ false (
  if %build_flag% equ false (
    if %run_tests% equ false (
      echo ------- Nothing is done ^(%DATE% %TIME:~0,8%^) -------
    )
  )
)
