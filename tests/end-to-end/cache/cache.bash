# Helpers of the cache tests: an origin, a cache in front of it, and a
# collector of the g-stream of the cache.
#
# Two sources tell what the cache did:
#
#   * The file_close record of the g-stream. Its b_todisk is the number of
#     bytes which came from the origin for one open of the file.
#   * The .cinfo file, through xrdpfc_print. It tells which blocks are on
#     disk, if the file is complete, and the history of the accesses.
#
# Do not use B_hit and B_miss to tell if bytes came from the origin. XrdPfc
# counts a miss only for the request which creates a block. Thus a second
# request for a block in flight counts as a hit (see issue #2366).

load ../setup/server.bash

readonly BLOCK=$((128 * 1024))

# start_cache [snippet...]: start the origin, the collector and the cache
start_cache() {
    require_commands xrdpfc_print xrdpfc-read
    start_server origin server
    export ORIGIN="localhost:$ORIGIN_PORT"
    start_collector gstream
    start_server cache cache gstream.cfg "$@"
}

# make_origin_file <lfn> <blocks>: a random file of whole blocks on the origin
make_origin_file() {
    mkdir -p "$(dirname "$ORIGIN_DATA/$1")"
    dd if=/dev/urandom of="$ORIGIN_DATA/$1" bs="$BLOCK" count="$2" status=none
}

# The bytes of the origin file <lfn> in the range <offset>:<length>
origin_range() {
    local range=$1
    tail -c +$(( ${range%%:*} + 1 )) "$ORIGIN_DATA/$2" | head -c "${range##*:}"
}

# The file_close records of <lfn>, one in each line
close_records() {
    grep -o '{"event":"file_close"[^}]*}' "$GSTREAM_RECORDS" | grep -F "\"lfn\":\"/$1\""
}

# close_field <lfn> <access> <field>: the value of <field> in the file_close
# record of the access number <access> to <lfn>. The cache sends the record
# when it closes the file, after the client, and the g-stream flushes once
# each second; thus this waits up to 20 s for the record.
close_field() {
    wait_for_close "$1" "$2" || return 1
    _close_record "$1" "$2" | sed -n "s/.*\"$3\":\([0-9]*\).*/\1/p"
}

# wait_for_close <lfn> <access>: wait up to 20 s for the file_close record of
# the access number <access> to <lfn>. A test waits for it before the next
# read, else the next read can use the same open of the cache.
wait_for_close() {
    wait_for 20 _close_record "$1" "$2" > /dev/null || {
        echo "no file_close record of /$1 for access $2" >&2
        return 1
    }
}

_close_record() {
    close_records "$1" | grep -F "\"access_cnt\":$2," | tail -1 | grep .
}

# The sum of b_todisk in all file_close records of <lfn>: all bytes which the
# origin sent for it. It waits for the first record, then 3 s for the others.
fetched_total() {
    wait_for 20 close_records "$1" > /dev/null || return 1
    sleep 3
    close_records "$1" | sed -n 's/.*"b_todisk":\([0-9]*\).*/\1/p' | awk '{ s += $1 } END { print s + 0 }'
}

# The path of the .cinfo file of <lfn> in the cache
cinfo() {
    echo "$CACHE_DATA/$1.cinfo"
}

# "<n_blocks> <n_downloaded> <state>" of <lfn> in the cache
cinfo_blocks() {
    xrdpfc_print -u B "$(cinfo "$1")" |
        sed -n 's/^file_size .*n_blocks \([0-9]*\), n_downloaded \([0-9]*\), state \([a-z]*\).*/\1 \2 \3/p'
}

# The number of access records of <lfn>
cinfo_accesses() {
    xrdpfc_print -u B "$(cinfo "$1")" | sed -n 's/^Access records (N_acc_total=\([0-9]*\)).*/\1/p'
}

# "<B_hit> <B_miss> <B_bypass>" of the access record <n> of <lfn>, in bytes
cinfo_access() {
    xrdpfc_print -u B "$(cinfo "$1")" | awk -v rec="$2" '$1 == rec { print $(NF-2), $(NF-1), $NF; exit }'
}

# The blocks of <lfn> in the cache, one character for each: x is on disk
cinfo_block_map() {
    xrdpfc_print -u B -v "$(cinfo "$1")" |
        sed -n '/^printing /,/^Access records/p' |
        sed -n 's/^ *[0-9]\{1,\} \([x.]\{1,\}\)$/\1/p' | tr -d '\n'
}

# wait_for_accesses <lfn> <n>: wait up to 20 s for <n> access records
wait_for_accesses() {
    wait_for 20 _has_accesses "$1" "$2"
}

_has_accesses() {
    [[ -f "$(cinfo "$1")" && "$(cinfo_accesses "$1")" == "$2" ]]
}

# wait_for_complete <lfn>: wait up to 20 s until all blocks are on disk
wait_for_complete() {
    wait_for 20 _is_complete "$1"
}

_is_complete() {
    local total stored state
    [[ -f "$(cinfo "$1")" ]] || return 1
    read -r total stored state <<< "$(cinfo_blocks "$1")"
    [[ "$state" == complete && "$total" == "$stored" ]]
}
