#!/usr/bin/env bash

# A PUT with a known length tells the OSS the size of the file, with
# ?oss.asize=<length> on the open; a chunked PUT cannot. The log does not
# show the CGI, but the length of the open request (dlen), which grows by the
# length of the CGI. The tests share one server.

load ../../setup/server.bash

readonly CONTENT='abcdefghijklmnopqrstuvw987'

setup_file() {
    require_plugin libXrdHttp
    require_commands curl
    start_server server server http
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    FILE="/t${BATS_TEST_NUMBER}.txt"
    printf '%s' "$CONTENT" > local.txt
}

# The length of the request which opened the file of the test
open_length() {
    server_log server | grep -B 2 -F "open odm $FILE" | grep -o 'Bridge req=3010 dlen=[0-9]*' | cut -d= -f3
}

@test "a PUT with a known length opens the file with the size" {
    curl --silent --fail "$SERVER_HTTP$FILE" --upload-file local.txt
    local cgi="?oss.asize=${#CONTENT}"
    run -0 open_length
    assert_output $(( ${#FILE} + ${#cgi} + 1 ))
}

@test "a chunked PUT opens the file without a size" {
    curl --silent --fail -H 'Transfer-Encoding: chunked' "$SERVER_HTTP$FILE" --upload-file local.txt
    run -0 open_length
    assert_output $(( ${#FILE} + 1 ))
}

@test "a chunked PUT stores the content" {
    curl --silent --fail -H 'Transfer-Encoding: chunked' "$SERVER_HTTP$FILE" --upload-file local.txt
    run -0 cmp local.txt "$SERVER_DATA$FILE"
}
