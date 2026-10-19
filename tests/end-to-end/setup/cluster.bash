# Cluster setups: managers and data servers, each an xrootd with a cmsd.
#
# A test file loads this file in place of server.bash. Each node is a server
# of the role server (see start_server) with the snippet cluster and the
# snippet of its place in the cluster. The cmsd of a manager listens on a
# free port, which the nodes below it connect to. Export, with <NAME> the
# node name in upper case, the variables of start_server and, for a manager,
# <NAME>_CMSPORT.

load "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/server.bash"

# _cluster_start <name> <cluster snippet> [snippet...]
#
# Start the cmsd, then the xrootd, of a node. The cmsd starts first, as the
# xrootd connects to it. Both log through a fifo (-k fifo), which a log
# rotation can use.
_cluster_start() {
    local name=$1 text=$2 tmp
    shift 2
    tmp=$(_setup_tmpdir)

    printf '%s\n' "# Snippet: the place of $name in the cluster" "$text" > "$tmp/$name-cluster.cfg"
    _setup_compose "$name" server cluster "$tmp/$name-cluster.cfg" "$@" || return 1
    _setup_run cmsd "$name" any -k fifo || return 1
    _setup_run xrootd "$name" any -k fifo || return 1
    _setup_export "$name"
}

# start_meta_manager <name> [snippet...]
#
# Start a meta manager, the top of a cluster of managers.
start_meta_manager() {
    local name=$1 cms
    shift
    require_commands python3
    cms=$(free_port) || return 1
    export "$(_setup_var "$name")_CMSPORT=$cms"

    _cluster_start "$name" "all.role meta manager
all.manager meta localhost:$cms
xrd.port $cms if exec cmsd" "$@"
}

# start_manager <name> <meta manager> [snippet...]
#
# Start a manager of data servers. With a meta manager (a name, or "" for
# none), the manager subscribes to it.
start_manager() {
    local name=$1 meta=$2 cms text
    shift 2
    require_commands python3
    cms=$(free_port) || return 1
    export "$(_setup_var "$name")_CMSPORT=$cms"

    text="all.role manager
all.manager localhost:$cms
xrd.port $cms if exec cmsd"
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
