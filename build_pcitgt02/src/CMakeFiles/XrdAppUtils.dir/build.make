# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The program to use to edit the cache.
CMAKE_EDIT_COMMAND = /usr/bin/ccmake

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02

# Include any dependencies generated for this target.
include src/CMakeFiles/XrdAppUtils.dir/depend.make

# Include the progress variables for this target.
include src/CMakeFiles/XrdAppUtils.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/XrdAppUtils.dir/flags.make

src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o: src/CMakeFiles/XrdAppUtils.dir/flags.make
src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o: ../src/XrdApps/XrdCpConfig.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdApps/XrdCpConfig.cc

src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdApps/XrdCpConfig.cc > CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.i

src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdApps/XrdCpConfig.cc -o CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.s

src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o.requires:
.PHONY : src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o.requires

src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o.provides: src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o.requires
	$(MAKE) -f src/CMakeFiles/XrdAppUtils.dir/build.make src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o.provides.build
.PHONY : src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o.provides

src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o.provides.build: src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o

src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o: src/CMakeFiles/XrdAppUtils.dir/flags.make
src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o: ../src/XrdApps/XrdCpFile.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdApps/XrdCpFile.cc

src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdApps/XrdCpFile.cc > CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.i

src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdApps/XrdCpFile.cc -o CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.s

src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o.requires:
.PHONY : src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o.requires

src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o.provides: src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o.requires
	$(MAKE) -f src/CMakeFiles/XrdAppUtils.dir/build.make src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o.provides.build
.PHONY : src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o.provides

src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o.provides.build: src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o

# Object files for target XrdAppUtils
XrdAppUtils_OBJECTS = \
"CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o" \
"CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o"

# External object files for target XrdAppUtils
XrdAppUtils_EXTERNAL_OBJECTS =

src/libXrdAppUtils.so.1.0.0: src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o
src/libXrdAppUtils.so.1.0.0: src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o
src/libXrdAppUtils.so.1.0.0: src/CMakeFiles/XrdAppUtils.dir/build.make
src/libXrdAppUtils.so.1.0.0: src/libXrdUtils.so.2.0.0
src/libXrdAppUtils.so.1.0.0: src/CMakeFiles/XrdAppUtils.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX shared library libXrdAppUtils.so"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/XrdAppUtils.dir/link.txt --verbose=$(VERBOSE)
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && $(CMAKE_COMMAND) -E cmake_symlink_library libXrdAppUtils.so.1.0.0 libXrdAppUtils.so.1 libXrdAppUtils.so

src/libXrdAppUtils.so.1: src/libXrdAppUtils.so.1.0.0

src/libXrdAppUtils.so: src/libXrdAppUtils.so.1.0.0

# Rule to build all files generated by this target.
src/CMakeFiles/XrdAppUtils.dir/build: src/libXrdAppUtils.so
.PHONY : src/CMakeFiles/XrdAppUtils.dir/build

src/CMakeFiles/XrdAppUtils.dir/requires: src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpConfig.cc.o.requires
src/CMakeFiles/XrdAppUtils.dir/requires: src/CMakeFiles/XrdAppUtils.dir/XrdApps/XrdCpFile.cc.o.requires
.PHONY : src/CMakeFiles/XrdAppUtils.dir/requires

src/CMakeFiles/XrdAppUtils.dir/clean:
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && $(CMAKE_COMMAND) -P CMakeFiles/XrdAppUtils.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/XrdAppUtils.dir/clean

src/CMakeFiles/XrdAppUtils.dir/depend:
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02 && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3 /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02 /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src/CMakeFiles/XrdAppUtils.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/CMakeFiles/XrdAppUtils.dir/depend

