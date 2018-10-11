:: Copyright 2018 the MetaHash project authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: Command line parameters:
:: The only one parameter is a command for checking (must be)
:: e.g. check-command.bat git

@echo off

call where %1 >nul 2>&1
if %errorlevel% neq 0 exit /b 1

exit /b 0
