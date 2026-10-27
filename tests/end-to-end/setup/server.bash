#!/usr/bin/env bash

# Common server setups for the end-to-end tests.
#
# A test file loads this file, then calls start_server in setup_file() to
# share the servers among all tests of the file, or in setup() to start new
# servers for each test. The matching teardown_file() or teardown() calls
# stop_servers. The servers run below BATS_FILE_TMPDIR or BATS_TEST_TMPDIR,
# thus BATS removes all their files after the run.

SETUP_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

load "$SETUP_DIR/../helper/common.bash"

# The client must log at the Debug level at least, as tests check the client
# log for some messages. Each test has its own client log, which is printed
# when the test fails. Short timeouts make an error fail the test quickly,
# rather than hang it. The client uses only unix authentication, thus it
# never presents a grid proxy of the user to a server. A test of another
# security protocol sets XrdSecPROTOCOL itself.

: "${XRD_LOGLEVEL:=Debug}"
: "${XRD_REQUESTTIMEOUT:=15}"
: "${XRD_STREAMTIMEOUT:=10}"
: "${XRD_TIMEOUTRESOLUTION:=1}"

export XRD_LOGLEVEL XRD_REQUESTTIMEOUT XRD_STREAMTIMEOUT XRD_TIMEOUTRESOLUTION
export XrdSecPROTOCOL=unix

if [[ -n "${BATS_TEST_TMPDIR:-}" ]]; then
    export XRD_LOGFILE="$BATS_TEST_TMPDIR/client.log"
fi

# A port on which no test server runs: a server gets a free port from the
# kernel, above 1023, and the port of a stopped server can go to a server of
# another test file at any time.
# shellcheck disable=SC2034 # the test files use it
NO_SERVER_PORT=1

# The size of each server log when the test starts, as lines "<size> <log>".
# On failure, only the lines which the server logged during the test are
# printed, as a server shared by all tests of a file logs the requests of the
# earlier tests too. The helpers work with bash 3.2, the bash of macOS, thus
# they use no associative arrays.

SETUP_LOG_OFFSETS=

if [[ -n "${BATS_TEST_TMPDIR:-}" ]]; then
    for log in "$BATS_FILE_TMPDIR"/*/xrootd.log; do
        if [[ -f "$log" ]]; then
            SETUP_LOG_OFFSETS+="$(wc -c < "$log" | tr -d ' ') $log
"
        fi
    done
    unset log
fi

# _setup_log_offset <log>: the size of the log when the test started, or 0
_setup_log_offset() {
    local size path

    while read -r size path; do
        if [[ "$path" == "$1" ]]; then
            echo "$size"
            return
        fi
    done <<< "$SETUP_LOG_OFFSETS"
    echo 0
}

# The directory of the servers started in the current context: the test
# in setup() and @test, the file in setup_file() and teardown_file().
_setup_tmpdir() {
    echo "${BATS_TEST_TMPDIR:-$BATS_FILE_TMPDIR}"
}

# The run directory of the server: of the test, else of the file, thus a
# test can also stop or restart a server of its file.
_setup_rundir() {
    local dir

    for dir in "${BATS_TEST_TMPDIR:-/nonexistent}/$1" "$BATS_FILE_TMPDIR/$1"; do
        if [[ -d "$dir" ]]; then
            echo "$dir"
            return
        fi
    done

    echo "no server '$1'" >&2
    return 1
}

# The name of the variables which start_server exports for a server.
_setup_var() {
    local var
    var=$(printf '%s' "$1" | tr '[:lower:]' '[:upper:]')
    echo "${var//[^A-Z0-9]/_}"
}

# A Unix socket path must fit in 108 bytes, and xrootd adds a subdirectory
# and the socket name to the admin path. When the run directory is too long
# for that, the admin path goes to a new directory in TMPDIR, or in /tmp if
# TMPDIR is long as well. The link "admin" in the run directory points to
# it, thus stop_servers can remove it.
_setup_admin_dir() {
    local rundir=$1 tmp=${TMPDIR:-/tmp} dir

    if (( ${#rundir} <= 64 )); then
        mkdir "$rundir/admin" && echo "$rundir/admin"
        return
    fi

    (( ${#tmp} <= 40 )) || tmp=/tmp

    dir=$(mktemp -d "$tmp/xrd.XXXXXX") || return 1
    ln -s "$dir" "$rundir/admin"
    echo "$dir"
}

# _setup_run [--foreground] <program> <name> <port> [argument...]
#
# Start the program, xrootd or cmsd, with the configuration in the run
# directory, on the given port, and with the extra arguments, and wait until
# its initialization completes. For xrootd, write the port which it bound to
# the run directory. The daemon runs in the background (-b), or with
# --foreground as a job of the shell, as a service manager runs it.
_setup_run() {
    local daemon=-b
    if [[ "$1" == --foreground ]]; then
        daemon=
        shift
    fi
    local prog=$1 name=$2 port=$3
    shift 3
    local rundir admin log pidfile line offset=0
    rundir=$(_setup_rundir "$name") || return 1
    admin="$rundir/admin"
    # a link to a directory outside of a long run directory
    if [[ -L "$admin" ]]; then
        admin=$(readlink "$admin")
    fi
    log="$rundir/$prog.log"
    pidfile="$rundir/$prog.pid"

    # A restart appends to the log of the earlier run, which has a line of its
    # initialization too, thus look only at the lines after the end of it
    if [[ -f "$log" ]]; then
        offset=$(wc -c < "$log" | tr -d ' ')
    fi

    # The program with -b exits when the daemon reports the result of its
    # configuration, with a nonzero status on failure; in the foreground, the
    # subshell succeeds. The daemon must not keep the descriptors 3 and 4, on
    # which BATS reads the output of the test until all holders close them.
    if (
        cd "$rundir" &&
        SETUP_NAME=$name SETUP_RUNDIR=$rundir SETUP_ADMIN=$admin SETUP_PORT=$port \
        SETUP_BINDIR=$(dirname "$(command -v xrootd)") \
        SETUP_CREDENTIALS=${CREDENTIALS:-} SETUP_TESTDIR=${BATS_TEST_DIRNAME:-} \
        SETUP_SETUPDIR=$SETUP_DIR \
            XDG_CACHE_HOME="$rundir/cache" "$prog" $daemon -c "$rundir/$name.cfg" -l "$log" -s "$pidfile" "$@" \
                < /dev/null > "$rundir/$prog.out" 2>&1 3>&- 4>&- &
        if [[ -n "$daemon" ]]; then
            wait $!
        fi
    ); then
        # the daemon logs this line at the end of its configuration; the
        # components log similar lines before it, thus match the whole line
        for _ in {1..200}; do
            line=$(tail -c +$(( offset + 1 )) "$log" 2>/dev/null |
                   grep -asE "^------ $prog .*:[0-9]+ initialization (completed|failed)" || true)

            if [[ "$line" == *completed* ]]; then
                line=${line% initialization completed.}
                [[ "$prog" == xrootd ]] && echo "${line##*:}" > "$rundir/port"
                return 0
            fi

            [[ "$line" == *failed* ]] && break

            # the pid file exists but the daemon is gone
            if [[ -s "$pidfile" ]] && ! is_running "$(<"$pidfile")"; then
                break
            fi

            sleep 0.05
        done
    fi

    echo "$prog '$name' failed to start" >&2
    [[ -s "$rundir/$prog.out" ]] && sed "s|^|[$name] |" "$rundir/$prog.out" >&2
    [[ -f "$log" ]] && sed "s|^|[$name] |" "$log" >&2
    [[ -s "$pidfile" ]] && stop_pid "$(<"$pidfile")"
    return 1
}

_setup_run_xrootd() {
    _setup_run xrootd "$@"
}

# start_server <name> <role> [snippet...]
#
# Start a server with the configuration base.cfg + <role>.cfg + snippets. The
# roles are server (a data server with unix authentication), server-nosec (a
# data server without the security library) and cache (a caching proxy in
# front of the server at $ORIGIN, see cache/cache.bash). A snippet is either a
# word, for the snippet <word>.cfg of this directory (for example http, tls,
# http-tpc), or a file, which ends in .cfg or has a slash, with a path
# relative to the directory of the test file. Export, with <NAME> the server
# name in upper case:
#
#   <NAME>_URL   root://localhost:<port>
#   <NAME>_URL4  root://127.0.0.1:<port>, for tests of the IPv4 stack
#   <NAME>_URL6  root://[::1]:<port>, for tests of the IPv6 stack
#   <NAME>_HTTP  http://localhost:<port>, with the snippet http
#   <NAME>_HTTPS https://localhost:<port>, with the snippets http and tls
#   <NAME>_PORT  the port, which the server chose
#   <NAME>_DATA  the data directory of the server (oss.localroot)
#   <NAME>_LOG   the server log
start_server() {
    _setup_compose "$@" || return 1
    _setup_run_xrootd "$1" any || return 1
    _setup_export "$1"
}

# _setup_compose <name> <role> [snippet...]
#
# Make the run directory of the server and write its configuration.
_setup_compose() {
    local name=$1 role=$2
    shift 2

    local rundir snippet
    rundir="$(_setup_tmpdir)/$name"

    if [[ -e "$rundir" ]]; then
        echo "start_server: server '$name' exists already" >&2
        return 1
    fi

    mkdir -p "$rundir/data"
    _setup_admin_dir "$rundir" >/dev/null || return 1

    cat "$SETUP_DIR/base.cfg" "$SETUP_DIR/$role.cfg" > "$rundir/$name.cfg" || return 1

    for snippet in "$@"; do
        if [[ "$snippet" != *.cfg && "$snippet" != */* ]]; then
            snippet="$SETUP_DIR/$snippet.cfg"
        elif [[ "$snippet" != /* ]]; then
            snippet="$BATS_TEST_DIRNAME/$snippet"
        fi
        { echo; cat "$snippet"; } >> "$rundir/$name.cfg" || return 1
    done
}

# _setup_export <name>
#
# Export the variables of a running server (see start_server).
_setup_export() {
    local name=$1 rundir var port
    rundir="$(_setup_tmpdir)/$name"
    var=$(_setup_var "$name")
    port=$(<"$rundir/port")

    export "${var}_PORT=$port"
    export "${var}_URL=root://localhost:$port"
    export "${var}_URL4=root://127.0.0.1:$port"
    export "${var}_URL6=root://[::1]:$port"
    export "${var}_HTTP=http://localhost:$port"
    export "${var}_HTTPS=https://localhost:$port"
    export "${var}_DATA=$rundir/data"
    export "${var}_LOG=$rundir/xrootd.log"
}

# stop_server <name>
#
# Stop one server. A server of another test file can take its port later,
# thus a test of a port without a server uses NO_SERVER_PORT.
stop_server() {
    local name=$1 pidfile
    pidfile="$(_setup_rundir "$name")/xrootd.pid" || return 1

    stop_pid "$(<"$pidfile")" && rm -f "$pidfile"
}

# restart_server <name>
#
# Stop the server, then start it again on the same port, with the same
# configuration and data.
restart_server() {
    local name=$1 rundir
    rundir=$(_setup_rundir "$name") || return 1

    stop_pid "$(<"$rundir/xrootd.pid")" || return 1
    rm -f "$rundir/xrootd.pid"
    _setup_run_xrootd "$name" "$(<"$rundir/port")"
}

# start_issuer
#
# Start the token issuer "issuer", an HTTPS server which publishes the key of
# the shared credentials (issuer.jwks), and write the configuration of the
# snippet scitokens, which accepts the tokens of the issuer. Export ISSUER,
# the URL of the issuer, which make_token puts into the tokens.
start_issuer() {
    start_server issuer server http tls || return 1

    mkdir -p "$ISSUER_DATA/.well-known"
    cp "$CREDENTIALS/issuer.jwks" "$ISSUER_DATA/issuer.jwks"
    printf '{ "issuer": "%s", "jwks_uri": "%s/issuer.jwks" }\n' "$ISSUER_HTTPS" "$ISSUER_HTTPS" \
        > "$ISSUER_DATA/.well-known/openid-configuration"

    export ISSUER=$ISSUER_HTTPS

    cat > "$(_setup_tmpdir)/scitokens.conf" <<EOF
[Global]
onmissing = passthrough
audience = https://xrootd.test

[Issuer test]
issuer = $ISSUER
base_path = /
default_user = nobody
authorization_strategy = capability
EOF
}

# start_collector <name>
#
# Start a collector of monitoring data in the directory <name> of the current
# context: it receives UDP datagrams on a free port and writes each one as a
# line of a file. Export <NAME>_PORT, the port, and <NAME>_RECORDS, the file;
# a snippet names the port with "set x = $<NAME>_PORT". stop_servers stops it.
start_collector() {
    local name=$1 dir var
    dir="$(_setup_tmpdir)/$name"
    var=$(_setup_var "$name")

    require_commands python3
    mkdir -p "$dir"
    touch "$dir/records"
    python3 "$SETUP_DIR/udp-collector.py" "$dir/port" "$dir/records" "$dir/collector.pid" || return 1

    for _ in {1..100}; do
        [[ -s "$dir/collector.pid" ]] && break
        sleep 0.05
    done

    export "${var}_PORT=$(<"$dir/port")"
    export "${var}_RECORDS=$dir/records"
}

# make_token <scope> [lifetime]
#
# Print a token of the issuer (see start_issuer) with the scope, for example
# "storage.read:/ storage.create:/", valid for the lifetime in seconds.
make_token() {
    xrdscitokens-create-token "$CREDENTIALS/issuer.pub" "$CREDENTIALS/issuer.key" \
        test "$ISSUER" "$1" ${2:+"$2"}
}

# server_log <name>
#
# Print the lines which the server logged during the current test. A server
# of the file logs the requests of the earlier tests too, which a test must
# not see.
server_log() {
    local log

    for log in "${BATS_TEST_TMPDIR:-/nonexistent}/$1/xrootd.log" "$BATS_FILE_TMPDIR/$1/xrootd.log"; do
        if [[ -f "$log" ]]; then
            tail -c +$(( $(_setup_log_offset "$log") + 1 )) "$log"
            return
        fi
    done

    echo "server_log: no server '$1'" >&2
    return 1
}

# server_is_up <url>
#
# Succeed if the server at the URL answers a query.
server_is_up() {
    XRD_CONNECTIONRETRY=0 xrdfs "$1" query config version >/dev/null 2>&1
}

# stop_servers
#
# Stop all servers and daemons of the current context, remove the admin
# directories outside of it, and fail if a process of a server survives.
stop_servers() {
    local tmp pidfile link survivors pid status=0
    tmp=$(_setup_tmpdir)

    # the servers, and other daemons of the setup with a pid file; a process
    # which does not stop fails the call, after the cleanup below
    for pidfile in "$tmp"/*/*.pid; do
        if [[ -s "$pidfile" ]]; then
            stop_pid "$(<"$pidfile")" || status=1
        fi
        rm -f "$pidfile"
    done

    for link in "$tmp"/*/admin; do
        [[ -L "$link" ]] || continue
        rm -rf "$(readlink "$link")"
        rm -f "$link"
    done

    # Only -f is portable: -a lists the ancestors on macOS, and the pgrep of
    # BusyBox has no -u. The path is unique, thus it matches no other process.
    survivors=$(pgrep -f -- "$tmp/" || true)

    if [[ -n "$survivors" ]]; then
        echo "stop_servers: processes survived:" >&2
        for pid in $survivors; do
            ps -o pid=,args= -p "$pid" >&2 2>/dev/null || echo "$pid" >&2
        done
        return 1
    fi

    return "$status"
}

# On failure, print the client log of the test and the lines which each
# server logged during the test.
bats::on_failure() {
    local log name

    [[ -z "${BATS_SKIP_SERVER_LOGS:-}" ]] || return 0

    for log in "${BATS_FILE_TMPDIR:-/nonexistent}"/*/xrootd.log \
               "${BATS_TEST_TMPDIR:-/nonexistent}"/*/xrootd.log \
               "${BATS_TEST_TMPDIR:-/nonexistent}"/client.log; do
        [[ -s "$log" ]] || continue
        name=$(basename "$(dirname "$log")")
        [[ "$log" == */client.log ]] && name=client
        printf '\n'
        tail -c +$(( $(_setup_log_offset "$log") + 1 )) "$log" | sed "s|^|[$name] |"
        printf '\n'
    done
}
