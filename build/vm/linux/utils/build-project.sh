#!/bin/bash
#
#  Copyright 2018 the AdSniper project authors. All rights reserved.
#
#

# Command line parameters:
# $1 is a folder for building project (e.g. out\win\release_x64) (must be)
# $2 is a project name. If it's omitted then all projects'll be built
# e.g. build-project.sh out/linux/release_x86_sharedlibs v8_vm

# Export depot_tools
. $(dirname "$0")/export-depot-tools.sh

# Check and get depots_tools
$(dirname "$0")/check-and-get-depot-tools.sh
errcode=$?
if [ $errcode -ne 0 ]; then
  echo "------- Building of '$project_name' failed ($(date '+%d.%m.%Y %H:%M:%S')) -------"
  exit $errcode
fi

# If second parameter is absent we'll build all projects
project_name=$2
if [ $# -eq 1 ]; then
  project_name=All
fi

# Go into a root folder of source codes
cd $(dirname "$0")/../../../..

# Build a project
echo "Build project '$project_name' ($(date '+%d.%m.%Y %H:%M:%S'))..."
ninja -C $1 $2
errcode=$?
if [ $errcode -ne 0 ]; then
  echo "------- Building of '$project_name' failed ($(date '+%d.%m.%Y %H:%M:%S')) -------"
  exit $errcode
fi

echo "------- Building of '$project_name' is successful ($(date '+%d.%m.%Y %H:%M:%S')) -------"

exit 0
