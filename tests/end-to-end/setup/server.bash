#!/usr/bin/env bash

# Common server setups for the end-to-end tests.
#
# A test file loads this file, then calls start_server in setup_file() to
# share the servers among all tests of the file, or in setup() to start new
# servers for each test. The matching teardown_file() or teardown() calls
# stop_servers. The servers run below BATS_FILE_TMPDIR or BATS_TEST_TMPDIR,
# thus BATS removes all their files after the run.

# shellcheck disable=SC2155 # the directory of this file exists
readonly SETUP_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

load "$SETUP_DIR/../helper/common.bash"

# The client must log at the Debug level at least, as tests check the client
# log for some messages, thus a level of the user applies only when it is
# Dump. Each test has its own client log, which is printed when the test
# fails. Short timeouts make an error fail the test quickly, rather than hang
# it. The client uses only unix authentication, and helper/common.bash hides
# the credentials of the user from the client. A test of another security
# protocol sets XrdSecPROTOCOL itself.

[[ "${XRD_LOGLEVEL:-}" == Dump ]] || XRD_LOGLEVEL=Debug
: "${XRD_REQUESTTIMEOUT:=15}"
: "${XRD_STREAMTIMEOUT:=10}"
: "${XRD_TIMEOUTRESOLUTION:=1}"

export XRD_LOGLEVEL XRD_REQUESTTIMEOUT XRD_STREAMTIMEOUT XRD_TIMEOUTRESOLUTION
export XrdSecPROTOCOL=unix

if [[ -n "${BATS_TEST_TMPDIR:-}" ]]; then
    export XRD_LOGFILE="$BATS_TEST_TMPDIR/client.log"
fi

# A port on which no test server runs: a server gets a random port from 21000
# to 31999, and the port of a stopped server can go to a server of another
# test file at any time.
# shellcheck disable=SC2034 # the test files use it
readonly NO_SERVER_PORT=1

# The tests of a file share its servers, and each test empties their logs
# (see below), thus "bats --jobs" must not run them in parallel.
# shellcheck disable=SC2034 # BATS reads it
BATS_NO_PARALLELIZE_WITHIN_FILE=true

# A server shared by all tests of a file logs the requests of each test.
# Each test starts with an empty log of each such server, thus the test sees
# only its own lines. The server writes its log with O_APPEND, thus its next
# line goes to the start of the empty file.

if [[ -n "${BATS_TEST_TMPDIR:-}" ]]; then
    for log in "$BATS_FILE_TMPDIR"/*/xrootd.log; do
        if [[ -f "$log" ]]; then
            : > "$log"
        fi
    done
    unset log
fi

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

# A Unix socket path must fit in 108 bytes, and xrootd adds the instance
# name, a subdirectory and the socket name to the admin path. When the run
# directory is too long for that, the admin path goes to a new directory in
# TMPDIR, or in /tmp if TMPDIR is long as well. The link "admin" in the run
# directory points to it, thus stop_servers can remove it.
_setup_admin_dir() {
    local rundir=$1 name=${1##*/} tmp=${TMPDIR:-/tmp} dir

    if (( ${#rundir} + ${#name} <= 64 )); then
        mkdir -p "$rundir/admin" && echo "$rundir/admin"
        return
    fi

    (( ${#tmp} <= 40 )) || tmp=/tmp

    dir=$(mktemp -d "$tmp/xrd.XXXXXX") || return 1
    ln -s "$dir" "$rundir/admin" || { rm -rf "$dir"; return 1; }
    echo "$dir"
}

# _setup_run [--foreground] <program> <name> <port> [argument...]
#
# Start the program, xrootd or cmsd, with the configuration in the run
# directory, on the given port, or on a random port for "any", and with the
# extra arguments. The options are those of the systemd units: the instance
# name is the server name, thus the program has its home in the run
# directory, and logs to <program>.log there. For xrootd, write the port to
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
    local rundir admin log offset bind=$port
    rundir=$(_setup_rundir "$name") || return 1
    admin="$rundir/admin"
    # a link to a directory outside of a long run directory
    if [[ -L "$admin" ]]; then
        admin=$(readlink "$admin")
    fi
    log="$rundir/$prog.log"

    # For "any", a random port above the fixed ports of the old test suites
    # (up to 20972), and below the ephemeral ports of Linux (32768) and macOS
    # (49152), on which the system puts the clients. When another process has
    # the port, the daemon fails to start, and the next try uses another port.
    for _ in 1 2 3 4 5; do
        if [[ "$port" == any ]]; then
            bind=$(( 21000 + RANDOM % 11000 ))
        fi

        # an earlier try or start of the daemon logged to the same file
        offset=0
        if [[ -f "$log" ]]; then
            offset=$(wc -c < "$log" | tr -d ' ')
        fi

        if _setup_daemon $daemon "$prog" "$name" "$bind" "$rundir" "$admin" "$@"; then
            [[ "$prog" == xrootd ]] && echo "$bind" > "$rundir/port"
            [[ "$prog" == cmsd ]] && echo "$bind" > "$rundir/cmsd.port"
            return 0
        fi

        # The daemon reports the failure before it logs it, and then exits.
        # Wait for the exit, at most 5 s, so that the log is complete.
        for _ in {1..100}; do
            pgrep -f -- "^$prog .*-c $rundir/$name.cfg" >/dev/null || break
            sleep 0.05
        done

        # only a port in use is worth another try; the text of the error
        # differs between C libraries ("address in use" on musl). With
        # SO_REUSEADDR, two daemons can bind one port, and the second one
        # then fails to listen.
        if [[ "$port" != any ]] || ! tail -c +$(( offset + 1 )) "$log" 2>/dev/null | tail -n 2 |
            grep -qE 'Unable to (bind socket to port|listen on stream)'; then
            break
        fi
    done

    echo "$prog '$name' failed to start" >&2
    [[ -s "$rundir/$prog.out" ]] && sed "s|^|[$name] |" "$rundir/$prog.out" >&2
    [[ -f "$log" ]] && sed "s|^|[$name] |" "$log" >&2
    stop_pid_file "$rundir/$prog.pid"
    return 1
}

# _setup_daemon [-b] <program> <name> <port> <rundir> <admin> [argument...]
#
# Run the program once. With -b, the program exits when the daemon reports
# the result of its configuration, with a nonzero status on failure. In the
# foreground, wait for the line which the daemon logs at the end of its
# configuration. A configuration which hangs, for example on a lookup on the
# network, fails after SETUP_START_TIMEOUT seconds (30 by default), and the
# daemon stops. With SETUP_PRELOAD, the daemon runs with that library
# preloaded, and the other programs of the setup do not. The daemon must not
# keep the descriptors 3 and 4, on which BATS reads the output of the test
# until all holders close them.
_setup_daemon() {
    local daemon=
    if [[ "$1" == -b ]]; then
        daemon=-b
        shift
    fi
    local prog=$1 name=$2 port=$3 rundir=$4 admin=$5 line offset=0 pid i
    local preload=()
    shift 5

    if [[ -n "${SETUP_PRELOAD:-}" && "$(uname)" == Darwin ]]; then
        preload=(env "DYLD_INSERT_LIBRARIES=$SETUP_PRELOAD")
    elif [[ -n "${SETUP_PRELOAD:-}" ]]; then
        preload=(env "LD_PRELOAD=$SETUP_PRELOAD")
    fi

    # an earlier try or start of the daemon logged to the same file
    if [[ -f "$rundir/$prog.log" ]]; then
        offset=$(wc -c < "$rundir/$prog.log" | tr -d ' ')
    fi
    # the pid file of a try which failed names a process which is gone
    rm -f "$rundir/$prog.pid"

    (
        SETUP_NAME=$name SETUP_RUNDIR=$rundir SETUP_PORT=$port \
        SETUP_BINDIR=$(dirname "$(command -v xrootd)") \
        SETUP_CREDENTIALS=${CREDENTIALS:-} SETUP_TESTDIR=${BATS_TEST_DIRNAME:-} \
        SETUP_SETUPDIR=$SETUP_DIR \
            XDG_CACHE_HOME="$rundir/cache" ${preload[@]+"${preload[@]}"} "$prog" $daemon -k fifo -n "$name" \
                -a "$admin" -w "${rundir%/*}" -c "$rundir/$name.cfg" \
                -l "${rundir%/*}/$prog.log" -s "$rundir/$prog.pid" "$@" \
                < /dev/null > "$rundir/$prog.out" 2>&1 3>&- 4>&- &
        if [[ -n "$daemon" ]]; then
            pid=$!
            for (( i = 0; i < ${SETUP_START_TIMEOUT:-30} * 20; i++ )); do
                is_running "$pid" || break
                sleep 0.05
            done
            if is_running "$pid"; then
                echo "$prog '$name' reports no configuration after ${SETUP_START_TIMEOUT:-30} s" >&2
                # Before 5.3, bash reports on stderr a job which a signal
                # killed, if it reaps the job before wait, here at pkill
                { _setup_kill_daemon "$rundir" "$prog"; wait "$pid"; } 2>/dev/null
                exit 1
            fi
            wait "$pid" 2>/dev/null
        fi
    ) || return 1

    # The program with -b exits when the daemon reports the result of its
    # configuration, just before the daemon logs it. The components log
    # similar lines before it, thus match the whole line.
    for (( i = 0; i < ${SETUP_START_TIMEOUT:-30} * 20; i++ )); do
        line=$(tail -c +$(( offset + 1 )) "$rundir/$prog.log" 2>/dev/null |
               grep -asE "^------ $prog .*:[0-9]+ initialization (completed|failed)" || true)
        [[ "$line" == *completed* ]] && return 0
        [[ "$line" == *failed* ]] && return 1
        # the pid file exists but the daemon is gone
        if [[ -s "$rundir/$prog.pid" ]] && ! is_running "$(<"$rundir/$prog.pid")"; then
            return 1
        fi
        sleep 0.05
    done
    echo "$prog '$name' reports no configuration after ${SETUP_START_TIMEOUT:-30} s" >&2
    _setup_kill_daemon "$rundir" "$prog"
    return 1
}

# _setup_kill_daemon <rundir> <program>
#
# Stop the program and its daemon, which has no pid file yet. Each copy has the
# path of the pid file on its command line. The daemon of "xrootd -b" is a
# grandchild of the program, as XrdOucUtils::Undercover forks two times, and
# cmsd and xrootd of a server have one configuration file, but two pid files.
_setup_kill_daemon() {
    pkill -9 -f -- "-s $1/$2.pid" 2>/dev/null
    return 0
}

_setup_run_xrootd() {
    _setup_run xrootd "$@"
}

# _setup_snippet <snippet.cfg>
#
# Print the path of a snippet: an absolute path, a path with a slash relative
# to the directory of the test file, or a name of a file in the directory of
# the test file or in the setup directory, but not in both.
_setup_snippet() {
    local test=$BATS_TEST_DIRNAME/$1 setup=$SETUP_DIR/$1

    if [[ "$1" == /* ]]; then
        echo "$1"
    elif [[ "$1" == */* ]]; then
        echo "$test"
    elif [[ -f "$test" && -f "$setup" && "$BATS_TEST_DIRNAME" != "$SETUP_DIR" ]]; then
        echo "start_server: snippet '$1' is in the test and in the setup directory" >&2
        return 1
    elif [[ -f "$test" ]]; then
        echo "$test"
    else
        echo "$setup"
    fi
}

# start_server [name] [role] [snippet.cfg...]
#
# Start a server with the configuration base.cfg + <role>.cfg + snippets. The
# name and the role are "server" by default. The roles are server (a data
# server with unix authentication), server-nosec (a data server without the
# security library) and cache (a caching proxy in front of the server at
# $ORIGIN, see cache/cache.bash). An argument which ends in .cfg is a snippet
# (see _setup_snippet), for example http.cfg, tls.cfg or a file of the test.
# Export, with <NAME> the server name in upper case:
#
#   <NAME>_URL   root://localhost:<port>
#   <NAME>_URL4  root://127.0.0.1:<port>, for tests of the IPv4 stack
#   <NAME>_URL6  root://[::1]:<port>, for tests of the IPv6 stack
#   <NAME>_HTTP  http://localhost:<port>, with the snippet http.cfg
#   <NAME>_HTTPS https://localhost:<port>, with http.cfg and tls.cfg
#   <NAME>_PORT  the port of the server
#   <NAME>_DATA  the data directory of the server (oss.localroot)
#   <NAME>_LOG   the server log
start_server() {
    local name
    name=$(_setup_name "$@")
    _setup_compose "$@" || return 1
    _setup_run_xrootd "$name" any || return 1
    _setup_export "$name"
}

# _setup_name [name] [role] [snippet.cfg...]: print the name of the server
_setup_name() {
    local arg

    for arg in "$@"; do
        if [[ "$arg" != *.cfg ]]; then
            echo "$arg"
            return
        fi
    done
    echo server
}

# _setup_compose [name] [role] [snippet.cfg...]
#
# Make the run directory of the server and write its configuration.
_setup_compose() {
    local name=server role=server arg n=0
    local snippets=()
    for arg in "$@"; do
        if [[ "$arg" == *.cfg ]]; then
            snippets+=("$arg")
        elif (( n == 0 )); then
            name=$arg n=1
        elif (( n == 1 )); then
            role=$arg n=2
        else
            echo "start_server: unexpected argument '$arg'" >&2
            return 1
        fi
    done

    local rundir snippet
    rundir="$(_setup_tmpdir)/$name"

    # xrootd takes these instance names for no name, and then does not put
    # the name into the path of its log
    if [[ "$name" == anon || "$name" == default ]]; then
        echo "start_server: xrootd does not take '$name' as a server name" >&2
        return 1
    fi

    if [[ -e "$rundir" ]]; then
        echo "start_server: server '$name' exists already" >&2
        return 1
    fi

    mkdir -p "$rundir/data"
    _setup_admin_dir "$rundir" >/dev/null || return 1

    cat "$SETUP_DIR/base.cfg" "$SETUP_DIR/$role.cfg" > "$rundir/$name.cfg" || return 1

    for snippet in ${snippets[@]+"${snippets[@]}"}; do
        snippet=$(_setup_snippet "$snippet") || return 1
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

    stop_pid_file "$pidfile"
}

# restart_server <name>
#
# Stop the server, then start it again on the same port, with the same
# configuration and data.
restart_server() {
    local name=$1 rundir
    rundir=$(_setup_rundir "$name") || return 1

    stop_pid_file "$rundir/xrootd.pid" || return 1
    _setup_run_xrootd "$name" "$(<"$rundir/port")"
}

# start_issuer
#
# Start the token issuer "issuer", an HTTPS server which publishes the key of
# the shared credentials (issuer.jwks), and write the configuration of the
# snippet scitokens.cfg, which accepts the tokens of the issuer. Export ISSUER,
# the URL of the issuer, which make_token puts into the tokens.
start_issuer() {
    start_server issuer http.cfg tls.cfg || return 1

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

# start_collector <name> [port]
#
# Start a collector of monitoring data in the directory <name> of the current
# context: it receives UDP datagrams on <port>, or on a free port when <port>
# is omitted, and writes each one as a line of a file. Export <NAME>_PORT,
# the port, and <NAME>_RECORDS, the file; a snippet names the port with
# "set x = $<NAME>_PORT". stop_servers stops it.
start_collector() {
    local name=$1 port=${2:-0} dir var
    dir="$(_setup_tmpdir)/$name"
    var=$(_setup_var "$name")

    require_commands python3
    mkdir -p "$dir"
    touch "$dir/records"
    python3 "$SETUP_DIR/udp-collector.py" "$dir/port" "$dir/records" "$dir/collector.pid" "$port" || return 1

    for _ in {1..100}; do
        [[ -s "$dir/collector.pid" ]] && break
        sleep 0.05
    done
    if [[ ! -s "$dir/collector.pid" ]]; then
        echo "start_collector: collector '$name' wrote no pid file" >&2
        return 1
    fi

    export "${var}_PORT=$(<"$dir/port")"
    export "${var}_RECORDS=$dir/records"
}

# collected <records> <ere>: succeed when a record of the collector matches
# the extended regular expression
collected() {
    grep -qE "$2" "$1"
}

# gstream_count <records> <key>
#
# The sum over the servers of the count of the HTTP counter <key>, for example
# HTTP_GET_302, in the g-stream records. A server reports the total since its
# start, and the "sid" of the header of its records tells it apart.
gstream_count() {
    awk -v key="\"$2\":{\"count\":" '
        match($0, /"sid":[0-9]+/) && (i = index($0, key)) {
            sid = substr($0, RSTART + 6, RLENGTH - 6)
            n = substr($0, i + length(key)) + 0
            if (n > max[sid]) max[sid] = n
        }
        END { for (sid in max) total += max[sid]; print total + 0 }' "$1"
}

# gstream_base <records> <key>
#
# The count of <key> when it stays the same for one flush period of the
# g-stream (3 s in monitor.cfg). Thus a late report of an earlier request does
# not count for the request of the test.
gstream_base() {
    local last now
    now=$(gstream_count "$1" "$2")
    for _ in 1 2 3 4 5; do
        last=$now
        sleep 4
        now=$(gstream_count "$1" "$2")
        (( now == last )) && break
    done
    echo "$now"
}

# gstream_above <records> <key> <n>: succeed when the count of <key> is above <n>
gstream_above() {
    (( $(gstream_count "$1" "$2") > $3 ))
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
# Print the lines which the server logged during the current test.
server_log() {
    local log

    for log in "${BATS_TEST_TMPDIR:-/nonexistent}/$1/xrootd.log" "$BATS_FILE_TMPDIR/$1/xrootd.log"; do
        if [[ -f "$log" ]]; then
            cat "$log"
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
        stop_pid_file "$pidfile" || status=1
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

# On failure, print the client log of the test and the log of each server,
# which has only the lines of the test.
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
        sed "s|^|[$name] |" "$log"
        printf '\n'
    done
}
