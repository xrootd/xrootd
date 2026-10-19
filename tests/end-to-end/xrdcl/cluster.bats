#!/usr/bin/env bash

# The tests of the XrdCl API in gtest/, against the cluster and the data of
# gtest/cluster.bash. Each test runs one test of xrdcl-cluster-tests. The
# tests share the cluster; some tests depend on the data which earlier tests
# leave, as they did with the old cluster fixture.

load gtest/cluster.bash

setup_file() {
    require_commands xrdcl-cluster-tests
    start_gtest_cluster
}

teardown_file() {
    stop_servers
}

setup() {
    cd "$BATS_TEST_TMPDIR"
}

# Run the test of xrdcl-cluster-tests with the name of the current test. A
# filter which matches no test passes, thus check that the one test ran.
# The tests use the default request and stream timeouts of the client, not
# the short ones of the setup: a vector read of 2 GiB can take longer than
# 15 s on a slow host.
gtest() {
    run -0 env -u XRD_REQUESTTIMEOUT -u XRD_STREAMTIMEOUT \
        xrdcl-cluster-tests --gtest_filter="$BATS_TEST_DESCRIPTION"
    assert_line --partial '[  PASSED  ] 1 test.'
}

@test "PostMasterTest.ThreadingTest" {
    gtest
}

@test "PostMasterTest.FunctionalTest" {
    gtest
}

@test "PostMasterTest.PingIPv6" {
    gtest
}

@test "ThreadingTest.ReadTest" {
    gtest
}

@test "ThreadingTest.MultiStreamReadTest" {
    gtest
}

@test "ThreadingTest.ReadForkTest" {
    gtest
}

@test "ThreadingTest.MultiStreamReadForkTest" {
    gtest
}

@test "ThreadingTest.MultiStreamReadMonitorTest" {
    gtest
}

@test "ThreadingTest.ReadForkTestWithTTLTimeout" {
    gtest
}

@test "ZipTest.ExtractTest" {
    gtest
}

@test "ZipTest.OpenFileTest" {
    gtest
}

@test "ZipTest.ListFileTest" {
    gtest
}

@test "ZipTest.GetterTests" {
    gtest
}

@test "FileTest.RedirectReturnTest" {
    gtest
}

@test "FileTest.ReadTest" {
    gtest
}

@test "FileTest.WriteTest" {
    gtest
}

@test "FileTest.WriteVTest" {
    gtest
}

@test "FileTest.VectorReadTest" {
    gtest
}

@test "FileTest.VectorWriteTest" {
    gtest
}

@test "FileTest.XAttrTest" {
    gtest
}

@test "FileTest.PlugInTest" {
    gtest
}

@test "FileTTLTest.TTLManyStreams" {
    gtest
}

@test "FileClone.FileCloneTest" {
    gtest
}

@test "FileCopyTest.UploadTest" {
    gtest
}

@test "FileCopyTest.MultiStreamUploadTest" {
    gtest
}

@test "FileCopyTest.DownloadTest" {
    gtest
}

@test "FileCopyTest.MultiStreamDownloadTest" {
    gtest
}

@test "FileCopyTest.ThirdPartyCopyTest" {
    gtest
}

@test "FileCopyTest.NormalCopyTest" {
    gtest
}

@test "FileSystemTest.LocateTest" {
    gtest
}

@test "FileSystemTest.MvTest" {
    gtest
}

@test "FileSystemTest.ServerQueryTest" {
    gtest
}

@test "FileSystemTest.TruncateRmTest" {
    gtest
}

@test "FileSystemTest.MkdirRmdirTest" {
    gtest
}

@test "FileSystemTest.ChmodTest" {
    gtest
}

@test "FileSystemTest.PingTest" {
    gtest
}

@test "FileSystemTest.StatTest" {
    gtest
}

@test "FileSystemTest.StatVFSTest" {
    gtest
}

@test "FileSystemTest.ProtocolTest" {
    gtest
}

@test "FileSystemTest.DeepLocateTest" {
    gtest
}

@test "FileSystemTest.DirListTest" {
    gtest
}

@test "FileSystemTest.SendInfoTest" {
    gtest
}

@test "FileSystemTest.PrepareTest" {
    gtest
}

@test "FileSystemTest.XAttrTest" {
    gtest
}

@test "FileSystemTest.PlugInTest" {
    gtest
}

@test "WorkflowTest.ReadingWorkflowTest" {
    gtest
}

@test "WorkflowTest.WritingWorkflowTest" {
    gtest
}

@test "WorkflowTest.MissingParameterTest" {
    gtest
}

@test "WorkflowTest.OperationFailureTest" {
    gtest
}

@test "WorkflowTest.DoubleRunningTest" {
    gtest
}

@test "WorkflowTest.ParallelTest" {
    gtest
}

@test "WorkflowTest.FileSystemWorkflowTest" {
    gtest
}

@test "WorkflowTest.MixedWorkflowTest" {
    gtest
}

@test "WorkflowTest.WorkflowWithFutureTest" {
    gtest
}

@test "WorkflowTest.XAttrWorkflowTest" {
    gtest
}

@test "WorkflowTest.MkDirAsyncTest" {
    gtest
}

@test "WorkflowTest.CheckpointTest" {
    gtest
}
