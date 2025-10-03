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


if [ -z "$MINIO_PID" ]; then
  echo "\$MINIO_PID environment variable is not set; cannot tear down process"
  exit 1
fi

kill "$MINIO_PID"

if [ ! -z "$XROOTD_PID" ]; then
  kill "$XROOTD_PID" || (tail -n 1000 $BINARY_DIR/tests/$TEST_NAME/server.log && exit 1)
fi
