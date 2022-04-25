TEST_CASE_NAME="Download files from an HTTP source"

test_init() {
    local string_length=1024
    local num_files=5

    mkdir -p $WORKSPACE/in
    mkdir -p $WORKSPACE/out

    # Generate some input files
    for i in $(seq 1 $num_files); do
        local s=$(generate_random_string $string_length)
        local h=$(str_sha1 $s)
        echo $s > $WORKSPACE/in/$h
    done

    start_caddy $WORKSPACE/in $WORKSPACE/config/caddyfile
}

test_main() {
    for f in $(ls $WORKSPACE/in/) ; do
        echo "Downloading: $WORKSPACE/in/$f"
        #XRD_LOGLEVEL=Debug \
        xrdcp -A -f --silent http://localhost:8080/$f $WORKSPACE/out/
        local sha1_out=$(file_sha1 $WORKSPACE/out/$f)
        if [ x"$sha1_out" != x"$f" ]; then
            echo "Error: incorrect transfer of file: $WORKSPACE/in/$f"
            echo "  SHA1  (in): $f"
            echo "  SHA1 (out): $sha1_out"
            exit 1
        fi
    done
}

test_finalize() {
    stop_caddy
}
