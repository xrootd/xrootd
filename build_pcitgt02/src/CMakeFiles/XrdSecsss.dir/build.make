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
include src/CMakeFiles/XrdSecsss.dir/depend.make

# Include the progress variables for this target.
include src/CMakeFiles/XrdSecsss.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/XrdSecsss.dir/flags.make

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o: src/CMakeFiles/XrdSecsss.dir/flags.make
src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o: ../src/XrdSecsss/XrdSecProtocolsss.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdSecsss/XrdSecProtocolsss.cc

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdSecsss/XrdSecProtocolsss.cc > CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.i

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdSecsss/XrdSecProtocolsss.cc -o CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.s

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o.requires:
.PHONY : src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o.requires

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o.provides: src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o.requires
	$(MAKE) -f src/CMakeFiles/XrdSecsss.dir/build.make src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o.provides.build
.PHONY : src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o.provides

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o.provides.build: src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o: src/CMakeFiles/XrdSecsss.dir/flags.make
src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o: ../src/XrdSecsss/XrdSecsssID.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdSecsss/XrdSecsssID.cc

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdSecsss/XrdSecsssID.cc > CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.i

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdSecsss/XrdSecsssID.cc -o CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.s

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o.requires:
.PHONY : src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o.requires

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o.provides: src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o.requires
	$(MAKE) -f src/CMakeFiles/XrdSecsss.dir/build.make src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o.provides.build
.PHONY : src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o.provides

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o.provides.build: src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o: src/CMakeFiles/XrdSecsss.dir/flags.make
src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o: ../src/XrdSecsss/XrdSecsssKT.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o -c /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdSecsss/XrdSecsssKT.cc

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.i"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdSecsss/XrdSecsssKT.cc > CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.i

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.s"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src/XrdSecsss/XrdSecsssKT.cc -o CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.s

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o.requires:
.PHONY : src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o.requires

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o.provides: src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o.requires
	$(MAKE) -f src/CMakeFiles/XrdSecsss.dir/build.make src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o.provides.build
.PHONY : src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o.provides

src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o.provides.build: src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o

# Object files for target XrdSecsss
XrdSecsss_OBJECTS = \
"CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o" \
"CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o" \
"CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o"

# External object files for target XrdSecsss
XrdSecsss_EXTERNAL_OBJECTS =

src/libXrdSecsss.so.2.0.0: src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o
src/libXrdSecsss.so.2.0.0: src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o
src/libXrdSecsss.so.2.0.0: src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o
src/libXrdSecsss.so.2.0.0: src/CMakeFiles/XrdSecsss.dir/build.make
src/libXrdSecsss.so.2.0.0: src/libXrdCryptoLite.so.1.0.0
src/libXrdSecsss.so.2.0.0: src/libXrdUtils.so.2.0.0
src/libXrdSecsss.so.2.0.0: src/CMakeFiles/XrdSecsss.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX shared library libXrdSecsss.so"
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/XrdSecsss.dir/link.txt --verbose=$(VERBOSE)
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && $(CMAKE_COMMAND) -E cmake_symlink_library libXrdSecsss.so.2.0.0 libXrdSecsss.so.2 libXrdSecsss.so

src/libXrdSecsss.so.2: src/libXrdSecsss.so.2.0.0

src/libXrdSecsss.so: src/libXrdSecsss.so.2.0.0

# Rule to build all files generated by this target.
src/CMakeFiles/XrdSecsss.dir/build: src/libXrdSecsss.so
.PHONY : src/CMakeFiles/XrdSecsss.dir/build

src/CMakeFiles/XrdSecsss.dir/requires: src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecProtocolsss.cc.o.requires
src/CMakeFiles/XrdSecsss.dir/requires: src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssID.cc.o.requires
src/CMakeFiles/XrdSecsss.dir/requires: src/CMakeFiles/XrdSecsss.dir/XrdSecsss/XrdSecsssKT.cc.o.requires
.PHONY : src/CMakeFiles/XrdSecsss.dir/requires

src/CMakeFiles/XrdSecsss.dir/clean:
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src && $(CMAKE_COMMAND) -P CMakeFiles/XrdSecsss.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/XrdSecsss.dir/clean

src/CMakeFiles/XrdSecsss.dir/depend:
	cd /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02 && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3 /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/src /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02 /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src /afs/cern.ch/user/f/furano/Park/xrootd/xrootd-fbx3/xrootd-fbx3/build_pcitgt02/src/CMakeFiles/XrdSecsss.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/CMakeFiles/XrdSecsss.dir/depend

