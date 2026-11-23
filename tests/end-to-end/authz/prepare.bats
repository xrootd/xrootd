#!/usr/bin/env bash

load ../setup/server.bash

setup_file() {
    export XRD_TEST_PREPARE_PROGRAM
    XRD_TEST_PREPARE_PROGRAM=$(type -P true)
    start_server prepare.cfg
}

teardown_file() {
    stop_servers
}

@test "prepare accepts credentials for both paths in either order" {
    run -0 xrdfs "$SERVER_URL" prepare -s '/a?authz=alpha' '/b?authz=beta'
    run -0 xrdfs "$SERVER_URL" prepare -s '/b?authz=beta' '/a?authz=alpha'
}

@test "prepare checks each credential against its own path" {
    run ! xrdfs "$SERVER_URL" prepare -s '/a?authz=beta' '/b?authz=alpha'
    run ! xrdfs "$SERVER_URL" prepare -s '/a?authz=alpha' '/b?authz=alpha'
}

@test "prepare requires a credential for each path" {
    run ! xrdfs "$SERVER_URL" prepare -s '/a?authz=alpha' /b
    run ! xrdfs "$SERVER_URL" prepare -s /a '/b?authz=beta'
}

@test "prepare clears the mapped name before checking the next path" {
    run ! xrdfs "$SERVER_URL" prepare -s '/a?authz=alpha' /inherited
}
