#!/usr/bin/env bash

# A cluster of a meta manager, two managers and four data servers: srv1 and
# srv2 below man1, srv3 and srv4 below man2. The tests share the cluster;
# each test has its own files. All nodes also serve HTTP.

load ../setup/cluster.bash

setup_file() {
    require_plugin libXrdHttp
    require_commands curl
    start_collector monitor
    start_meta_manager metaman http monitor
    start_manager man1 metaman http monitor
    start_manager man2 metaman http monitor
    start_data_server srv2 man1 http monitor
    # an open of cluster_redirfile2 on srv1 redirects to srv2
    export XRDOSSTESTS_REDIRECT_URL="root://localhost:$SRV2_PORT//cluster_redirfile3"
    start_data_server srv1 man1 http monitor
    start_data_server srv3 man2 http monitor
    start_data_server srv4 man2 http monitor
    wait_for_cluster metaman 4
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
    F="/t${BATS_TEST_NUMBER}.dat"
    make_random_file local.dat 100003
}

# The servers of a manager in the JSON map of xrdmapc, in any order
servers_of() {
    local manager=$1 a=$2 b=$3
    echo "\"name\":\"localhost:$manager\",\"type\":\"manager\",\"servers\":\\[(\\{\"name\":\"localhost:($a|$b)\"\\},?){2}\\]"
}

#
# The map of the cluster
#

@test "the map of the meta manager lists both managers" {
    run -0 xrdmapc --format json "localhost:$METAMAN_PORT"
    assert_output --regexp "\"name\":\"localhost:$METAMAN_PORT\",\"type\":\"manager\",\"managers\":\\[\\{\"name\":\"localhost:($MAN1_PORT|$MAN2_PORT)\""
}

@test "the map of the meta manager lists srv1 and srv2 below man1" {
    run -0 xrdmapc --format json "localhost:$METAMAN_PORT"
    assert_output --regexp "$(servers_of "$MAN1_PORT" "$SRV1_PORT" "$SRV2_PORT")"
}

@test "the map of the meta manager lists srv3 and srv4 below man2" {
    run -0 xrdmapc --format json "localhost:$METAMAN_PORT"
    assert_output --regexp "$(servers_of "$MAN2_PORT" "$SRV3_PORT" "$SRV4_PORT")"
}

@test "the text map of the meta manager lists the two managers" {
    run -0 xrdmapc "localhost:$METAMAN_PORT"
    assert_equal "$(grep -c ' Man localhost:' <<< "$output")" 2
}

#
# Queries
#

@test "query config role of the meta manager reports a meta manager" {
    run -0 xrdfs "$METAMAN_URL" query config role
    assert_output 'meta manager'
}

@test "query config role of a manager reports a manager" {
    run -0 xrdfs "$MAN1_URL" query config role
    assert_output 'manager'
}

@test "query config role of a data server reports a server" {
    run -0 xrdfs "$SRV1_URL" query config role
    assert_output 'server'
}

@test "query config sitename of a data server reports its name" {
    run -0 xrdfs "$SRV3_URL" query config sitename
    assert_output 'srv3'
}

@test "stat of / through the meta manager reports a directory" {
    run -0 xrdfs "$METAMAN_URL" stat /
    assert_line --regexp '^Flags: .*IsDir'
}

@test "statvfs through the meta manager succeeds" {
    run -0 xrdfs "$METAMAN_URL" statvfs /
}

@test "spaceinfo through the meta manager succeeds" {
    run -0 xrdfs "$METAMAN_URL" spaceinfo /
}

@test "ls through the meta manager lists a directory of one data server" {
    mkdir "$SRV4_DATA/d${BATS_TEST_NUMBER}"
    touch "$SRV4_DATA/d${BATS_TEST_NUMBER}/file"
    run -0 --separate-stderr xrdfs "$METAMAN_URL" ls "/d${BATS_TEST_NUMBER}"
    assert_output "/d${BATS_TEST_NUMBER}/file"
}

#
# Uploads and downloads
#

@test "a file uploaded through the meta manager downloads through it" {
    xrdcp local.dat "$METAMAN_URL/$F"
    xrdcp "$METAMAN_URL/$F" copy.dat
    run -0 cmp local.dat copy.dat
}

@test "an upload through a manager lands on one of its data servers" {
    xrdcp local.dat "$MAN1_URL/$F"
    run -0 test -e "$SRV1_DATA/$F" -o -e "$SRV2_DATA/$F"
}

@test "a file uploaded to a data server downloads through the meta manager" {
    xrdcp local.dat "$SRV3_URL/$F"
    xrdcp "$METAMAN_URL/$F" copy.dat
    run -0 cmp local.dat copy.dat
}

@test "query checksum crc32c through the meta manager returns the checksum of the file" {
    cp local.dat "$SRV4_DATA/$F"
    run -0 xrdfs "$METAMAN_URL" query checksum "$F?cks.type=crc32c"
    assert_output "crc32c $(checksum crc32c local.dat)"
}

@test "query checksum adler32 through the meta manager returns the checksum of the file" {
    cp local.dat "$SRV4_DATA/$F"
    run -0 xrdfs "$METAMAN_URL" query checksum "$F?cks.type=adler32"
    assert_output "adler32 $(checksum adler32 local.dat)"
}

@test "an HTTP PUT through the meta manager follows the redirect to a data server" {
    curl --silent --fail -L -T local.dat "$METAMAN_HTTP$F"
    xrdcp "$METAMAN_URL/$F" copy.dat
    run -0 cmp local.dat copy.dat
}

@test "an HTTP GET through the meta manager returns the file of a data server" {
    cp local.dat "$SRV3_DATA/$F"
    curl --silent --fail -L -o copy.dat "$METAMAN_HTTP$F"
    run -0 cmp local.dat copy.dat
}

@test "an HTTP PUT of a name with %23 stores a file with # in its name" {
    curl --silent --fail -L -T local.dat "$METAMAN_HTTP${F}_%23_http"
    xrdcp "$METAMAN_URL/${F}_#_http" copy.dat
    run -0 cmp local.dat copy.dat
}

@test "an HTTP MOVE through the meta manager is not implemented" {
    cp local.dat "$SRV1_DATA/$F"
    run -0 curl --silent -L -o /dev/null -w '%{http_code}' -X MOVE \
        -H "Destination: $METAMAN_HTTP$F.new" "$METAMAN_HTTP$F"
    assert_output 501
}

@test "an HTTP MOVE through a manager renames the file" {
    cp local.dat "$SRV1_DATA/$F"
    run -0 curl --silent -L -o /dev/null -w '%{http_code}' -X MOVE \
        -H "Destination: $MAN1_HTTP$F.new" "$MAN1_HTTP$F"
    assert_output 201
}

@test "an HTTP MOVE on a data server renames the file" {
    cp local.dat "$SRV1_DATA/$F"
    run -0 curl --silent -L -o /dev/null -w '%{http_code}' -X MOVE \
        -H "Destination: $SRV1_HTTP$F.new" "$SRV1_HTTP$F"
    assert_output 201
}

#
# Redirects of the data servers to a URL (see XrdOssTests)
#

@test "a read of a file which a data server redirects to a missing local file fails with ENOENT" {
    cp local.dat "$SRV1_DATA/cluster_redirfile1"
    run -54 xrdcp -f "$SRV1_URL//cluster_redirfile1" /dev/null
    assert_output --partial '[ERROR] Local error: no such file or directory'
}

@test "a read through the meta manager of a file which redirects to a missing local file retries at the meta manager" {
    cp local.dat "$SRV1_DATA/cluster_redirfile1"
    XRD_LOGLEVEL=Dump run ! xrdcp -f "$METAMAN_URL//cluster_redirfile1" /dev/null
    run -0 grep -F ":$METAMAN_PORT] Got notification that outgoing message kXR_open (file: /cluster_redirfile1?tried=" "$XRD_LOGFILE"
}

@test "a read through the meta manager of a file which redirects to a missing file of another server retries at the meta manager" {
    cp local.dat "$SRV1_DATA/cluster_redirfile2"
    XRD_LOGLEVEL=Dump run ! xrdcp -f "$METAMAN_URL//cluster_redirfile2" /dev/null
    run -0 grep -F ":$METAMAN_PORT] Got notification that outgoing message kXR_open (file: /cluster_redirfile2?tried=" "$XRD_LOGFILE"
}

#
# Monitoring of the nodes
#

# Succeed when a record of the collector matches the extended regular
# expression
recorded() {
    grep -qE "$1" "$MONITOR_RECORDS"
}

@test "the g-stream of a redirector counts the redirect of an HTTP GET" {
    cp local.dat "$SRV3_DATA/$F"
    curl --silent --fail -L -o copy.dat "$METAMAN_HTTP$F"
    run -0 wait_for 10 recorded '"HTTP_GET_302":\{"count":[1-9]'
}

@test "the g-stream of a redirector counts the redirect of an HTTP PUT" {
    curl --silent --fail -L -T local.dat "$MAN2_HTTP$F"
    run -0 wait_for 10 recorded '"HTTP_PUT_307":\{"count":[1-9]'
}

@test "the summary report of a node has the statistics of the HTTP plugin" {
    run -0 wait_for 10 recorded '"site":"srv1",.*"stats_http_plugin"'
}

# The node listens on "xrd.port any", thus it knows the port only after the
# network binds it (fixed in Xrd)
@test "the summary report of a node names the port which it listens on" {
    run -0 wait_for 10 recorded "\"src\":\"[^\"]*:$SRV1_PORT\",[^}]*\"site\":\"srv1\""
}
