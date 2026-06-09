cmake_minimum_required(VERSION 3.18...4.1 FATAL_ERROR)

project(PyXRootD LANGUAGES CXX)

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR OR PYPI_BUILD)
  add_subdirectory(src)
else()
  configure_file(setup.py setup.py)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/VERSION "${XRootD_VERSION_STRING}")

  if(BUILD_TESTS AND ENABLE_SERVER_TESTS)
    execute_process(COMMAND ${Python_EXECUTABLE} -m pytest --version
      RESULT_VARIABLE PYTEST_STATUS OUTPUT_QUIET ERROR_QUIET)

    if(PYTEST_STATUS EQUAL 0)
      set(PYXROOTD_TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test-install)

      add_subdirectory(src)
      set_target_properties(client PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${PYXROOTD_TEST_DIR}/pyxrootd)

      file(MAKE_DIRECTORY ${PYXROOTD_TEST_DIR}/pyxrootd)
      file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/libs
        ${PYXROOTD_TEST_DIR}/XRootD SYMBOLIC)
      configure_file(src/__init__.py
        ${PYXROOTD_TEST_DIR}/pyxrootd/__init__.py COPYONLY)

      set(PYXROOTD_TEST_ENV
        PYTHONPATH=${PYXROOTD_TEST_DIR}
        PYTHONDONTWRITEBYTECODE=1
        PYXROOTD_SERVER_URL=root://localhost:3094/)

      if(APPLE)
        list(APPEND PYXROOTD_TEST_ENV
          DYLD_LIBRARY_PATH=${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
      else()
        list(APPEND PYXROOTD_TEST_ENV
          LD_LIBRARY_PATH=${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
      endif()

      add_test(NAME PyXRootD::pytest
        COMMAND ${Python_EXECUTABLE} -m pytest -v
          ${CMAKE_CURRENT_SOURCE_DIR}/tests
          -k "not test_glob_remote and not test_multiple_glob_with_url_params and not test_folder_glob_with_url_params")

      set_tests_properties(PyXRootD::pytest
        PROPERTIES
          FIXTURES_REQUIRED XRootD::noauth
          ENVIRONMENT "${PYXROOTD_TEST_ENV}"
          RUN_SERIAL TRUE)
    else()
      message(STATUS "pytest not found; Python binding tests disabled")
    endif()
  endif()

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
