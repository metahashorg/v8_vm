:: Copyright 2018 the AdSniper project authors. All rights reserved.
::
::

@echo off

call %~dp0utils\generate-and-build.bat v8_vm x64 debug shared %*
