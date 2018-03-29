
set(SETUP_PY_IN    "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in")
set(SETUP_PY       "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
set(DEPS           "${CMAKE_CURRENT_SOURCE_DIR}/libs/__init__.py")
set(OUTPUT         "${CMAKE_CURRENT_BINARY_DIR}/python_bindings")
set(XRD_SRCINCDIR  "${CMAKE_SOURCE_DIR}/src")
set(XRD_BININCDIR  "${CMAKE_BINARY_DIR}/src")
set(XRDCL_LIBDIR   "${CMAKE_BINARY_DIR}/src/XrdCl")
set(XRD_LIBDIR     "${CMAKE_BINARY_DIR}/src")

if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )
  if( CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.7 )
    message( "clang 3.5" )
    set( CLANG_PROHIBITED ", '-Wp,-D_FORTIFY_SOURCE=2', '-fstack-protector-strong'" )
  endif()
endif()

configure_file(${SETUP_PY_IN} ${SETUP_PY})

add_custom_command(OUTPUT ${OUTPUT}
                   COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} build
                   DEPENDS ${DEPS})

add_custom_target(python_target ALL DEPENDS ${OUTPUT} XrdCl)

install(
  CODE
  "EXECUTE_PROCESS(
    COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} install --prefix \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX} --record PYTHON_INSTALLED)")

