#!/bin/bash
#
#  Copyright 2018 the AdSniper project authors. All rights reserved.
#
#

# Command line parameters:
# $1 is options for generating project (must be)
# $2 is a destination folder (must be)
# $3 is CPU architecture (default: x64)
# e.g. generate-projects.sh 'is_debug=false target_cpu="x64"' ./out/linux/release_x64

# Export depot_tools
. $(dirname "$0")/export-depot-tools.sh

# Check and get depots_tools
$(dirname "$0")/check-and-get-depot-tools.sh
errcode=$?
if [ $errcode -ne 0 ]; then
  echo "------- Generation of projects failed ($(date '+%d.%m.%Y %H:%M:%S')) -------"
  exit $errcode
fi

# Install sysroot
target_cpu=x64
if [ $# -eq 3 ]; then
  target_cpu=$3
fi

$(dirname "$0")/install-sysroot.sh $target_cpu
errcode=$?
if [ $errcode -ne 0 ]; then
  echo "------- Generation of projects failed ($(date '+%d.%m.%Y %H:%M:%S')) -------"
  exit $errcode
fi

# Go into a root folder of source codes
cd $(dirname "$0")/../../../..

echo "Generate project files (options: $1; folder: $2; $(date '+%d.%m.%Y %H:%M:%S'))..."

# Generate all projects
gn gen $2 --args="$1"
errcode=$?
if [ $errcode -ne 0 ]; then
  echo "------- Generation of projects failed ($(date '+%d.%m.%Y %H:%M:%S')) -------"
  exit $errcode
fi

echo "------- Generation of projects is successful ($(date '+%d.%m.%Y %H:%M:%S')) -------"

exit 0
