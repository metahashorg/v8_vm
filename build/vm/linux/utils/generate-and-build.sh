#!/bin/bash
#
#  Copyright 2018 the AdSniper project authors. All rights reserved.
#
#

# Command line parameters:
# First parameter is project name for building or may be 'all' (must be)
# debug/release - building type (default: release)
# shared - build project with shared libraries (default: static)
# x86/x64 - target CPU (default: x64)
# wg - the only to build a projec, don't generate (default: false)
# wb - the only to generate a project files, don't build (default: false)
# sf - use a subfolder for a project (format: sf=<subfolder>; result: out/linux/<subfolder>/...)
# rt - after all to run tests
# e.g. generate-and-build.bat v8 shared x86 rt

# We need at least one parameter and it is a project name
# 'All' means to build all projects
project_name=$1
visible_project_name=$1
if [ "${project_name,,}" == "all" ]; then
  project_name=
  visible_project_name=All
fi

if [ $# -eq 0 ]; then
  echo "Component for build isn't provided!!!"
  exit 1
else
  echo "Building '$visible_project_name' ($(date '+%d.%m.%Y %H:%M:%S')):"
fi

# Set default parameters of build
build_flag=true
build_type=release
debug_flag=false
generation_flag=true
shared_library=false
target_cpu=x64
sub_folder=
run_tests=false

# Parse command line
for i in $@; do
  if [ "${i,,}" == "debug" ]; then
    debug_flag=true
    build_type=debug
  fi

  if [ "${i,,}" == "shared" ]; then
    shared_library=true
  fi

  if [ "${i,,}" == "x86" ]; then
    target_cpu=x86
  fi

  if [ "${i,,}" == "wg" ]; then
    generation_flag=false
  fi

  if [ "${i,,}" == "wb" ]; then
    build_flag=false
  fi

  if [ "${i:0:3}" == "sf=" ]; then
    sub_folder=${i:3}
  fi

  if [ "${i,,}" == "rt" ]; then
    run_tests=true
  fi
done

# Create a build folder name
build_folder=out/linux
if ! [ "$sub_folder" == "" ]; then
  build_folder=${build_folder}/${sub_folder}
fi

build_folder=${build_folder}/${build_type}_${target_cpu}
if [ "$shared_library" == "true" ]; then
  build_folder=${build_folder}_sharedlibs
fi

# Generate project files
if [ "$generation_flag" == "true" ]; then
  $(dirname "$0")/generate-projects.sh \
  "is_debug=${debug_flag} is_component_build=${shared_library} target_cpu=\"${target_cpu}\"" \
  $build_folder $target_cpu
  if [ $? -ne 0 ]; then
    exit $?
  fi
fi

# Build project
if [ "$build_flag" == "true" ]; then
  $(dirname "$0")/build-project.sh $build_folder $project_name
  if [ $? -ne 0 ]; then
    exit $?
  fi
fi

# Run tests
if [ "$run_tests" == "true" ]; then
  $(dirname "$0")/run-tests.sh $build_folder
  if [ $? -ne 0 ]; then
    exit $?
  fi
fi

if [ "$generation_flag" == "false" ]; then
  if [ "$build_flag" == "false" ]; then
    if [ "$run_tests" == "false" ]; then
      echo "------- Nothing is done ($(date '+%d.%m.%Y %H:%M:%S')) -------"
    fi
  fi
fi
