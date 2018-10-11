Virtual machine is based on V8
=============

Virtual machine is MetaHash's open source project.

Virtual machine is written in C++ and uses V8, open source JavaScript engine from Google.

Virtual machine can run standalone, or can be embedded into any C++ application.


Getting the Code
=============
**Linux:**
1. Install [Git LFS](https://github.com/git-lfs/git-lfs/wiki/Installation)
2. Clone repository - ``git clone https://github.com/metahashorg/v8_vm.git``

**Windows:**
1. Install [Git LFS](https://github.com/git-lfs/git-lfs/wiki/Installation)
2. Install [Windows 10 SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)
3. Install [Visual Studio 2017](https://visualstudio.microsoft.com/downloads/)
4. Launch console as Administrator and run - ``git clone -c core.symlinks=true https://github.com/metahashorg/v8_vm.git``

Building the Code
=============

**Linux:**

        ./build/vm/linux/build-all-x64-release.sh

**Windows:**

        .\build\vm\win\build-all-x64-release.bat

Result will be in ``./out`` directory.
