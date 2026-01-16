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


if [ -z "$ORIGIN_PID" ]; then
  echo "\$ORIGIN_PID environment variable is not set; cannot tear down process"
  exit 1
fi

if ! kill -0 "$ORIGIN_PID" 2>/dev/null; then
  echo "Origin process was already shut down by time the tear down was started"
  exit
else
  kill "$ORIGIN_PID"
fi

if [ -z "$CACHE_PID" ]; then
  echo "\$CACHE_PID environment variable is not set; cannot tear down process"
  exit 1
fi

if ! kill -0 "$CACHE_PID" 2>/dev/null; then
  echo "Cache process was already shut down by time the tear down was started"
  exit
else 
  kill "$CACHE_PID"
fi

