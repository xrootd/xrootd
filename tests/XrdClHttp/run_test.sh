#!/bin/sh

set -e

# Integration script for the XrdClHttp plugin

CADDY_URL=http://ecsft.cern.ch:80/dist/cvmfs/builddeps/caddy-linux-amd64
CADDY_SHA1=0f3d1ea280ec744805cd557f123ec0d785bb1da0


SCRIPT_LOCATION=$(cd "$(dirname "$0")"; pwd)
. ${SCRIPT_LOCATION}/common.sh

TEST_CASE_PATTERN="*"
if [ x"$1" != x"" ]; then
    TEST_CASE_PATTERN=$1
fi

# Additional commands
check_executable wget

# Preconditions (XRootD and XrdClHttp in $XROOTD_PREFIX)
check_prefix $XROOTD_PREFIX
PATH=$XROOTD_PREFIX/bin:$PATH
LD_LIBRARY_PATH=$XROOTD_PREFIX/lib:$LD_LIBRARY_PATH

# Download the caddy HTTP server, if needed
CADDY_EXEC=/tmp/caddy
if [ ! -f $CADDY_EXEC ] || [ x"$(file_sha1 $CADDY_EXEC)" != x"$CADDY_SHA1" ]; then
    echo "Downloading: $CADDY_URL"
    wget -q -O $CADDY_EXEC $CADDY_URL
    chmod +x $CADDY_EXEC
fi

# Run all the test cases
set +e
TEST_CASES=$(ls -d $SCRIPT_LOCATION/cases/$TEST_CASE_PATTERN)
for c in $TEST_CASES ; do
    # Set up the workspace for the test run
    WORKSPACE=$(create_workspace)

    cd $WORKSPACE

    # Copy all test config files into the workspace
    cp -r $SCRIPT_LOCATION/config $WORKSPACE/

    # Configure the Caddyfile template with the value of $WORKSPACE
    sed -i -e "s:<<CADDY_UPLOAD_DIR>>:$WORKSPACE/out:g" $WORKSPACE/config/caddyfile

    # Set up the XRootD client HTTP plugin configuration
    sed -i -e "s:<<XROOTD_PREFIX>>:$XROOTD_PREFIX:g" $WORKSPACE/config/client/http.conf
    XRD_PLUGINCONFDIR=$WORKSPACE/config/client

    run_test_case $c &
    wait $!

    clean_up

    cd $SCRIPT_LOCATION
done

echo "Finished."
