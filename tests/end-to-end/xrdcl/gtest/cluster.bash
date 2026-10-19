# The cluster and the data of the XrdCl tests in gtest/, with the layout of
# the old cluster fixture: a meta manager, man1 with srv1 and srv2, man2 with
# srv3 and srv4. The tests read the settings of TestSetup.hh from the
# XRDTEST_* variables which start_gtest_cluster exports.

load ../setup/cluster.bash

GTEST_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# The test files of 16 MiB of random data, each with the data servers which
# have a copy
GTEST_COPIES=(
    "1db882c8-8cd6-4df1-941f-ce669bad3458.dat srv2 srv3 srv4"
    "3c9a9dd8-bc75-422c-b12c-f00604486cc1.dat srv2 srv3"
    "7235b5d1-cede-4700-a8f9-596506b4cc38.dat srv2"
    "7e480547-fe1a-4eaf-a210-0f3927751a43.dat srv2 srv4"
    "89120cec-5244-444c-9313-703e4bee72de.dat srv2 srv3 srv4"
    "a048e67f-4397-4bb8-85eb-8d7e40d90763.dat srv1"
    "b3d40b3f-1d15-4ad3-8cb5-a7516acb2bab.dat srv1"
    "b74d025e-06d6-43e8-91e1-a862feb03c84.dat srv1 srv3 srv4"
    "cb4aacf1-6f28-42f2-b68a-90a73460f424.dat srv1"
    "cef4d954-936f-4945-ae49-60ec715b986e.dat srv1 srv3 srv4"
)

# start_gtest_cluster
#
# Start the cluster, make the test data, and export the settings.
start_gtest_cluster() {
    start_meta_manager metaman gtest/node.cfg
    start_manager man1 metaman gtest/node.cfg
    start_manager man2 metaman gtest/node.cfg
    start_data_server srv1 man1 gtest/node.cfg
    start_data_server srv2 man1 gtest/node.cfg
    start_data_server srv3 man2 gtest/node.cfg
    start_data_server srv4 man2 gtest/node.cfg
    wait_for_cluster metaman 4 || return 1
    make_gtest_data || return 1

    export XRDTEST_MAINSERVERURL="localhost:$METAMAN_PORT"
    export XRDTEST_DISKSERVERURL="localhost:$METAMAN_PORT"
    export XRDTEST_MANAGER1URL="localhost:$MAN1_PORT"
    export XRDTEST_MANAGER2URL="localhost:$MAN2_PORT"
    export XRDTEST_SERVER1URL="localhost:$SRV1_PORT"
    export XRDTEST_SERVER2URL="localhost:$SRV2_PORT"
    export XRDTEST_SERVER3URL="localhost:$SRV3_PORT"
    export XRDTEST_SERVER4URL="localhost:$SRV4_PORT"
    export XRDTEST_DATAPATH=/data
    export XRDTEST_REMOTEFILE=/data/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat
    export XRDTEST_LOCALFILE=/data/testFile.dat
    # the tests find the local root of a node at <LocalDataPath>/<node>
    export XRDTEST_LOCALDATAPATH="$BATS_FILE_TMPDIR/local"
}

# make_gtest_data
#
# Make the files of the tests below /data of the nodes.
make_gtest_data() {
    local tmp=$BATS_FILE_TMPDIR rand=$BATS_FILE_TMPDIR/random
    local entry file nodes node ml i

    mkdir -p "$rand" "$tmp/local"
    for node in metaman man1 man2 srv1 srv2 srv3 srv4; do
        mkdir -p "$tmp/$node/data/data"
        ln -s "$tmp/$node/data" "$tmp/local/$node"
    done

    for entry in "${GTEST_COPIES[@]}"; do
        read -r file nodes <<< "$entry"
        openssl rand -out "$rand/$file" $(( 1 << 24 ))
        for node in $nodes; do
            cp "$rand/$file" "$tmp/$node/data/data/$file"
        done
    done

    cp "$rand/a048e67f-4397-4bb8-85eb-8d7e40d90763.dat" "$tmp/metaman/data/data/testFile.dat"
    openssl rand -out "$tmp/srv1/data/data/2GB.dat" $(( (1 << 31) - 1 ))

    for node in srv1 srv2 srv3 srv4; do
        mkdir "$tmp/$node/data/data/bigdir"
        for i in {1..1000}; do
            : > "$tmp/$node/data/data/bigdir/$node-$i.dat"
        done
    done

    cp "$GTEST_DIR/data/data.zip" "$GTEST_DIR/data/large.zip" "$tmp/srv4/data/data/"

    mkdir "$tmp/srv1/data/data/metalink"
    for ml in "$GTEST_DIR"/data/*.meta4; do
        _gtest_metalink "$ml" "$rand" > "$tmp/srv1/data/data/metalink/$(basename "$ml")"
    done

    rm -rf "$rand"
}

# _gtest_metalink <template> <dir of the random files>
#
# Print the metalink with the ports of the nodes, and with the size and the
# crc32c checksum of the test file which it names.
_gtest_metalink() {
    local ml=$1 rand=$2 entry file script

    script="s/localhost:10940/localhost:$METAMAN_PORT/g
s/localhost:10941/localhost:$MAN1_PORT/g
s/localhost:10942/localhost:$MAN2_PORT/g
s/localhost:10943/localhost:$SRV1_PORT/g
s/localhost:10944/localhost:$SRV2_PORT/g
s/localhost:10945/localhost:$SRV3_PORT/g
s/localhost:10946/localhost:$SRV4_PORT/g"

    for entry in "${GTEST_COPIES[@]}"; do
        file=${entry%% *}
        grep -q "$file" "$ml" || continue
        script+="
s|<size>.*</size>|<size>$(wc -c < "$rand/$file" | tr -d ' ')</size>|
s|<hash type=\"crc32c\">.*</hash>|<hash type=\"crc32c\">$(checksum crc32c "$rand/$file")</hash>|"
    done

    sed "$script" "$ml"
}
