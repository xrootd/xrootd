TEST_CASE_NAME="Upload files to an HTTP destination"

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

    start_caddy $WORKSPACE/out $WORKSPACE/config/caddyfile
}

test_main() {
    for f in $(ls $WORKSPACE/in/) ; do
        echo "Downloading: $WORKSPACE/in/$f"
        #XRD_LOGLEVEL=Debug \
        xrdcp -A -f --silent $WORKSPACE/in/$f http://localhost:8080/$f
        local retrieve=$(xrdcp -A -f --silent http://localhost:8080/$f -)
        local sha1_out=$(str_sha1 $retrieve)
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
