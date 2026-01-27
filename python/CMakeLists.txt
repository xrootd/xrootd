cmake_minimum_required(VERSION 3.18...4.1 FATAL_ERROR)

project(PyXRootD LANGUAGES CXX)

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR OR PYPI_BUILD)
  add_subdirectory(src)
else()
  configure_file(setup.py setup.py)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/VERSION "${XRootD_VERSION_STRING}")

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
