# add_unit_tests([<qualifier>] SOURCES <file>... [LIBRARIES <library>...]
#                [INCLUDE_DIRECTORIES <dir>...] [NO_MAIN]
#                [PROPERTIES <name> <value>...])
#
# Make the executable tests-unit-<component>[-<qualifier>], where <component>
# is the name of the current directory in lower case, and register its tests
# in CTest as unit::<Component>::<Suite>.<Test>. Use a qualifier only for an
# additional executable of a component, and let it give the reason for the
# separate executable, for example process-global state. NO_MAIN is for test
# files that have their own main(). PROPERTIES are set on each test.
function(add_unit_tests)
  cmake_parse_arguments(PARSE_ARGV 0 ARG "NO_MAIN" ""
    "SOURCES;LIBRARIES;INCLUDE_DIRECTORIES;PROPERTIES")

  get_filename_component(component "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
  string(TOLOWER "${component}" name)

  list(LENGTH ARG_UNPARSED_ARGUMENTS count)
  if(count GREATER 1)
    message(FATAL_ERROR "add_unit_tests: more than one qualifier: ${ARG_UNPARSED_ARGUMENTS}")
  elseif(count EQUAL 1)
    set(name "${name}-${ARG_UNPARSED_ARGUMENTS}")
  endif()

  set(target "tests-unit-${name}")

  add_executable(${target} ${ARG_SOURCES})
  target_link_libraries(${target} PRIVATE ${ARG_LIBRARIES} GTest::gmock GTest::gtest)

  if(NOT ARG_NO_MAIN)
    target_link_libraries(${target} PRIVATE GTest::gtest_main)
  endif()

  if(DEFINED ARG_INCLUDE_DIRECTORIES)
    target_include_directories(${target} PRIVATE ${ARG_INCLUDE_DIRECTORIES})
  endif()

  # std::filesystem is in a separate library before GCC 9.1
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
    target_link_libraries(${target} PRIVATE stdc++fs)
  endif()

  # A function sees the variables of its caller, thus clear this one first
  set(properties)
  if(DEFINED ARG_PROPERTIES)
    set(properties PROPERTIES ${ARG_PROPERTIES})
  endif()

  gtest_discover_tests(${target} TEST_PREFIX "unit::${component}::"
    DISCOVERY_TIMEOUT 10 ${properties})
endfunction()

add_subdirectory(example)

add_subdirectory(Xrd)
add_subdirectory(XrdCeph)
add_subdirectory(XrdCks)
add_subdirectory(XrdCl)
add_subdirectory(XrdClHttp)
add_subdirectory(XrdClS3)
add_subdirectory(XrdEc)
add_subdirectory(XrdHttp)
add_subdirectory(XrdHttpCors)
add_subdirectory(XrdHttpTapeApi)
add_subdirectory(XrdHttpTpc)
add_subdirectory(XrdMacaroons)
add_subdirectory(XrdNet)
add_subdirectory(XrdOssMirage)
add_subdirectory(XrdOuc)
add_subdirectory(XrdPfc)
add_subdirectory(XrdSecgsi)
add_subdirectory(XrdSys)
add_subdirectory(XrdThrottle)
add_subdirectory(XrdXrootd)
