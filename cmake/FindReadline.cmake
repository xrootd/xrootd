include( FindPackageHandleStandardArgs )
include( CheckCXXSourceCompiles )

if( READLINE_INCLUDE_DIR AND READLINE_LIBRARY )
  set( READLINE_FOUND TRUE )
else( READLINE_INCLUDE_DIR AND READLINE_LIBRARY )
  find_path( READLINE_INCLUDE_DIR readline/readline.h /usr/include/readline )

  find_library(
    READLINE_LIB
    NAMES readline
    HINTS
    ${READLINE_ROOT_DIR}
    PATH_SUFFIXES
    ${LIBRARY_PATH_PREFIX}
    ${LIB_SEARCH_OPTIONS})

  #-----------------------------------------------------------------------------
  # Check if we need ncurses - a hack required for SLC5
  #-----------------------------------------------------------------------------
  if( READLINE_LIB )

    set( CMAKE_REQUIRED_LIBRARIES ${READLINE_LIB} )
    set( CMAKE_REQUIRED_INCLUDES ${READLINE_INCLUDE_DIR} )
    check_cxx_source_compiles(
      "
      #include <stdio.h>
      #include <readline/readline.h>
      int main()
      {
        char shell_prompt[100];
        readline(shell_prompt);
        return 0;
      }
      "
      READLINE_OK )

      if( READLINE_OK )
        set( READLINE_LIBRARY ${READLINE_LIB} )
      else()
        find_library(
          NCURSES_LIBRARY
          NAMES ncurses
          HINTS
          ${READLINE_ROOT_DIR}
          PATH_SUFFIXES
          ${LIBRARY_PATH_PREFIX}
          ${LIB_SEARCH_OPTIONS})

        if( NCURSES_LIBRARY )
          set( READLINE_LIBRARY "${READLINE_LIB};${NCURSES_LIBRARY}" )
        endif()

      endif()

    endif()

  find_package_handle_standard_args(
    READLINE
    DEFAULT_MSG
    READLINE_LIBRARY READLINE_INCLUDE_DIR )

  mark_as_advanced( READLINE_INCLUDE_DIR READLINE_LIBRARY )
endif( READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
