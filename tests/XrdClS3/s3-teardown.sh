#!/bin/sh

TEST_NAME=$1

if [ -z "$BINARY_DIR" ]; then
  echo "\$BINARY_DIR environment variable is not set; cannot run test"
  exit 1
fi
if [ ! -d "$BINARY_DIR" ]; then
  echo "$BINARY_DIR is not a directory; cannot run test"
  exit 1
fi

echo "Tearing down $TEST_NAME"

if [ ! -f "$BINARY_DIR/tests/$TEST_NAME/setup.sh" ]; then
  echo "Test environment file $BINARY_DIR/tests/$TEST_NAME/setup.sh does not exist - cannot run test"
  exit 1
fi
. "$BINARY_DIR/tests/$TEST_NAME/setup.sh"


STATUS=0

# Stop one server and wait until it is gone. Every server gets a stop attempt
# no matter what happens to the others, so that none is ever left running.

stop() {
  NAME=$1
  PID=$2

  if [ -z "$PID" ]; then
    echo "PID of the $NAME process is not recorded; cannot tear it down"
    STATUS=1
    return
  fi

  if ! kill -0 "$PID" 2>/dev/null; then
    echo "$NAME process was already shut down by time the tear down was started"
    return
  fi

  kill "$PID"

  IDX=0
  while kill -0 "$PID" 2>/dev/null; do
    IDX=$((IDX+1))
    if [ $IDX -ge 10 ]; then
      echo "$NAME process did not exit within 10 seconds; killing it"
      kill -9 "$PID" 2>/dev/null
      break
    fi
    sleep 1
  done
}

stop minio "$MINIO_PID"
stop xrootd "$XROOTD_PID"

# The setup created the run directory with mktemp under /tmp; remove it here
# so that runs do not accumulate directories there.

if [ -n "$XROOTD_RUNDIR" ] && [ -d "$XROOTD_RUNDIR" ]; then
  rm -rf "$XROOTD_RUNDIR"
fi

exit $STATUS
