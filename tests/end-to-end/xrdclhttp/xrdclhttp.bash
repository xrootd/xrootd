# Helpers of the XrdClHttp tests: the issuer of the tokens, an HTTPS origin,
# and an HTTPS cache in front of it. Both use TLS and the tokens of the
# issuer; /test-public needs no token. root:// needs ztn on the cache, and no
# authentication on the origin.

load ../setup/server.bash

export X509_CERT_FILE="$CREDENTIALS/ca.pem"
export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"
export XRD_HTTPCERTFILE="$CREDENTIALS/ca.pem"
export XrdSecPROTOCOL=ztn

# start_https_cache
#
# Start the issuer, the origin and the cache, make the tokens, the test data,
# and the settings file of the gtests. Export TAPE, the root of the Tape REST
# API, DISK, the local root of the origin, READ_TOKEN and WRITE_TOKEN, the
# token files, and ENV_FILE, the settings file.
start_https_cache() {
    require_plugin libXrdHttp
    require_plugin libXrdAccSciTokens
    require_plugin libXrdHttpTapeApi
    require_plugin libXrdPfc

    export TAPE="$BATS_FILE_TMPDIR/tape" DISK="$BATS_FILE_TMPDIR/tape/disk"
    mkdir -p "$TAPE/archive/test/tape" "$DISK/test" "$DISK/test-public/subdir/test3" \
        "$BATS_FILE_TMPDIR/cache-spaces/data" "$BATS_FILE_TMPDIR/cache-spaces/meta"

    start_issuer || return 1

    export READ_TOKEN="$BATS_FILE_TMPDIR/read.token" WRITE_TOKEN="$BATS_FILE_TMPDIR/write.token"
    make_token storage.read:/ 600 > "$READ_TOKEN"
    make_token storage.modify:/ 600 > "$WRITE_TOKEN"
    chmod 0600 "$READ_TOKEN" "$WRITE_TOKEN"
    export BEARER_TOKEN_FILE=$READ_TOKEN

    make_https_data

    # the client of the cache reads the origin, and its slow transfers fail
    # after 2 s. XrdClHttp presents a grid proxy of the user, from
    # /tmp/x509up_u<uid>, which the test CA cannot verify, thus point the
    # servers at a proxy which cannot exist.
    export XRD_HTTPSLOWRATEBYTESSEC=1024 XRD_HTTPSTALLTIMEOUT=2
    local snippets=(http-port.cfg tls https-tpc scitokens authdb.cfg)
    X509_USER_PROXY=/dev/null/x509 start_server origin server "${snippets[@]}" origin.cfg || return 1
    export ORIGIN="https://localhost:$ORIGIN_PORT"
    X509_USER_PROXY=/dev/null/x509 start_server cache cache "${snippets[@]}" ztn cache.cfg || return 1
    unset XRD_HTTPSLOWRATEBYTESSEC XRD_HTTPSTALLTIMEOUT

    export ENV_FILE="$BATS_FILE_TMPDIR/env"
    cat > "$ENV_FILE" <<ENV
ORIGIN_URL=$ORIGIN_HTTPS
CACHE_URL=$CACHE_HTTPS
X509_CA_FILE=$CREDENTIALS/ca.pem
READ_TOKEN=$READ_TOKEN
WRITE_TOKEN=$WRITE_TOKEN
XRD_PLUGINCONFDIR=$XRD_PLUGINCONFDIR
ENV
}

# make_https_data
#
# Make the files of the origin and of its tape.
make_https_data() {
    printf '%s\n' 'tape contents' > "$TAPE/archive/test/tape/file"
    echo 'Hello, World' > "$DISK/test/hello_world.txt"
    echo 'Hello, World' > "$DISK/test/slow_open.txt"
    echo 'Hello, World' > "$DISK/test-public/hello_world.txt"
    touch "$DISK/test-public/subdir/test1"
    echo 'Hello, world!' > "$DISK/test-public/subdir/test2"
    make_random_file "$DISK/test-public/hello_world-1mb.txt" $(( 4 << 20 ))
}
