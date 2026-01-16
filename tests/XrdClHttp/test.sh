#!/bin/bash

TEST_NAME=$1

function error() {
        echo "error: $*" >&2; exit 1;
}

function assert() {
        echo "$@"; "$@" || error "command \"$*\" failed";
}

function assert_eq() {
  [[ "$1" == "$2" ]] || error "$3: expected $1 but received $2"
}

if [ -z "$BINARY_DIR" ]; then
  echo "\$BINARY_DIR environment variable is not set; cannot run test"
  exit 1
fi
if [ ! -d "$BINARY_DIR" ]; then
  echo "$BINARY_DIR is not a directory; cannot run test"
  exit 1
fi

CURL_BIN=$(command -v curl)
if [ -z "$CURL_BIN" ]; then
  echo "curl is not installed; required for test"
  exit 1
fi

XRDFS_BIN="$BINARY_DIR/bin/xrdfs"
if [ -z "$XRDFS_BIN" ]; then
  echo "$XRDFS_BIN is not present; cannot run test"
  exit 1
fi

echo "Running $TEST_NAME - simple download"

if [ ! -f "$BINARY_DIR/tests/$TEST_NAME/setup.sh" ]; then
  echo "Test environment file $BINARY_DIR/tests/$TEST_NAME/setup.sh does not exist - cannot run test"
  exit 1
fi
. "$BINARY_DIR/tests/$TEST_NAME/setup.sh"

echo "Running $TEST_NAME - simple download"

CONTENTS=$(curl --cacert "$X509_CA_FILE" -v -L --fail -H "@$HEADER_FILE" "$CACHE_URL/test/hello_world.txt" 2>> "$BINARY_DIR/tests/$TEST_NAME/client.log")
CURL_EXIT=$?

if [ $CURL_EXIT -ne 0 ]; then
  cat "$BINARY_DIR/tests/$TEST_NAME/cache.log"
  cat "$BINARY_DIR/tests/$TEST_NAME/client.log"
  echo "Download of hello-world text failed"
  exit 1
fi

if [ "$CONTENTS" != "Hello, World" ]; then
  echo "Downloaded hello-world text is incorrect: $CONTENTS"
  exit 1
fi

########################################################################
## The slow-read and checksum tests currently require pelican patches ##
## to function.  For now, disable.                                    ##
########################################################################
if false; then

echo "Running $TEST_NAME - slow open"

HTTP_CODE=$(curl -m 4 --cacert "$X509_CA_FILE" -v -L --write-out '%{http_code}' -H "@$HEADER_FILE" "$CACHE_URL/test/slow_open.txt?pelican.timeout=300ms" 2>> "$BINARY_DIR/tests/$TEST_NAME/client.log" | tail -n 1)
CURL_EXIT=$?

if [ "$HTTP_CODE" != 500 ] && [ "$HTTP_CODE" != 504 ]; then
  cat "$BINARY_DIR/tests/$TEST_NAME/cache.log"
  cat "$BINARY_DIR/tests/$TEST_NAME/client.log"
  echo "Download of slow_open file did not result in expected HTTP code (500 or 504): $HTTP_CODE"
  exit 1
fi

echo "Running $TEST_NAME - slow read"

HTTP_CODE=$(curl -m 10 --raw --cacert "$X509_CA_FILE" -v -L -o "$BINARY_DIR/tests/$TEST_NAME/slow.log" --write-out '%{http_code}' -H "@$HEADER_FILE" -H "X-Transfer-Status: true" -H "TE: trailers" "$CACHE_URL/test/slow_read.txt?pelican.timeout=300ms" 2>> "$BINARY_DIR/tests/$TEST_NAME/client.log")
CURL_EXIT=$?

if [ "$HTTP_CODE" != 200 ]; then
  cat "$BINARY_DIR/tests/$TEST_NAME/cache.log"
  cat "$BINARY_DIR/tests/$TEST_NAME/client.log"
  echo "Download of slow_read file did not result in expected HTTP code (200): $HTTP_CODE"
  exit 1
fi

grep -v "aaaaaaaaaaaaaaa" "$BINARY_DIR/tests/$TEST_NAME/slow.log" | tail -n 1 > "$BINARY_DIR/tests/$TEST_NAME/slow-filtered.log"

if grep -q "X-Transfer-Status: 504: Unable to read /test/slow_read.txt; sTREAM ioctl timeout" "$BINARY_DIR/tests/$TEST_NAME/slow-filtered.log"; then
  cat "$BINARY_DIR/tests/$TEST_NAME/cache.log"
  cat "$BINARY_DIR/tests/$TEST_NAME/client.log"
  echo "Unexpected transfer status for slow read file:"
  cat "$BINARY_DIR/tests/$TEST_NAME/slow.log"
  exit 1
fi

echo "Running $TEST_NAME - stalled read"

HTTP_CODE=$(curl -m 10 --raw --cacert "$X509_CA_FILE" -v -L -o "$BINARY_DIR/tests/$TEST_NAME/slow.log" --write-out '%{http_code}' -H "@$HEADER_FILE" -H "X-Transfer-Status: true" -H "TE: trailers" "$CACHE_URL/test/stall_read.txt?pelican.timeout=300ms" 2>> "$BINARY_DIR/tests/$TEST_NAME/client.log")
CURL_EXIT=$?

if [ "$HTTP_CODE" != 200 ]; then
  cat "$BINARY_DIR/tests/$TEST_NAME/cache.log"
  cat "$BINARY_DIR/tests/$TEST_NAME/client.log"
  echo "Download of slow_read file did not result in expected HTTP code (200): $HTTP_CODE"
  exit 1
fi

grep -v "aaaaaaaaaaaaaaa" "$BINARY_DIR/tests/$TEST_NAME/slow.log" | tail -n 1 > "$BINARY_DIR/tests/$TEST_NAME/slow-filtered.log"

if grep -q "X-Transfer-Status: 504: Unable to read /test/slow_read.txt; sTREAM ioctl timeout" "$BINARY_DIR/tests/$TEST_NAME/slow-filtered.log"; then
  cat "$BINARY_DIR/tests/$TEST_NAME/cache.log"
  cat "$BINARY_DIR/tests/$TEST_NAME/client.log"
  echo "Unexpected transfer status for slow read file:"
  cat "$BINARY_DIR/tests/$TEST_NAME/slow.log"
  exit 1
fi

echo "Running $TEST_NAME - checksum query"

CONTENTS=$(curl -I --cacert "$X509_CA_FILE" -v -L --fail -H 'Want-Digest: md5' -H "@$HEADER_FILE" "$CACHE_URL/test/hello_world.txt" 2>> "$BINARY_DIR/tests/$TEST_NAME/client.log")
CURL_EXIT=$?

if [ $CURL_EXIT -ne 0 ]; then
  cat "$BINARY_DIR/tests/$TEST_NAME/cache.log"
  cat "$BINARY_DIR/tests/$TEST_NAME/client.log"
  echo "Checksum of hello-world text failed"
  exit 1
fi

if [ "$(echo "$CONTENTS" | grep -c 'Digest: md5=mvL4IYsVDDUa2ALG89Zqvg==')" -ne "1" ]; then
  cat "$BINARY_DIR/tests/$TEST_NAME/cache.log"
  cat "$BINARY_DIR/tests/$TEST_NAME/client.log"
  cat "Digest incorrect or missing"
  exit 1
fi

fi

echo "Running $TEST_NAME - missing authz"

HTTP_CODE=$(curl --output /dev/null --cacert "$X509_CA_FILE" -v -L --write-out '%{http_code}' -H "Authorization: Bearer missing" "$CACHE_URL/test/hello_world.txt" 2>> "$BINARY_DIR/tests/$TEST_NAME/client.log")
if [ "$HTTP_CODE" -ne 403 ]; then
  cat "$BINARY_DIR/tests/$TEST_NAME/cache.log"
  cat  "$BINARY_DIR/tests/$TEST_NAME/client.log"
  echo "Expected HTTP code is 403; actual was $HTTP_CODE"
  exit 1
fi

echo "Running $TEST_NAME - missing object"

HTTP_CODE=$(curl --output /dev/null --cacert "$X509_CA_FILE" -v -L --write-out '%{http_code}' -H "@$HEADER_FILE" "$CACHE_URL/test/missin.txt" 2>> "$BINARY_DIR/tests/$TEST_NAME/client.log")
if [ "$HTTP_CODE" -ne 404 ]; then
  echo "Expected HTTP code is 404; actual was $HTTP_CODE"
  exit 1
fi

echo "Running $TEST_NAME - download directory"

HTTP_CODE=$(curl --output "$BINARY_DIR/tests/$TEST_NAME/directory.out" --cacert "$X509_CA_FILE" -v -L --write-out '%{http_code}' "$CACHE_URL/test-public/subdir" 2>> "$BINARY_DIR/tests/$TEST_NAME/client.log")
# Depending on the xrootd version, it seems that either 409 or 500 are a possibility
if [ "$HTTP_CODE" -ne 200 ]; then
  echo "Expected HTTP code is 200; actual was $HTTP_CODE"
  cat "$BINARY_DIR/tests/$TEST_NAME/client.log"
  cat "$BINARY_DIR/tests/$TEST_NAME/cache.log"
  cat "$BINARY_DIR/tests/$TEST_NAME/directory.out"
  exit 1
fi

##
# It's unfortunate but there's no good machine-readable output from the HTTP protocol.
# Switch to using xrdfs which also isn't great formatting -- but at least workable with egrep.
CACHE_ROOT_URL="$(echo "$CACHE_URL" | sed 's|https://|roots://|')"
echo "Listing directory $CACHE_ROOT_URL via root protocol"
#export XRD_LOGLEVEL=Debug
export X509_CERT_FILE=$X509_CA_FILE
export BEARER_TOKEN_FILE=$READ_TOKEN
chmod 0600 "$BEARER_TOKEN_FILE"

# Unset any env var that might have leaked from the caller - forces use of $BEARER_TOKEN_FILE
unset BEARER_TOKEN

if ! "$XRDFS_BIN" "$CACHE_ROOT_URL" ls -l /test-public/subdir > "$BINARY_DIR/tests/$TEST_NAME/xrdfs.out"; then
  echo "Failed to list directory via root:// protocol"
  exit 1
fi

echo "Output from xrdfs:"
cat "$BINARY_DIR/tests/$TEST_NAME/xrdfs.out"
assert egrep -q -e '-r-----r-- (.*) 0 (.*) /test-public/subdir/test1' "$BINARY_DIR/tests/$TEST_NAME/xrdfs.out"
assert egrep -q -e '-r-----r-- (.*) 14 (.*) /test-public/subdir/test2' "$BINARY_DIR/tests/$TEST_NAME/xrdfs.out"
assert egrep -q -e 'dr-x---r-x (.*) /test-public/subdir/test3' "$BINARY_DIR/tests/$TEST_NAME/xrdfs.out"
assert_eq 3 "$(wc -l "$BINARY_DIR/tests/$TEST_NAME/xrdfs.out" | awk '{print $1}')"

