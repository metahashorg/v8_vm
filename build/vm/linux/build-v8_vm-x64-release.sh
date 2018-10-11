#!/bin/bash
#
# Copyright 2018 the MetaHash project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

$(dirname "$0")/utils/generate-and-build.sh v8_vm x64 release $@
