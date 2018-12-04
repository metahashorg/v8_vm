:: Copyright 2018 the MetaHash project authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

@echo off

call %~dp0utils\generate-and-build.bat v8_http_server x64 release %*
