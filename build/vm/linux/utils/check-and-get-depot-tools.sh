#!/bin/bash
#
#  Copyright 2018 the AdSniper project authors. All rights reserved.
#
#

# Export depot_tools
. $(dirname "$0")/export-depot-tools.sh

# Check necessary commands. If they exists to do nothing
if [ -x "$(command -v gn)" ]; then
  if [ -x "$(command -v ninja)" ]; then
    exit 0
  fi
fi

# Go into a root folder of project
cd $(dirname "$0")/../../../../..

# Delete a depot_tools' folder because it's corrupted
rm -rf $PWD/depot_tools

# Before to get depot_tools we need to check a command 'git'
if ! [ -x "$(command -v git)" ]; then
  echo "Git hasn't been found"
  echo "NOTE: For building you need to install Git ('sudo apt install git')"
  echo "------- Getting of depot_tools failed ($(date '+%d.%m.%Y %H:%M:%S')) -------"
  exit 1
fi

# Clone depot_tools
echo Clone depot_tools files...

git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
errcode=$?
if [ $errcode -ne 0 ]; then
  echo "------- Getting of depot_tools failed ($(date '+%d.%m.%Y %H:%M:%S')) -------"
  exit $errcode
fi

echo "------- Cloning of depot_tools is successful ($(date '+%d.%m.%Y %H:%M:%S')) ------"

exit 0
