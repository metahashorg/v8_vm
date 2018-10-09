:: Copyright 2018 the AdSniper project authors. All rights reserved.
::
::

@echo off

call %~dp0utils\generate-and-build.bat v8 x64 debug shared %*
