#!/bin/bash
#
#  Copyright 2018 the AdSniper project authors. All rights reserved.
#
#

# Remember a current directory
temp_current_directory=$PWD

# Go into a root folder of project
cd $(dirname "$0")/../../../../..

# Export depot_tools
export PATH="$PATH:$PWD/depot_tools"

# Return a current directory
cd $temp_current_directory
