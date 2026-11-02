#!/usr/bin/env bash

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../helper/common.bash

readonly MIRAGE="localhost:11973"

teardown() {
    kill_pid_files
}

@test "stacked plugin without a path fails to initialize" {
    PORT=${MIRAGE##*:} run -1 launch_xrootd mirage_wrapper_nopath.cfg mirage

    assert_output --partial 'no mirage path given in the plugin parameters'
}

@test "stacked plugin with the root path fails to initialize" {
    PORT=${MIRAGE##*:} run -1 launch_xrootd mirage_wrapper_root.cfg mirage

    assert_output --partial "mirage path '/' is not allowed"
}

@test "stacked plugin with a valid path initializes" {
    PORT=${MIRAGE##*:} run -0 launch_xrootd mirage_wrapper.cfg mirage
}
