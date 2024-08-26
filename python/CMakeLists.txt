cmake_minimum_required(VERSION 3.18...4.1 FATAL_ERROR)

project(PyXRootD LANGUAGES CXX)

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR OR PYPI_BUILD)
  add_subdirectory(src)
else()
  configure_file(setup.py setup.py)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/VERSION "${XRootD_VERSION_STRING}")

  add_custom_target(PyXRootD ALL
    ${Python_EXECUTABLE} -m pip install --upgrade --target . .
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/XRootD DEPENDS XrdCl
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  if(NOT BUILD_TESTS OR NOT ENABLE_SERVER_TESTS)
    return()
  endif()

  # We need to set a library path because we need to build the bindings C++
  # libraries with their final install RPATH. We need to build with install
  # RPATH because the libraries are installed by pip instead of CMake, so
  # CMake cannot fix the RPATH during installation as with other libraries.

  if(NOT APPLE)
    list(APPEND ENVIRON "LD_LIBRARY_PATH=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  else()
    list(APPEND ENVIRON "DYLD_LIBRARY_PATH=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  endif()

  list(APPEND ENVIRON "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}")

  foreach(test copy file filesystem glob threads url)
    add_test(NAME PyXRootD::${test}
      COMMAND ${Python_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${test}.py)
    set_tests_properties(PyXRootD::${test}
      PROPERTIES FIXTURES_REQUIRED XRootD::noauth ENVIRONMENT "${ENVIRON}" RUN_SERIAL TRUE)
  endforeach()

  set_tests_properties(PyXRootD::filesystem PROPERTIES DEPENDS PyXRootD::file)

  option(INSTALL_PYTHON_BINDINGS "Install Python bindings" TRUE)

  if(INSTALL_PYTHON_BINDINGS)
    set(PIP_OPTIONS "" CACHE STRING "Install options for pip")

    install(CODE "
      execute_process(COMMAND ${Python_EXECUTABLE} -m pip install ${PIP_OPTIONS}
        --prefix \$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX} ${CMAKE_CURRENT_BINARY_DIR}
        RESULT_VARIABLE INSTALL_STATUS COMMAND_ECHO STDOUT)
      if(NOT INSTALL_STATUS EQUAL 0)
        message(FATAL_ERROR \"Failed to install Python bindings\")
      endif()
    ")
  endif()
endif()
