#!/usr/bin/env bash

# The modification times which xrdfs prints are UTC, without a time zone,
# whatever the time zone of the client, over root://, http:// and dav://.
# Each test has its own server, with a file of a known time.

load ../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
# A time zone in the POSIX format, UTC+5:30, which needs no zoneinfo files:
# without the files, libc takes an unknown zone name as UTC
export TZ=IST-5:30

setup() {
    require_plugin libXrdHttp
    start_server http.cfg
    echo 'content' > "$SERVER_DATA/file"
    TZ=UTC touch -t 202408211102.53 "$SERVER_DATA/file"
}

teardown() {
    stop_servers
}

@test "stat prints the modification time in UTC" {
    run -0 xrdfs "$SERVER_URL" stat /file
    assert_line --regexp '^MTime: +2024-08-21 11:02:53$'
}

@test "ls -l prints the modification time in UTC" {
    run -0 xrdfs "$SERVER_URL" ls -l /
    assert_line --regexp ' 2024-08-21 11:02:53 /file$'
}

@test "stat over HTTP prints the modification time of the file" {
    require_plugin libXrdClHttp
    run -0 xrdfs "$SERVER_HTTP" stat /file
    assert_line --regexp '^MTime: +2024-08-21 11:02:53$'
}

@test "stat over WebDAV prints the modification time of the file" {
    require_plugin libXrdClHttp
    run -0 xrdfs "dav://localhost:$SERVER_PORT" stat /file
    assert_line --regexp '^MTime: +2024-08-21 11:02:53$'
}
