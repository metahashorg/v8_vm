#!/bin/bash
#
#  Copyright 2018 the AdSniper project authors. All rights reserved.
#
#

# Command line parameters:
# The only one parameter is either x86 or x64 (must be)

architecture=amd64
sysroot_directory=$(dirname "$0")/../../../linux
if [ "$1" == "x86" ]; then
  architecture=i386
  sysroot_directory=${sysroot_directory}/debian_sid_i386-sysroot
else
  sysroot_directory=${sysroot_directory}/debian_sid_amd64-sysroot
fi

if ! [ -d $sysroot_directory ]; then
  python $(dirname "$0")/../../../linux/sysroot_scripts/install-sysroot.py \
      --arch=$architecture
  errcode=$?
  if [ $errcode -ne 0 ]; then
    echo "------- Installation of sysroot failed ($(date '+%d.%m.%Y %H:%M:%S')) -------"
    exit $errcode
  fi
fi

exit 0
