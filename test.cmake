cmake_minimum_required(VERSION 3.16)

set(ENV{LANG} "C")
set(ENV{LC_ALL} "C")

macro(section title)
  if (DEFINED ENV{CI})
    message("::group::${title}")
  endif()
endmacro()

macro(endsection)
  if (DEFINED ENV{CI})
    message("::endgroup::")
  endif()
endmacro()

site_name(CTEST_SITE)

if(EXISTS "/etc/os-release")
  file(STRINGS "/etc/os-release" OS_NAME REGEX "^ID=.*$")
  string(REGEX REPLACE "ID=[\"']?([^\"']*)[\"']?$" "\\1" OS_NAME "${OS_NAME}")
  file(STRINGS "/etc/os-release" OS_VERSION REGEX "^VERSION_ID=.*$")
  string(REGEX REPLACE "VERSION_ID=[\"']?([^\"'.]*).*$" "\\1" OS_VERSION "${OS_VERSION}")
  file(STRINGS "/etc/os-release" OS_FULL_NAME REGEX "^PRETTY_NAME=.*$")
  string(REGEX REPLACE "PRETTY_NAME=[\"']?([^\"']*)[\"']?$" "\\1" OS_FULL_NAME "${OS_FULL_NAME}")
elseif(APPLE)
  set(OS_NAME "macOS")
  execute_process(COMMAND sw_vers -productVersion
    OUTPUT_VARIABLE OS_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(OS_FULL_NAME "${OS_NAME} ${OS_VERSION}")
else()
  cmake_host_system_information(RESULT OS_NAME QUERY OS_NAME)
  cmake_host_system_information(RESULT OS_VERSION QUERY OS_VERSION)
  set(OS_FULL_NAME "${OS_NAME} ${OS_VERSION}")
endif()

string(APPEND CTEST_SITE " (${OS_FULL_NAME} - ${CMAKE_SYSTEM_PROCESSOR})")

cmake_host_system_information(RESULT
  NCORES QUERY NUMBER_OF_PHYSICAL_CORES)
cmake_host_system_information(RESULT
  NTHREADS QUERY NUMBER_OF_LOGICAL_CORES)

if(NOT DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
  set(ENV{CMAKE_BUILD_PARALLEL_LEVEL} ${NTHREADS})
endif()

if(NOT DEFINED ENV{CTEST_PARALLEL_LEVEL})
  set(ENV{CTEST_PARALLEL_LEVEL} ${NCORES})
endif()

if(NOT DEFINED CTEST_CONFIGURATION_TYPE)
  if(DEFINED ENV{CMAKE_BUILD_TYPE})
    set(CTEST_CONFIGURATION_TYPE $ENV{CMAKE_BUILD_TYPE})
  else()
    set(CTEST_CONFIGURATION_TYPE RelWithDebInfo)
  endif()
endif()

set(CTEST_BUILD_NAME "${CMAKE_SYSTEM_NAME}")

execute_process(COMMAND ${CMAKE_COMMAND} --system-information
  OUTPUT_VARIABLE CMAKE_SYSTEM_INFORMATION ERROR_VARIABLE ERROR)

if(ERROR)
  message(FATAL_ERROR "Cannot detect system information")
endif()

string(REGEX REPLACE ".+CMAKE_CXX_COMPILER_ID \"([-0-9A-Za-z ]+)\".*$" "\\1"
  COMPILER_ID "${CMAKE_SYSTEM_INFORMATION}")
string(REPLACE "GNU" "GCC" COMPILER_ID "${COMPILER_ID}")

string(REGEX REPLACE ".+CMAKE_CXX_COMPILER_VERSION \"([^\"]+)\".*$" "\\1"
  COMPILER_VERSION "${CMAKE_SYSTEM_INFORMATION}")

string(APPEND CTEST_BUILD_NAME " ${COMPILER_ID} ${COMPILER_VERSION}")
string(APPEND CTEST_BUILD_NAME " ${CTEST_CONFIGURATION_TYPE}")

if(DEFINED ENV{CMAKE_GENERATOR})
  set(CTEST_CMAKE_GENERATOR $ENV{CMAKE_GENERATOR})
else()
  string(REGEX REPLACE ".+CMAKE_GENERATOR \"([-0-9A-Za-z ]+)\".*$" "\\1"
    CTEST_CMAKE_GENERATOR "${CMAKE_SYSTEM_INFORMATION}")
endif()

if(NOT CTEST_CMAKE_GENERATOR MATCHES "Makefile")
  string(APPEND CTEST_BUILD_NAME " ${CTEST_CMAKE_GENERATOR}")
endif()

if(NOT DEFINED CTEST_SOURCE_DIRECTORY)
  set(CTEST_SOURCE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
endif()

if(NOT DEFINED CTEST_BINARY_DIRECTORY)
  get_filename_component(CTEST_BINARY_DIRECTORY "$ENV{PWD}/build" REALPATH)
endif()

find_program(CTEST_GIT_COMMAND NAMES git)

if(EXISTS ${CTEST_GIT_COMMAND})
  if(DEFINED ENV{GIT_PREVIOUS_COMMIT} AND DEFINED ENV{GIT_COMMIT})
    set(CTEST_CHECKOUT_COMMAND
      "${CTEST_GIT_COMMAND} -C ${CTEST_SOURCE_DIRECTORY} checkout -f $ENV{GIT_PREVIOUS_COMMIT}")
    set(CTEST_GIT_UPDATE_CUSTOM
      ${CTEST_GIT_COMMAND} -C ${CTEST_SOURCE_DIRECTORY} checkout -f $ENV{GIT_COMMIT})
  elseif(DEFINED ENV{GIT_COMMIT})
    set(CTEST_CHECKOUT_COMMAND
      "${CTEST_GIT_COMMAND} -C ${CTEST_SOURCE_DIRECTORY} checkout -f $ENV{GIT_COMMIT}")
    set(CTEST_GIT_UPDATE_CUSTOM
      ${CTEST_GIT_COMMAND} -C ${CTEST_SOURCE_DIRECTORY} checkout -f $ENV{GIT_COMMIT})
  else()
    set(CTEST_GIT_UPDATE_CUSTOM ${CTEST_GIT_COMMAND} -C ${CTEST_SOURCE_DIRECTORY} diff HEAD)
  endif()
endif()

set(CMAKE_ARGS $ENV{CMAKE_ARGS} ${CMAKE_ARGS})

if(COVERAGE)
  find_program(CTEST_COVERAGE_COMMAND NAMES gcov)
  list(PREPEND CMAKE_ARGS "-DCMAKE_CXX_FLAGS=--coverage -fprofile-update=atomic")
endif()

if(MEMCHECK)
  find_program(CTEST_MEMORYCHECK_COMMAND NAMES valgrind)
endif()

if(STATIC_ANALYSIS)
  find_program(CMAKE_CXX_CLANG_TIDY NAMES clang-tidy)
  list(PREPEND CMAKE_ARGS "-DCMAKE_CXX_CLANG_TIDY=${CMAKE_CXX_CLANG_TIDY}")
endif()

foreach(FILENAME ${OS_NAME}${OS_VERSION}.cmake ${OS_NAME}.cmake config.cmake)
  if(EXISTS "${CTEST_SOURCE_DIRECTORY}/.ci/${FILENAME}")
    message(STATUS "Using CMake cache file ${FILENAME}")
    list(PREPEND CMAKE_ARGS -C ${CTEST_SOURCE_DIRECTORY}/.ci/${FILENAME})
    list(APPEND CTEST_NOTES_FILES ${CTEST_SOURCE_DIRECTORY}/.ci/${FILENAME})
    break()
  endif()
endforeach()

if(NOT DEFINED MODEL)
  if(DEFINED CTEST_SCRIPT_ARG)
    set(MODEL ${CTEST_SCRIPT_ARG})
  else()
    set(MODEL Experimental)
  endif()
endif()

if(IS_DIRECTORY "${CTEST_BINARY_DIRECTORY}")
  ctest_empty_binary_directory("${CTEST_BINARY_DIRECTORY}")
endif()

ctest_read_custom_files("${CTEST_SOURCE_DIRECTORY}")

ctest_start(${MODEL})
ctest_update()

section("Configure")
ctest_configure(OPTIONS "${CMAKE_ARGS}")

ctest_read_custom_files("${CTEST_BINARY_DIRECTORY}")
list(APPEND CTEST_NOTES_FILES ${CTEST_BINARY_DIRECTORY}/CMakeCache.txt)
endsection()

section("Build")
ctest_build(RETURN_VALUE BUILD_RESULT)

if(NOT ${BUILD_RESULT} EQUAL 0)
  message(FATAL_ERROR "Build failed")
endif()

if(INSTALL)
  set(ENV{DESTDIR} "${CTEST_BINARY_DIRECTORY}/install")
  ctest_build(TARGET install)
endif()
endsection()

section("Test")
ctest_test(PARALLEL_LEVEL $ENV{CTEST_PARALLEL_LEVEL} RETURN_VALUE TEST_RESULT)

if(NOT ${TEST_RESULT} EQUAL 0)
  message(FATAL_ERROR "Tests failed")
endif()
endsection()

if(DEFINED CTEST_COVERAGE_COMMAND)
  section("Coverage")
  find_program(GCOVR NAMES gcovr)
  if(EXISTS ${GCOVR})
    execute_process(COMMAND
      ${GCOVR} -r ${CTEST_SOURCE_DIRECTORY}/src ${CTEST_BINARY_DIRECTORY}
        --html-details ${CTEST_BINARY_DIRECTORY}/html/ ERROR_VARIABLE ERROR)
    if(ERROR)
      message(FATAL_ERROR "Failed to generate coverage report")
    endif()
  endif()
  ctest_coverage()
  endsection()
endif()

if(DEFINED CTEST_MEMORYCHECK_COMMAND)
  section("Memcheck")
  ctest_memcheck()
  endsection()
endif()
