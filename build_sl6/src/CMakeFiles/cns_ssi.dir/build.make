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
include src/CMakeFiles/cns_ssi.dir/depend.make

# Include the progress variables for this target.
include src/CMakeFiles/cns_ssi.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/cns_ssi.dir/flags.make

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o: src/CMakeFiles/cns_ssi.dir/flags.make
src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o: ../src/XrdCns/XrdCnsSsi.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCns/XrdCnsSsi.cc

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCns/XrdCnsSsi.cc > CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.i

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCns/XrdCnsSsi.cc -o CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.s

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o.requires:
.PHONY : src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o.requires

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o.provides: src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o.requires
	$(MAKE) -f src/CMakeFiles/cns_ssi.dir/build.make src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o.provides.build
.PHONY : src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o.provides

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o.provides.build: src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o: src/CMakeFiles/cns_ssi.dir/flags.make
src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o: ../src/XrdCns/XrdCnsSsiCfg.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCns/XrdCnsSsiCfg.cc

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCns/XrdCnsSsiCfg.cc > CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.i

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCns/XrdCnsSsiCfg.cc -o CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.s

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o.requires:
.PHONY : src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o.requires

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o.provides: src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o.requires
	$(MAKE) -f src/CMakeFiles/cns_ssi.dir/build.make src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o.provides.build
.PHONY : src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o.provides

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o.provides.build: src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o: src/CMakeFiles/cns_ssi.dir/flags.make
src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o: ../src/XrdCns/XrdCnsSsiMain.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCns/XrdCnsSsiMain.cc

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCns/XrdCnsSsiMain.cc > CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.i

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdCns/XrdCnsSsiMain.cc -o CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.s

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o.requires:
.PHONY : src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o.requires

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o.provides: src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o.requires
	$(MAKE) -f src/CMakeFiles/cns_ssi.dir/build.make src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o.provides.build
.PHONY : src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o.provides

src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o.provides.build: src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o

# Object files for target cns_ssi
cns_ssi_OBJECTS = \
"CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o" \
"CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o" \
"CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o"

# External object files for target cns_ssi
cns_ssi_EXTERNAL_OBJECTS =

src/cns_ssi: src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o
src/cns_ssi: src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o
src/cns_ssi: src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o
src/cns_ssi: src/libXrdUtils.so.2.0.0
src/cns_ssi: src/libXrdCnsLib.a
src/cns_ssi: src/CMakeFiles/cns_ssi.dir/build.make
src/cns_ssi: src/CMakeFiles/cns_ssi.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable cns_ssi"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/cns_ssi.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/CMakeFiles/cns_ssi.dir/build: src/cns_ssi
.PHONY : src/CMakeFiles/cns_ssi.dir/build

src/CMakeFiles/cns_ssi.dir/requires: src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsi.cc.o.requires
src/CMakeFiles/cns_ssi.dir/requires: src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiCfg.cc.o.requires
src/CMakeFiles/cns_ssi.dir/requires: src/CMakeFiles/cns_ssi.dir/XrdCns/XrdCnsSsiMain.cc.o.requires
.PHONY : src/CMakeFiles/cns_ssi.dir/requires

src/CMakeFiles/cns_ssi.dir/clean:
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src && $(CMAKE_COMMAND) -P CMakeFiles/cns_ssi.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/cns_ssi.dir/clean

src/CMakeFiles/cns_ssi.dir/depend:
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6 && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3 /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6 /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_sl6/src/CMakeFiles/cns_ssi.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/CMakeFiles/cns_ssi.dir/depend

