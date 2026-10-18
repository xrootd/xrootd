#!/usr/bin/env bash

# Logins to a server without security and to a server which requires unix
# authentication. The tests share the servers.

load ../setup/server.bash

setup_file() {
    start_server nosec server-nosec
    start_server unix server
}

teardown_file() {
    stop_servers
}

@test "a server without security accepts a client which does not authenticate" {
    run -0 xrdfs "$NOSEC_URL" query config version
}

@test "a server without security logs a login without a user name" {
    xrdfs "$NOSEC_URL" query config version
    run -0 grep -E 'XrootdXeq: .* login$' <(server_log nosec)
}

@test "a server without security warns that it has no security library" {
    run -0 grep -F "Config warning: 'xrootd.seclib' not specified" "$NOSEC_LOG"
}

@test "a server with unix authentication logs the login as the user" {
    xrdfs "$UNIX_URL" query config version
    run -0 grep -E "XrootdXeq: .* login as $(id -un)\$" <(server_log unix)
}

@test "the client authenticates with unix to a server which requires it" {
    xrdfs "$UNIX_URL" query config version
    run -0 grep -F 'Authenticated with unix' "$XRD_LOGFILE"
}
