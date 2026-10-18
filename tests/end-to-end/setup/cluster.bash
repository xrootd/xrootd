# Cluster setups: managers and data servers, each an xrootd with a cmsd.
#
# A test file loads this file in place of server.bash. Each node is a server
# of the role server (see start_server) with the snippet cluster.cfg and the
# snippet of its place in the cluster. The cmsd of a node listens on a random
# port, as an xrootd does, and the nodes below a manager connect to the port
# of its cmsd. Export, with <NAME> the node name in upper case, the variables
# of start_server and <NAME>_CMSPORT, the port of the cmsd.

load "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/server.bash"

# _cluster_start <name> <cluster snippet> [snippet...]
#
# Start the cmsd, then the xrootd, of a node. The cmsd starts first, as the
# xrootd connects to it. The cmsd gets a random port, with a new try when
# another process has the port, and the xrootd gets the port of the cmsd in
# <NAME>_CMSPORT.
_cluster_start() {
    local name=$1 text=$2 tmp
    shift 2
    tmp=$(_setup_tmpdir)

    printf '%s\n' "# Snippet: the place of $name in the cluster" "$text" > "$tmp/$name-cluster.cfg"
    _setup_compose "$name" cluster.cfg "$tmp/$name-cluster.cfg" "$@" || return 1
    _setup_run cmsd "$name" any || return 1
    export "$(_setup_var "$name")_CMSPORT=$(<"$(_setup_rundir "$name")/cmsd.port")"
    _setup_run xrootd "$name" any || return 1
    _setup_export "$name"
}

# _cluster_cmsport <name>
#
# Print the lines of a snippet which set cmsport to the port of the cmsd of
# the node: for the cmsd, its own port, and for the xrootd, <NAME>_CMSPORT.
# A set takes environment variables, thus SETUP_PORT and not $port.
_cluster_cmsport() {
    # shellcheck disable=SC2016 # the configuration expands the variable
    printf '%s\n' 'if exec cmsd' 'set cmsport = $SETUP_PORT' 'else' \
        "set cmsport = \$$(_setup_var "$1")_CMSPORT" 'fi'
}

# start_meta_manager <name> [snippet...]
#
# Start a meta manager, the top of a cluster of managers.
start_meta_manager() {
    local name=$1
    shift

    _cluster_start "$name" "all.role meta manager
$(_cluster_cmsport "$name")
all.manager meta localhost:\$cmsport" "$@"
}

# start_manager <name> <meta manager> [snippet...]
#
# Start a manager of data servers. With a meta manager (a name, or "" for
# none), the manager subscribes to it.
start_manager() {
    local name=$1 meta=$2 text
    shift 2

    text="all.role manager
$(_cluster_cmsport "$name")
all.manager localhost:\$cmsport"
    if [[ -n "$meta" ]]; then
        local var
        var="$(_setup_var "$meta")_CMSPORT"
        text+="
all.manager meta localhost:${!var}"
    fi

    _cluster_start "$name" "$text" "$@"
}

# start_data_server <name> <manager> [snippet...]
#
# Start a data server, which subscribes to the manager.
start_data_server() {
    local name=$1 manager=$2 var
    shift 2
    var="$(_setup_var "$manager")_CMSPORT"

    _cluster_start "$name" "all.role server
all.manager localhost:${!var}" "$@"
}

# cluster_servers <manager>
#
# Print the number of data servers which the manager knows, as the cluster
# map of xrdmapc shows them.
cluster_servers() {
    local var
    var="$(_setup_var "$1")_PORT"
    xrdmapc "localhost:${!var}" 2>/dev/null | grep -c ' Srv '
}

# wait_for_cluster <manager> <servers>
#
# Wait up to 30 s until the manager knows the number of data servers. On
# timeout, print the map of the cluster and the logs of all cmsd.
wait_for_cluster() {
    local manager=$1 servers=$2 var log
    var="$(_setup_var "$manager")_PORT"

    wait_for 30 _cluster_has "$manager" "$servers" && return

    echo "the cluster below $manager did not form" >&2
    xrdmapc "localhost:${!var}" 2>/dev/null | sed "s/^/[map] /" >&2
    for log in "$(_setup_tmpdir)"/*/cmsd.log; do
        sed "s|^|[$(basename "$(dirname "$log")") cmsd] |" "$log" >&2
    done
    return 1
}

_cluster_has() {
    [[ "$(cluster_servers "$1")" == "$2" ]]
}
