# Helper functions

run_test_case() {
    local test_case_dir=$1
    local idx=$(basename $test_case_dir | cut -d'-' -f1)
    . $test_case_dir/main.sh
    echo "\nRunning test case $idx: $TEST_CASE_NAME\n"
    echo "Using test workspace: $WORKSPACE"
    test_init
    test_main
    test_finalize
    echo "\nTest case $idx: SUCCESS\n"
}

start_caddy() {
    local www_root=$1
    local caddyfile=$2

    ulimit -n 8192

    echo -n "Starting Caddy... "
    $CADDY_EXEC -root $www_root -conf $caddyfile -log $WORKSPACE/caddy.log -pidfile $WORKSPACE/caddy_pid &
    sleep 1
    echo "done."
}

stop_caddy() {
    if [ -f $WORKSPACE/caddy_pid ]; then
        echo -n "Stopping Caddy... "
        kill $(cat $WORKSPACE/caddy_pid)
        rm -f $WORKSPACE/caddy_pid
        echo "done."
    fi
}

generate_random_string() {
    local length=$1
    cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w $length | head -n 1
}

file_sha1() {
    echo $(sha1sum $1 | cut -d' ' -f1)
}

str_sha1() {
    echo $(echo $1 | sha1sum | cut -d' ' -f1)
}

die() {
    echo $1
    exit 1
}

create_workspace() {
    mktemp -d -p /tmp/ xrdclhttp-test-ws.XXXXXXXXXX
}

check_executable() {
    which $1 > /dev/null || die "Could not find executable: $1"
}

check_file() {
    local needle=$1
    local haystack=$2
    local res=$(find $haystack -name $needle)
    if [ x"$res" = x"" ]; then
        echo "Could not find file $needle in $haystack"
        exit 1
    fi
}

check_prefix() {
    [ ! -z $XROOTD_PREFIX ] || die "The variable XROOTD_PREFIX is needed to point to the XRootD installation"
    check_file xrootd $XROOTD_PREFIX/bin
    check_file xrdcp $XROOTD_PREFIX/bin
    check_file libXrdClHttp-4.so $XROOTD_PREFIX/lib
}

clean_up() {
    if [ x"$WORKSPACE" != x"" ] && [ -d $WORKSPACE ]; then
        stop_caddy
        echo "Cleaning up: $WORKSPACE"
        rm -r $WORKSPACE
    fi
}

trap clean_up EXIT HUP INT TERM || return $?

