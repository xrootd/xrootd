
set(SETUP_PY_IN    "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in")
set(SETUP_PY       "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
set(DEPS           "${CMAKE_CURRENT_SOURCE_DIR}/libs/__init__.py")
set(OUTPUT         "${CMAKE_CURRENT_BINARY_DIR}/python_bindings")
set(XRD_SRCINCDIR  "${CMAKE_SOURCE_DIR}/src")
set(XRD_BININCDIR  "${CMAKE_BINARY_DIR}/src")
set(XRDCL_LIBDIR   "${CMAKE_BINARY_DIR}/src/XrdCl")
set(XRD_LIBDIR     "${CMAKE_BINARY_DIR}/src")

configure_file(${SETUP_PY_IN} ${SETUP_PY})

add_custom_command(OUTPUT ${OUTPUT}
                   COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} build
                   DEPENDS ${DEPS})

add_custom_target(python_target ALL DEPENDS ${OUTPUT} XrdCl)

install(
  CODE
  "EXECUTE_PROCESS(
    COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} install --prefix \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX} --record PYTHON_INSTALLED)
   EXECUTE_PROCESS(
     COMMAND cat ${CMAKE_BINARY_DIR}/PYTHON_INSTALLED
     COMMAND sed -e \"s|\$ENV{DESTDIR}||g\"
     OUTPUT_FILE ${CMAKE_BINARY_DIR}/PYTHON_INSTALLED_FILES )")

