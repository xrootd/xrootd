# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canoncical targets will work.
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
CMAKE_COMMAND = /afs/cern.ch/alice/offline/.@sys/local/bin/cmake

# The command to remove a file.
RM = /afs/cern.ch/alice/offline/.@sys/local/bin/cmake -E remove -f

# The program to use to edit the cache.
CMAKE_EDIT_COMMAND = /afs/cern.ch/alice/offline/.@sys/local/bin/ccmake

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6

# Include any dependencies generated for this target.
include src/CMakeFiles/xrd.dir/depend.make

# Include the progress variables for this target.
include src/CMakeFiles/xrd.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/xrd.dir/flags.make

src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o: src/CMakeFiles/xrd.dir/flags.make
src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o: ../src/XrdClient/XrdCommandLine.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdClient/XrdCommandLine.cc

src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdClient/XrdCommandLine.cc > CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.i

src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdClient/XrdCommandLine.cc -o CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.s

src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o.requires:
.PHONY : src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o.requires

src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o.provides: src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o.requires
	$(MAKE) -f src/CMakeFiles/xrd.dir/build.make src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o.provides.build
.PHONY : src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o.provides

src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o.provides.build: src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o

# Object files for target xrd
xrd_OBJECTS = \
"CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o"

# External object files for target xrd
xrd_EXTERNAL_OBJECTS =

src/xrd: src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o
src/xrd: src/libXrdClient.so.2.0.0
src/xrd: src/libXrdUtils.so.2.0.0
src/xrd: /usr/lib64/libreadline.so
src/xrd: /usr/lib64/libncurses.so
src/xrd: /usr/lib64/libncurses.so
src/xrd: src/CMakeFiles/xrd.dir/build.make
src/xrd: src/CMakeFiles/xrd.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable xrd"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/xrd.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/CMakeFiles/xrd.dir/build: src/xrd
.PHONY : src/CMakeFiles/xrd.dir/build

src/CMakeFiles/xrd.dir/requires: src/CMakeFiles/xrd.dir/XrdClient/XrdCommandLine.cc.o.requires
.PHONY : src/CMakeFiles/xrd.dir/requires

src/CMakeFiles/xrd.dir/clean:
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && $(CMAKE_COMMAND) -P CMakeFiles/xrd.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/xrd.dir/clean

src/CMakeFiles/xrd.dir/depend:
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6 && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3 /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6 /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src/CMakeFiles/xrd.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/CMakeFiles/xrd.dir/depend

