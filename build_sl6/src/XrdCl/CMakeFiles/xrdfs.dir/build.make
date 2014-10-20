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
include src/XrdCl/CMakeFiles/xrdfs.dir/depend.make

# Include the progress variables for this target.
include src/XrdCl/CMakeFiles/xrdfs.dir/progress.make

# Include the compile flags for this target's objects.
include src/XrdCl/CMakeFiles/xrdfs.dir/flags.make

src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.o: src/XrdCl/CMakeFiles/xrdfs.dir/flags.make
src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.o: ../src/XrdCl/XrdClFS.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src/XrdCl && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/xrdfs.dir/XrdClFS.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCl/XrdClFS.cc

src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/xrdfs.dir/XrdClFS.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src/XrdCl && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCl/XrdClFS.cc > CMakeFiles/xrdfs.dir/XrdClFS.cc.i

src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/xrdfs.dir/XrdClFS.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src/XrdCl && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCl/XrdClFS.cc -o CMakeFiles/xrdfs.dir/XrdClFS.cc.s

src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.o.requires:
.PHONY : src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.o.requires

src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.o.provides: src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.o.requires
	$(MAKE) -f src/XrdCl/CMakeFiles/xrdfs.dir/build.make src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.o.provides.build
.PHONY : src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.o.provides

src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.o.provides.build: src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.o

src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o: src/XrdCl/CMakeFiles/xrdfs.dir/flags.make
src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o: ../src/XrdCl/XrdClFSExecutor.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src/XrdCl && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCl/XrdClFSExecutor.cc

src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src/XrdCl && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCl/XrdClFSExecutor.cc > CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.i

src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src/XrdCl && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCl/XrdClFSExecutor.cc -o CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.s

src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o.requires:
.PHONY : src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o.requires

src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o.provides: src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o.requires
	$(MAKE) -f src/XrdCl/CMakeFiles/xrdfs.dir/build.make src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o.provides.build
.PHONY : src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o.provides

src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o.provides.build: src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o

# Object files for target xrdfs
xrdfs_OBJECTS = \
"CMakeFiles/xrdfs.dir/XrdClFS.cc.o" \
"CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o"

# External object files for target xrdfs
xrdfs_EXTERNAL_OBJECTS =

src/XrdCl/xrdfs: src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.o
src/XrdCl/xrdfs: src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o
src/XrdCl/xrdfs: src/XrdCl/libXrdCl.so.2.0.0
src/XrdCl/xrdfs: /usr/lib64/libreadline.so
src/XrdCl/xrdfs: /usr/lib64/libncurses.so
src/XrdCl/xrdfs: /usr/lib64/libncurses.so
src/XrdCl/xrdfs: src/XrdCl/CMakeFiles/xrdfs.dir/build.make
src/XrdCl/xrdfs: src/XrdCl/CMakeFiles/xrdfs.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable xrdfs"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src/XrdCl && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/xrdfs.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/XrdCl/CMakeFiles/xrdfs.dir/build: src/XrdCl/xrdfs
.PHONY : src/XrdCl/CMakeFiles/xrdfs.dir/build

src/XrdCl/CMakeFiles/xrdfs.dir/requires: src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFS.cc.o.requires
src/XrdCl/CMakeFiles/xrdfs.dir/requires: src/XrdCl/CMakeFiles/xrdfs.dir/XrdClFSExecutor.cc.o.requires
.PHONY : src/XrdCl/CMakeFiles/xrdfs.dir/requires

src/XrdCl/CMakeFiles/xrdfs.dir/clean:
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src/XrdCl && $(CMAKE_COMMAND) -P CMakeFiles/xrdfs.dir/cmake_clean.cmake
.PHONY : src/XrdCl/CMakeFiles/xrdfs.dir/clean

src/XrdCl/CMakeFiles/xrdfs.dir/depend:
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6 && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3 /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCl /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6 /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src/XrdCl /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src/XrdCl/CMakeFiles/xrdfs.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/XrdCl/CMakeFiles/xrdfs.dir/depend

