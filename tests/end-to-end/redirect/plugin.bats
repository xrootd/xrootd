#!/usr/bin/env bash

# The redirect plugin of xrootd (xrootd.redirlib), on the paths of HTTP TPC
# and of the XRootD protocol, for redirects to a host and port and to a URL.
# The tests share one server.

load ../setup/server.bash

setup_file() {
    require_plugin libXrdHttp
    require_plugin libXrdHttpTPC
    require_commands curl
    start_server server server http http-tpc plugin.cfg
    # the HTTP TPC handler asks the source with HEAD before the open
    echo 'source contents' > "$SERVER_DATA/source.txt"
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    echo 'upload payload' > upload.txt
}

# The response headers of an HTTP COPY with tpc.key in the URL of the
# destination; curl does not follow the redirect
copy() {
    curl --silent -i -X COPY -H "Source: $SERVER_HTTP/source.txt" \
        "$SERVER_HTTP/destfile?tpc.key=test" -o - | tr -d '\r'
}

# The last redirect in the log of the client
last_redirect() {
    grep -oE 'Redirected from: [^[:space:]]+ to: [^[:space:]]+' "$XRD_LOGFILE" | tail -1
}

@test "an HTTP COPY to a destination with tpc.key gets a redirect" {
    run -0 copy
    assert_line --regexp '^HTTP/.* 307'
}

@test "the redirect plugin rewrites the host of the HTTP TPC redirect" {
    run -0 copy
    assert_line --regexp '^[Ll]ocation:.*rdlplugin-rewritten'
}

@test "the redirect plugin rewrites the port of the HTTP TPC redirect" {
    run -0 copy
    assert_line --regexp '^[Ll]ocation:.*:9999'
}

@test "the HTTP TPC redirect joins the CGI of the client and of the OFS with &" {
    run -0 copy
    assert_line --regexp '^[Ll]ocation:.*tpc\.key=test&sfsopq=sfsval'
}

@test "an XRootD open with tpc.key gets the target of the redirect plugin" {
    XRD_CONNECTIONRETRY=0 xrdcp -f upload.txt "$SERVER_URL//destfile?tpc.key=test" || true
    run -0 last_redirect
    assert_output --regexp 'to: .*rdlplugin-rewritten:9999'
}

@test "a URL redirect of the OSS gets the URL of the redirect plugin" {
    XRD_CONNECTIONRETRY=0 xrdcp -f upload.txt "$SERVER_URL//urlredir/destfile" || true
    run -0 last_redirect
    assert_output --regexp 'to: .*rdlplugin-rewritten-url:9997'
}
