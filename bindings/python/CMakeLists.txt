
set(SETUP_PY_IN    "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in")
set(SETUP_PY       "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
set(DEPS           "${CMAKE_CURRENT_SOURCE_DIR}/libs/__init__.py")
set(OUTPUT         "${CMAKE_CURRENT_BINARY_DIR}/python_bindings")
set(XRD_SRCINCDIR  "${CMAKE_SOURCE_DIR}/src")
set(XRD_BININCDIR  "${CMAKE_BINARY_DIR}/src")
set(XRDCL_LIBDIR   "${CMAKE_BINARY_DIR}/src/XrdCl")
set(XRD_LIBDIR     "${CMAKE_BINARY_DIR}/src")
set(XRDCL_INSTALL  "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

if( PYPI_BUILD )
  set(XRDCL_RPATH  "$ORIGIN/${CMAKE_INSTALL_LIBDIR}")
else()
  set(XRDCL_RPATH  "$ORIGIN/../../..")
endif()

if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )
  if( CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.7 )
    message( "clang 3.5" )
    set( CLANG_PROHIBITED ", '-Wp,-D_FORTIFY_SOURCE=2', '-fstack-protector-strong'" )
  endif()
  if( ( CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0 ) OR ( CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0 ) )
    message( "clang 6.0" )
    set( CLANG_PROHIBITED ", '-fcf-protection'" )
  endif()
  if( CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0 )
    message( "clang > 7.0" )
    set( CLANG_PROHIBITED ", '-fstack-clash-protection'" )
  endif()
endif()

configure_file(${SETUP_PY_IN} ${SETUP_PY})

string(FIND "${PIP_OPTIONS}" "--prefix" PIP_OPTIONS_PREFIX_POSITION)
if( "${PIP_OPTIONS_PREFIX_POSITION}" EQUAL "-1" )
  string(APPEND PIP_OPTIONS " --prefix \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}")
else()
  message(WARNING
        " The pip option --prefix has been set in '${PIP_OPTIONS}' which will change"
        " it from its default value of '--prefix \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}'."
        " Make sure this is intentional and that you understand the effects."
        )
endif()

# Check it the Python interpreter has a valid version of pip
execute_process(
        COMMAND ${PYTHON_EXECUTABLE} -m pip --version
        RESULT_VARIABLE VALID_PIP_EXIT_CODE
        OUTPUT_QUIET
)

if ( NOT ${VALID_PIP_EXIT_CODE} EQUAL 0 )
  # Attempt to still install with deprecated invocation of setup.py
  message(WARNING
         " ${PYTHON_EXECUTABLE} does not have a valid pip associated with it."
         " It is recommended that you install a version of pip to install Python"
         " packages and bindings. If you are unable to install a version of pip"
         " through a package manager or with your Python build try using the PyPA's"
         " get-pip.py bootstrapping script ( https://github.com/pypa/get-pip ).\n"
         " The installation of the Python bindings will attempt to continue using"
         " the deprecated method of `${PYTHON_EXECUTABLE} setup.py install`."
         )

  # https://docs.python.org/3/install/#splitting-the-job-up
  add_custom_command(OUTPUT ${OUTPUT}
                     COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} --verbose build
                     DEPENDS ${DEPS})

  add_custom_target(python_target ALL DEPENDS ${OUTPUT} XrdCl)

  # Get the distribution name on Debian families
  execute_process( COMMAND grep -i ^NAME= /etc/os-release
                   OUTPUT_VARIABLE DEB_DISTRO )
  STRING(REGEX REPLACE "^NAME=\"" "" DEB_DISTRO "${DEB_DISTRO}")
  STRING(REGEX REPLACE "\".*"     "" DEB_DISTRO "${DEB_DISTRO}")

  if( DEB_DISTRO STREQUAL "Debian" OR DEB_DISTRO STREQUAL "Ubuntu" )
    set(PYTHON_LAYOUT  "unix" CACHE STRING "Python installation layout (deb or unix)")
    set(DEB_INSTALL_ARGS "--install-layout ${PYTHON_LAYOUT}")
  endif()

  install(
    CODE
    "EXECUTE_PROCESS(
      RESULT_VARIABLE INSTALL_STATUS
      COMMAND /usr/bin/env ${XROOTD_PYBUILD_ENV} ${PYTHON_EXECUTABLE} ${SETUP_PY} install \
                --verbose \
                --prefix \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX} \
                ${DEB_INSTALL_ARGS}
    )
    if(NOT INSTALL_STATUS EQUAL 0)
      message(FATAL_ERROR \"Failed to install Python bindings\")
    endif()
  ")
else()
  install(
    CODE
    "EXECUTE_PROCESS(
      RESULT_VARIABLE INSTALL_STATUS
      COMMAND /usr/bin/env ${XROOTD_PYBUILD_ENV} ${PYTHON_EXECUTABLE} -m pip install \
                ${PIP_OPTIONS} \
                ${CMAKE_CURRENT_BINARY_DIR}
      )
      if(NOT INSTALL_STATUS EQUAL 0)
        message(FATAL_ERROR \"Failed to install Python bindings\")
      endif()
  ")
endif()
