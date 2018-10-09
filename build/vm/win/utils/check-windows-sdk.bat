:: Copyright 2018 the AdSniper project authors. All rights reserved.
::
::

@echo off

:: If user defined a SDK directory then to do nothing
if [%WINDOWSSDKDIR%] neq [] goto :script_end

set WINDOWSSDKDIR=

set windows_sdk_folder=\Program Files (x86)\Windows Kits\10
set temp_windowssdkdir=

:: Check a different letters of disk for SDK
for %%i in (A B C D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
  if exist "%%i:%windows_sdk_folder%" (
    set WINDOWSSDKDIR=%%i:
    goto :for_end
  )
)
:for_end

:: If %WINDOWSSDKDIR% is empty we hasn't found SDK
if []==[%WINDOWSSDKDIR%] (
  echo Windows 10 SDK hasn't been found
  echo NOTE: For building you need to install Windows 10 SDK ^
(https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk^) or
  echo       if you've done it you need to set a SDK's path into a system ^
enviroment variable 'WINDOWSSDKDIR' ^
(e.g. WINDOWSSDKDIR=C:\MyPrograms\Windows Kits\10^)
  echo ------- Checking of Windows 10 SDK failed ^(%DATE% %TIME:~0,8%^) -------
  exit 1
)

:: Set full path for SDK
set WINDOWSSDKDIR=%WINDOWSSDKDIR%%windows_sdk_folder%

:script_end
echo Windows 10 SDK has been found in "%WINDOWSSDKDIR%"

exit /b 0
