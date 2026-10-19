add_bats_test(bigfiles.bats)
add_bats_test(https.bats)
add_bats_test(plain.bats)
add_bats_test(protocol.bats)
add_bats_test(timeout.bats)

# The progress bar is printed only to a terminal, which script(1) provides
find_program(SCRIPT_EXECUTABLE script)
if(SCRIPT_EXECUTABLE)
  add_bats_test(progressbar.bats)
endif()

if(BUILD_MACAROONS)
  add_bats_test(macaroon.bats)
  add_bats_test(token_handle.bats)
endif()

# The OAuth test uses the SciTokens module to validate the tokens
if(BUILD_SCITOKENS)
  add_bats_test(oauth.bats)
  add_bats_test(scitokens.bats)
endif()

# A preload library which aborts a server on a close of a descriptor which is
# not open (issue #2889), for curl.bats
add_library(checkclose SHARED checkclose.cc)
target_link_libraries(checkclose ${CMAKE_DL_LIBS})

if(BUILD_SCITOKENS AND HAVE_SCITOKEN_CONFIG_SET_STR)
  add_bats_test(curl.bats)
endif()
