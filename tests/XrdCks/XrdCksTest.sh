target="xroot://sdfiana028.sdf.slac.stanford.edu//tmp/TestFile"

# The first argument designates the type of test being run and should
# be either "demand" or "r/t". We set a default here.
test="${1:-unknown}"

# When the test is for demand checksums, the server should be started
# with the XrdCksTestOD.cf config file.

# When the test is for r/t checksums, the server should be started
# with the XrdCksTestRT.cf config file.

# Global variable to track the overall test suite success
SUITE_RC=0

# Subroutine for handling return codes
handle_rc() {
    local cmd_rc=$1
    local test_name=$2

    if [ "$cmd_rc" -ne 0 ]; then
        echo "[FAIL] XrdCksTest $test_name exited with code $cmd_rc"

        # Action: Update global tracker
        SUITE_RC=$cmd_rc
    else
        echo "[PASS] XrdCksTest.sh $test_name completed successfully."
    fi
}

# Run all of the tests
./xrdckstest $test adler32 64fce8a2 XrdCksTestFile $target
handle_rc $? "$test adler32"

./xrdckstest $test crc32   55bd9dea XrdCksTestFile $target
handle_rc $? "$test crc32"

./xrdckstest $test crc32c  cf3aa257 XrdCksTestFile $target
handle_rc $? "$test crc32c"

./xrdckstest $test md5 b7018d4b11d10edbdba4a240e71d4976 XrdCksTestFile $target
handle_rc $? "$test md5"

# All done, exit with the 0 or an error code
#
exit $SUITE_RC
