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
include src/CMakeFiles/XrdPosixPreload.dir/depend.make

# Include the progress variables for this target.
include src/CMakeFiles/XrdPosixPreload.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/XrdPosixPreload.dir/flags.make

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o: src/CMakeFiles/XrdPosixPreload.dir/flags.make
src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o: ../src/XrdPosix/XrdPosixPreload32.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdPosix/XrdPosixPreload32.cc

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdPosix/XrdPosixPreload32.cc > CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.i

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdPosix/XrdPosixPreload32.cc -o CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.s

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o.requires:
.PHONY : src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o.requires

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o.provides: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o.requires
	$(MAKE) -f src/CMakeFiles/XrdPosixPreload.dir/build.make src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o.provides.build
.PHONY : src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o.provides

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o.provides.build: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o: src/CMakeFiles/XrdPosixPreload.dir/flags.make
src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o: ../src/XrdPosix/XrdPosixPreload.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdPosix/XrdPosixPreload.cc

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdPosix/XrdPosixPreload.cc > CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.i

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdPosix/XrdPosixPreload.cc -o CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.s

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o.requires:
.PHONY : src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o.requires

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o.provides: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o.requires
	$(MAKE) -f src/CMakeFiles/XrdPosixPreload.dir/build.make src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o.provides.build
.PHONY : src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o.provides

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o.provides.build: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o: src/CMakeFiles/XrdPosixPreload.dir/flags.make
src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o: ../src/XrdPosix/XrdPosix.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdPosix/XrdPosix.cc

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdPosix/XrdPosix.cc > CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.i

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdPosix/XrdPosix.cc -o CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.s

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o.requires:
.PHONY : src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o.requires

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o.provides: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o.requires
	$(MAKE) -f src/CMakeFiles/XrdPosixPreload.dir/build.make src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o.provides.build
.PHONY : src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o.provides

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o.provides.build: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o: src/CMakeFiles/XrdPosixPreload.dir/flags.make
src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o: ../src/XrdPosix/XrdPosixLinkage.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/CMakeFiles $(CMAKE_PROGRESS_4)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdPosix/XrdPosixLinkage.cc

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdPosix/XrdPosixLinkage.cc > CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.i

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdPosix/XrdPosixLinkage.cc -o CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.s

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o.requires:
.PHONY : src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o.requires

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o.provides: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o.requires
	$(MAKE) -f src/CMakeFiles/XrdPosixPreload.dir/build.make src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o.provides.build
.PHONY : src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o.provides

src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o.provides.build: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o

# Object files for target XrdPosixPreload
XrdPosixPreload_OBJECTS = \
"CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o" \
"CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o" \
"CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o" \
"CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o"

# External object files for target XrdPosixPreload
XrdPosixPreload_EXTERNAL_OBJECTS =

src/libXrdPosixPreload.so.1.0.0: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o
src/libXrdPosixPreload.so.1.0.0: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o
src/libXrdPosixPreload.so.1.0.0: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o
src/libXrdPosixPreload.so.1.0.0: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o
src/libXrdPosixPreload.so.1.0.0: src/libXrdPosix.so.2.0.0
src/libXrdPosixPreload.so.1.0.0: src/CMakeFiles/XrdPosixPreload.dir/build.make
src/libXrdPosixPreload.so.1.0.0: src/CMakeFiles/XrdPosixPreload.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX shared library libXrdPosixPreload.so"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/XrdPosixPreload.dir/link.txt --verbose=$(VERBOSE)
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && $(CMAKE_COMMAND) -E cmake_symlink_library libXrdPosixPreload.so.1.0.0 libXrdPosixPreload.so.1 libXrdPosixPreload.so

src/libXrdPosixPreload.so.1: src/libXrdPosixPreload.so.1.0.0

src/libXrdPosixPreload.so: src/libXrdPosixPreload.so.1.0.0

# Rule to build all files generated by this target.
src/CMakeFiles/XrdPosixPreload.dir/build: src/libXrdPosixPreload.so
.PHONY : src/CMakeFiles/XrdPosixPreload.dir/build

src/CMakeFiles/XrdPosixPreload.dir/requires: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload32.cc.o.requires
src/CMakeFiles/XrdPosixPreload.dir/requires: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixPreload.cc.o.requires
src/CMakeFiles/XrdPosixPreload.dir/requires: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosix.cc.o.requires
src/CMakeFiles/XrdPosixPreload.dir/requires: src/CMakeFiles/XrdPosixPreload.dir/XrdPosix/XrdPosixLinkage.cc.o.requires
.PHONY : src/CMakeFiles/XrdPosixPreload.dir/requires

src/CMakeFiles/XrdPosixPreload.dir/clean:
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && $(CMAKE_COMMAND) -P CMakeFiles/XrdPosixPreload.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/XrdPosixPreload.dir/clean

src/CMakeFiles/XrdPosixPreload.dir/depend:
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6 && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3 /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6 /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src/CMakeFiles/XrdPosixPreload.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/CMakeFiles/XrdPosixPreload.dir/depend

