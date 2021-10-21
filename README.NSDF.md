# Windows instructions

Install WSL2 and Visual Studio 2022 beta.

Then follow instructions to enable CMake for the Linux subsystem [here](https://devblogs.microsoft.com/cppblog/build-and-debug-c-with-wsl-2-distributions-and-visual-studio-2022/)

Open a Linux terminal and install prerequisites:

```
sudo apt-get update -y
sudo apt-get install -y \
   g++ gdb make ninja-build rsync zip pkg-config libreadline-dev \
   libkrb5-dev libfuse-dev libcurl3-dev libxml2-dev libtinyxml-dev  \
   libsystemd-dev libcppunit-dev  libjson-c-dev
```

From `Project menu` select `Configure xrootd`:

```
1> [CMake] -- Could NOT find Macaroons (missing: MACAROONS_INCLUDES MACAROONS_LIB) 
1> [CMake] -- Checking for module 'json-c'
1> [CMake] --   No package 'json-c' found
1> [CMake] -- Could NOT find SciTokensCpp (missing: SCITOKENS_CPP_LIBRARIES SCITOKENS_CPP_INCLUDE_DIR) 
1> [CMake] -- Could NOT find PythonLibs (missing: PYTHON_LIBRARIES PYTHON_INCLUDE_DIRS) (Required is at least version "2.4")
1> [CMake] -- Could NOT find VOMS (missing: VOMS_LIBRARY VOMS_INCLUDE_DIR) 
...
1> [CMake] -- Readline support:  yes
1> [CMake] -- Fuse support:      yes
1> [CMake] -- Crypto support:    yes
1> [CMake] -- Kerberos5 support: yes
1> [CMake] -- XrdCl:             yes
1> [CMake] -- Tests:             yes
1> [CMake] -- HTTP support:      yes
1> [CMake] -- HTTP TPC support:  yes
1> [CMake] -- Macaroons support: disabled
1> [CMake] -- VOMS support:      disabled
1> [CMake] -- Python support:    disabled
```

Then switch the "Solution Explored View" to "CMake Target" View and `Build All`
