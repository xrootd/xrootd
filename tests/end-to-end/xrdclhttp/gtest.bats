#!/usr/bin/env bash

# The gtests of XrdClHttp in gtest/, against the HTTPS origin and cache of
# xrdclhttp.bash, which they find in the settings file $ENV_FILE. Each test
# runs one test of xrdcl-test, which uses only the interfaces of XrdCl, or of
# xrdcl-http-test, which links XrdClHttp. The tests share the servers.

load xrdclhttp.bash

# The tests use only HTTPS, where XrdClHttp would present a grid proxy of the
# user, which the test CA cannot verify
export X509_USER_PROXY=/dev/null/x509

setup_file() {
    require_commands xrdcl-test xrdcl-http-test
    start_https_cache
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
}

# Run the test of the program with the name of the current test. A filter
# which matches no test passes, thus check that the one test ran.
gtest() {
    run -0 "$1" --gtest_filter="$BATS_TEST_DESCRIPTION"
    assert_line --partial '[  PASSED  ] 1 test.'
}

@test "CurlChecksumFixture.Basic" {
    gtest xrdcl-test
}

@test "CurlDeleteFixture.Test" {
    gtest xrdcl-test
}

@test "CurlMkcolFixture.Test" {
    gtest xrdcl-test
}

@test "CurlMkcolFixture.MkpathTest" {
    gtest xrdcl-test
}

@test "CurlReadFixture.SerialTest" {
    gtest xrdcl-test
}

@test "CurlReadFixture.PrefetchTimeoutTest" {
    gtest xrdcl-test
}

@test "CurlReadFixture.ConcurrentTest" {
    gtest xrdcl-test
}

@test "CurlReadFixture.FullDownloadUnknownSizePrefetchTest" {
    gtest xrdcl-test
}

@test "CurlWriteFixture.SerialTest" {
    gtest xrdcl-test
}

@test "CurlWriteFixture.ParallelTest" {
    gtest xrdcl-test
}

@test "CurlWriteFixture.SlowClientWriteTest" {
    gtest xrdcl-test
}

@test "CurlWriteFixture.QueuedWriteTest" {
    gtest xrdcl-test
}

@test "CurlWriteFixture.PutTimeoutTest" {
    gtest xrdcl-test
}

@test "CurlCopyFixture.Test" {
    gtest xrdcl-http-test
}

@test "CurlVectorFixture.Test" {
    gtest xrdcl-http-test
}

@test "CurlVectorFixture.WriteTest" {
    gtest xrdcl-http-test
}

@test "CurlVectorFixture.WriteTestSplitMultipartHeaderAcrossWrites" {
    gtest xrdcl-http-test
}

@test "CurlVectorFixture.WriteTestSplitAfterContentRangeBeforeBody" {
    gtest xrdcl-http-test
}

@test "CurlVectorFixture.WriteTestSplitCRLFAfterBoundary" {
    gtest xrdcl-http-test
}

@test "CurlVectorFixture.WriteTestOutOfOrderChunks" {
    gtest xrdcl-http-test
}

@test "CurlVectorFixture.WriteTestExtraCRLFBeforeBoundary" {
    gtest xrdcl-http-test
}

@test "CurlVectorFixture.WriteTestSingleRange206" {
    gtest xrdcl-http-test
}

@test "CurlVectorFixture.WriteTestCoalescedRange" {
    gtest xrdcl-http-test
}

@test "CurlVectorFixture.WriteTestBinaryPayloadLooksLikeFraming" {
    gtest xrdcl-http-test
}
