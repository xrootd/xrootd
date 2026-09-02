cmake_minimum_required(VERSION 3.18...4.1 FATAL_ERROR)

project(PyXRootD LANGUAGES CXX)

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)

add_subdirectory(src)

# When building standalone or for PyPI, setup.py drives the build and takes
# care of installation, so there is nothing else to do here.

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR OR PYPI_BUILD)
  return()
endif()

# As part of the main build, the extension module above is all that needs to
# be compiled, and setup.py only packages it into a wheel.
#
# Build with the install RPATH, using a path relative to the site-packages
# directory, since the module is installed by pip instead of CMake, and CMake
# cannot fix the RPATH during installation as it does with other libraries.

set_target_properties(client PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)

if(APPLE)
  set_target_properties(client PROPERTIES INSTALL_RPATH "@loader_path/../../..")
else()
  set_target_properties(client PROPERTIES INSTALL_RPATH "\$ORIGIN/../../../../\$LIB")
endif()

configure_file(setup.py setup.py)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/VERSION "${XRootD_VERSION_STRING}")

# On macOS, the name of the wheel depends on the deployment target: the wheel
# builder inspects the compiled module and raises the platform tag to match
# its minimum OS version. When a deployment target is set, pass it on to the
# tag prediction below and to the wheel build, so that the two agree.

if(APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET)
  set(PYXROOTD_ENV ${CMAKE_COMMAND} -E env
    "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

# Determine the name of the wheel in advance, so that it can be used as the
# output of the command which builds it. Ask setup.py for the version in order
# to keep a single implementation of the conversion to a PEP 440 version, and
# ask the wheel builder itself for the platform tag.

execute_process(COMMAND ${Python_EXECUTABLE} setup.py --version
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  OUTPUT_VARIABLE PYXROOTD_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_VARIABLE PYXROOTD_ERROR RESULT_VARIABLE PYXROOTD_STATUS)

if(NOT PYXROOTD_STATUS EQUAL 0)
  message(FATAL_ERROR "Failed to determine version of the Python bindings\n"
                      "${PYXROOTD_ERROR}")
endif()

execute_process(COMMAND ${PYXROOTD_ENV} ${Python_EXECUTABLE} -c "
from setuptools.dist import Distribution

try:
    from setuptools.command.bdist_wheel import bdist_wheel
except ImportError:
    from wheel.bdist_wheel import bdist_wheel

distribution = Distribution({'name': 'xrootd', 'version': '0'})
distribution.has_ext_modules = lambda: True

command = bdist_wheel(distribution)
command.ensure_finalized()

print('-'.join(command.get_tag()))
"
  OUTPUT_VARIABLE PYXROOTD_TAG OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_VARIABLE PYXROOTD_ERROR RESULT_VARIABLE PYXROOTD_STATUS)

if(NOT PYXROOTD_STATUS EQUAL 0)
  message(FATAL_ERROR "Failed to determine tag for the Python bindings wheel. "
                      "Please install setuptools 70.1 or later, or the wheel "
                      "package.\n${PYXROOTD_ERROR}")
endif()

set(PYXROOTD_WHEEL
  ${CMAKE_CURRENT_BINARY_DIR}/dist/xrootd-${PYXROOTD_VERSION}-${PYXROOTD_TAG}.whl)
set(PYXROOTD_SITEDIR ${CMAKE_CURRENT_BINARY_DIR}/site-packages)

message(STATUS "Python bindings wheel: ${PYXROOTD_WHEEL}")

# The C++ dependencies of the wheel are tracked via the module built above.
# The remaining files it packages have to be listed here, though. README.md
# is in the list because setup.py reads it for the package description.

set(PYXROOTD_SOURCES
  README.md
  libs/__init__.py
  libs/client/__init__.py
  libs/client/_version.py
  libs/client/copyprocess.py
  libs/client/env.py
  libs/client/file.py
  libs/client/filesystem.py
  libs/client/finalize.py
  libs/client/flags.py
  libs/client/glob_funcs.py
  libs/client/py.typed
  libs/client/responses.py
  libs/client/url.py
  libs/client/utils.py
  src/__init__.py)

list(TRANSFORM PYXROOTD_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)

# Building the wheel without isolation avoids creating a virtual environment
# and fetching setuptools from PyPI, but old versions of pip, like the one on
# RHEL 8, do not know the option. There, pip predates build isolation
# entirely, so nothing needs to be turned off.

execute_process(COMMAND ${Python_EXECUTABLE} -m pip wheel --help
  OUTPUT_VARIABLE PIP_HELP ERROR_QUIET RESULT_VARIABLE PYXROOTD_STATUS)

if(PYXROOTD_STATUS EQUAL 0 AND PIP_HELP MATCHES "--no-build-isolation")
  set(PYXROOTD_PIP_OPTIONS --no-build-isolation)
else()
  set(PYXROOTD_PIP_OPTIONS)
endif()

# Stage exactly the module built above for setup.py to package, recreating
# the staging directory every time so that a wheel can never pick up stale
# files, such as a module left behind by a build with another Python version.
# Clear the wheel directory as well, so that it holds exactly one wheel,
# which allows it to be referred to with a wildcard when building packages.
# The wheel is then unpacked so that the bindings can be imported from the
# build directory, both by the tests below and for manual testing. Note that
# this last step also verifies that the wheel has the predicted name.

add_custom_command(
  OUTPUT ${PYXROOTD_WHEEL} ${PYXROOTD_SITEDIR}/pyxrootd/__init__.py
  COMMAND ${CMAKE_COMMAND} -E rm -rf
          ${CMAKE_CURRENT_BINARY_DIR}/extension ${CMAKE_CURRENT_BINARY_DIR}/dist
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/extension
  COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE:client> ${CMAKE_CURRENT_BINARY_DIR}/extension
  COMMAND ${PYXROOTD_ENV} ${Python_EXECUTABLE} -m pip wheel --no-deps --no-index
          ${PYXROOTD_PIP_OPTIONS} --disable-pip-version-check --wheel-dir ${CMAKE_CURRENT_BINARY_DIR}/dist .
  COMMAND ${Python_EXECUTABLE} -m pip install --no-deps --no-index --upgrade
          --disable-pip-version-check --target ${PYXROOTD_SITEDIR} ${PYXROOTD_WHEEL}
  DEPENDS client $<TARGET_FILE:client> ${PYXROOTD_SOURCES}
          ${CMAKE_CURRENT_BINARY_DIR}/setup.py
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Building Python bindings wheel")

add_custom_target(PyXRootD ALL DEPENDS ${PYXROOTD_WHEEL})

# None of what pip leaves behind in the build directory is known to CMake.

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES
  ${CMAKE_CURRENT_BINARY_DIR}/build
  ${CMAKE_CURRENT_BINARY_DIR}/dist
  ${CMAKE_CURRENT_BINARY_DIR}/extension
  ${CMAKE_CURRENT_BINARY_DIR}/xrootd.egg-info
  ${PYXROOTD_SITEDIR})

option(INSTALL_PYTHON_BINDINGS "Install Python bindings" TRUE)

if(INSTALL_PYTHON_BINDINGS)
  set(PIP_OPTIONS "" CACHE STRING "Install options for pip")

  install(CODE "
    if(NOT EXISTS \"${PYXROOTD_WHEEL}\")
      message(FATAL_ERROR \"Python bindings wheel not found: ${PYXROOTD_WHEEL}\n\"
                          \"Please build the project before installing it.\")
    endif()

    # Note: without --ignore-installed, pip skips installing a wheel whose
    # version matches an already installed distribution, and would silently
    # install nothing here since the environment cannot be changed anyway.

    execute_process(COMMAND ${Python_EXECUTABLE} -m pip install ${PIP_OPTIONS}
      --no-deps --ignore-installed
      --prefix \$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX} ${PYXROOTD_WHEEL}
      RESULT_VARIABLE INSTALL_STATUS COMMAND_ECHO STDOUT)
    if(NOT INSTALL_STATUS EQUAL 0)
      message(FATAL_ERROR \"Failed to install Python bindings\")
    endif()
  ")
endif()

if(NOT BUILD_TESTS OR NOT ENABLE_SERVER_TESTS)
  return()
endif()

# The tests below are driven by pytest, a prerequisite the other tests do not
# have. Probe for it now, so that a missing pytest is reported by a single
# configuration failure instead of by every test at CTest time.

execute_process(COMMAND ${Python_EXECUTABLE} -m pytest --version
  OUTPUT_VARIABLE PYXROOTD_PYTEST_VERSION ERROR_VARIABLE PYXROOTD_PYTEST_VERSION
  RESULT_VARIABLE PYXROOTD_STATUS)

if(NOT PYXROOTD_STATUS EQUAL 0)
  message(FATAL_ERROR "The tests for the Python bindings require pytest, "
                      "which could not be imported by ${Python_EXECUTABLE}. "
                      "Please install pytest, or disable these tests with "
                      "-DENABLE_PYTHON=OFF or -DENABLE_SERVER_TESTS=OFF.")
endif()

string(REGEX REPLACE "\n.*" "" PYXROOTD_PYTEST_VERSION "${PYXROOTD_PYTEST_VERSION}")
message(STATUS "Found pytest: ${PYXROOTD_PYTEST_VERSION}")

# The module is built with its install RPATH, which does not resolve from the
# build tree, so the client library has to be found via the library path.

if(NOT APPLE)
  list(APPEND ENVIRON "LD_LIBRARY_PATH=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
else()
  list(APPEND ENVIRON "DYLD_LIBRARY_PATH=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()

list(APPEND ENVIRON "PYTHONPATH=${PYXROOTD_SITEDIR}")

# The tests start their own server via conftest.py, so they need to know
# where to find it. Keep the source tree clean of caches while at it.

list(APPEND ENVIRON "XROOTD=$<TARGET_FILE:xrootd>")
list(APPEND ENVIRON "XRDFS=$<TARGET_FILE:xrdfs>")
list(APPEND ENVIRON "PYTHONDONTWRITEBYTECODE=1")

foreach(test copy file filesystem glob threads url)
  add_test(NAME PyXRootD::${test}
    COMMAND ${Python_EXECUTABLE} -m pytest -p no:cacheprovider
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${test}.py)
  set_tests_properties(PyXRootD::${test} PROPERTIES ENVIRONMENT "${ENVIRON}")
endforeach()
