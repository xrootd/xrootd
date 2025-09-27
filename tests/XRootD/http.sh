#!/usr/bin/env bash

function setup_http() {
	require_commands davix-{get,put,mkdir,rm} openssl curl
	openssl rand -base64 -out macaroons-secret 64
}

function teardown_http() {
	rm macaroons-secret
}

function test_http() {
	echo
	echo "client: XRootD $(xrdcp --version 2>&1)"
	echo "server: XRootD $(xrdfs "${HOST}" query config version 2>&1)"
	echo

	# create local temporary directory
	TMPDIR=$(mktemp -d "${PWD}/${NAME}/test-XXXXXX")

	# create remote temporary directory
	# this will get cleaned up by CMake upon fixture tear down
	assert xrdfs "${HOST}" mkdir -p "${TMPDIR}"

	# from now on, we use HTTP
	export HOST="http://localhost:${XRD_PORT}"

	# create local files with random contents using OpenSSL

	FILES=$(seq -w 1 "${NFILES:-10}")

	for i in $FILES; do
		assert openssl rand -base64 -out "${TMPDIR}/${i}.ref" $((1024 * (RANDOM + 1)))
	done

	# upload local files to the server in parallel with davix-put

	for i in $FILES; do
		assert davix-put "${TMPDIR}/${i}.ref" "${HOST}/${TMPDIR}/${i}.ref"
	done
	printf "%1048576s" " " | sed 's/ /blah/g' > "${TMPDIR}/fail_read.txt"
	assert davix-put "${TMPDIR}/fail_read.txt" "${HOST}/${TMPDIR}/fail_read.txt"
	assert davix-put "${TMPDIR}/${i}.ref" "${HOST}/${TMPDIR}/testlistings/01.ref"

	# list uploaded files, then download them to check for corruption

	assert davix-ls "${HOST}/${TMPDIR}"

	# download files back with davix-get

	for i in $FILES; do
		assert davix-get "${HOST}/${TMPDIR}/${i}.ref" "${TMPDIR}/${i}.dat"
	done

	# check that all checksums for downloaded files match

	for i in $FILES; do
		REF32C=$(xrdcrc32c < "${TMPDIR}/${i}.ref" | cut -d' '  -f1)
		NEW32C=$(xrdcrc32c < "${TMPDIR}/${i}.dat" | cut -d' '  -f1)

		REFA32=$(xrdadler32 < "${TMPDIR}/${i}.ref" | cut -d' '  -f1)
		NEWA32=$(xrdadler32 < "${TMPDIR}/${i}.dat" | cut -d' '  -f1)

		if [[ "${NEWA32}" != "${REFA32}" ]]; then
			echo 1>&2 "${i}: adler32: reference: ${REFA32}, downloaded: ${NEWA32}"
			error "adler32 checksum check failed for file: ${i}.dat"
		fi

		if [[ "${NEW32C}" != "${REF32C}" ]]; then
			echo 1>&2 "${i}:  crc32c: reference: ${REF32C}, downloaded: ${NEW32C}"
			error "crc32 checksum check failed for file: ${i}.dat"
		fi
	done

	assert davix-ls "${HOST}/"

	for i in $FILES; do
	       assert davix-rm "${HOST}/${TMPDIR}/${i}.ref"
	done

  # GET range-request
  ## Upload a file with a fixed content string
  alphabetFile="alphabet.txt"
  alphabetFilePath="${TMPDIR}/$alphabetFile"
  echo -n "abcdefghijklmnopqrstuvw987" > "$alphabetFilePath"
  assert curl -v -L -H 'Transfer-Encoding: chunked' "${HOST}/$alphabetFilePath" --upload-file "$alphabetFilePath"
  ## Upload a file without chunked encoding; search to see if the oss.asize flag is set in the OSS query
  assert curl -v -L "${HOST}/$alphabetFilePath.2" --upload-file "$alphabetFilePath"
  # Since the query parameters are not logged, we look to see if the length of the URL (which *is* logged) is increased
  # by the correct amount between the first and second upload.  The first upload was done with transfer encoding, meaning
  # XRootD doesn't know the final size of the object and hence doesn't append the '?oss.asize=' flag
  # First, look for the thread that performed the alphabet.txt upload, then the size of the command
  uploadThread=$(grep PUT "$XROOTD_SERVER_LOGFILE" | grep 'alphabet.txt HTTP/1.1' | awk '{print $3}' | head -n 1)
  firstUrlLength=$(grep " $uploadThread " "$XROOTD_SERVER_LOGFILE" | grep PUT -A 30 | grep alphabet.txt -A 30 | grep 'Xrootd_Protocol: 0000 Bridge req=3010' | head -n 1 | tr '=' ' ' | awk '{print $NF}')
  # Next, the addition of '.2?oss.asize=26' is an increase of 15 characters
  uploadThread=$(grep PUT "$XROOTD_SERVER_LOGFILE" | grep 'alphabet.txt.2 HTTP/1.1' | awk '{print $3}' | head -n 1)
  secondUrlLength=$(grep " $uploadThread " "$XROOTD_SERVER_LOGFILE" | grep PUT -A 30 | grep alphabet.txt.2 -A 30 | grep 'Xrootd_Protocol: 0000 Bridge req=3010' | head -n 1 | tr '=' ' ' | awk '{print $NF}')
  assert_eq "$((firstUrlLength+15))" "$secondUrlLength" "PUT request is missing oss.asize argument"

  assert curl -L -H 'Transfer-Encoding: chunked' "${HOST}/$alphabetFilePath" --upload-file "$alphabetFilePath"
  outputFilePath=${TMPDIR}/output.txt
  ## Download the file to a file and sanitize the output (remove '\r')
  curl -v -L --silent -H 'range: bytes=0-3,24-26' "${HOST}/$alphabetFilePath" --output - | tr -d '\r' > "$outputFilePath"
  ## Check the first content range header received
  contentRange=$(grep -i 'Content-range' "$outputFilePath" | awk 'NR==1')
  expectedContentRange='Content-range: bytes 0-3/26'
  assert_eq "$expectedContentRange" "$contentRange" "GET range-request test failed (first Content-range)"
  ## Check the first body received
  expectedBody='abcd'
  receivedBody=$(grep -E 'abcd$' "$outputFilePath")
  assert_eq "$expectedBody" "$receivedBody" "GET range-request test failed (first body)"
  ## Check the second content range header received
  contentRange=$(grep -i 'Content-range' "$outputFilePath"| awk 'NR==2')
  expectedContentRange='Content-range: bytes 24-25/26'
  assert_eq "$expectedContentRange" "$contentRange" "GET range-request test failed (second Content-range)"
  ## Check the second body received
  expectedBody='87'
  receivedBody=$(grep -E '87' "$outputFilePath")
  assert_eq "$expectedBody" "$receivedBody" "GET range-request test failed (second body)"
  ## Check the amount of boundary delimiters there is in the body
  expectedDelimiters=3
  receivedDelimiters=$(grep -c '\-\-123456' "$outputFilePath")
  assert_eq "$expectedDelimiters" "$receivedDelimiters" "GET range-request test failed (boundary delimiters)"
  ## GET with trailers
  curl -v -L --raw -H "X-Transfer-Status: true" -H "TE: trailers" "${HOST}/$alphabetFilePath" --output - | tr -d '\r' > "$outputFilePath"
  cat "$outputFilePath"
  expectedTransferStatus='X-Transfer-Status: 200: OK'
  receivedTransferStatus=$(grep -i 'X-Transfer-Status' "$outputFilePath")
  assert_eq "$expectedTransferStatus" "$receivedTransferStatus" "GET request with trailers test failed (transfer status)"
  # HEAD request
  curl -v -I -H 'Want-Digest: adler32' "${HOST}/$alphabetFilePath" | tr -d '\r' > "$outputFilePath"
  cat "$outputFilePath"
  grep '200 OK' "$outputFilePath" || error "HEAD request test failed: Failed to perform HEAD request on ${HOST}/$alphabetFilePath"
  expectedDigest="Digest: adler32="$(xrdadler32 "$alphabetFilePath" | cut -d' ' -f1)
  receivedDigest=$(grep -i "Digest" "$outputFilePath")
  assert_eq "$expectedDigest" "$receivedDigest" "HEAD request test failed (adler32)"
  expectedContentLength="Content-Length: $(wc -c < "$alphabetFilePath" | sed 's/^ *//')"
  # Explanation of the above line: Use wc -c for getting the size in bytes of a file, MacOS does not support stat --printf.
  # In addition, remove all spaces coming from `wc -c` as MacOS adds extra spaces in front of the number returned by wc -c...
  receivedContentLength=$(grep -i 'Content-Length' "$outputFilePath")
  assert_eq "$expectedContentLength" "$receivedContentLength" "HEAD request test failed (Content-Length)"

  xrdcrc32c -s "$alphabetFilePath"
  curl -v -I -H 'Want-Digest: crc32c' "${HOST}/$alphabetFilePath" | tr -d '\r' > "$outputFilePath"
  cat "$outputFilePath"
  expectedDigest="Digest: crc32c=ee24f29e"
  receivedDigest=$(grep "Digest" "$outputFilePath")
  assert_eq "$expectedDigest" "$receivedDigest" "HEAD request test failed (crc32c)"
  curl -v -I -H 'Want-Digest: NotSupported, adler32, crc32c' "${HOST}/$alphabetFilePath" | tr -d '\r' > "$outputFilePath"
  cat "$outputFilePath"
  expectedDigest="Digest: adler32="$(xrdadler32 "$alphabetFilePath" | cut -d' ' -f1)
  receivedDigest=$(grep -i "Digest" "$outputFilePath")
  assert_eq "$expectedDigest" "$receivedDigest" "HEAD request test failed (digest not supported)"
	wait

  ## Generated HTML has appropriate trailing slashes for directories
  HTTP_CODE=$(curl --output "$outputFilePath" -v -L --write-out '%{http_code}' "${HOST}/${TMPDIR}")
  assert_eq 200 "$HTTP_CODE"
  HTTP_CONTENTS=$(curl -v -L "${HOST}/${TMPDIR}" | tr '"' '\n' | tr '<' '\n' | tr '>' '\n' | grep testlistings/ | wc -l | tr -d ' ')
  assert_eq 2 "$HTTP_CONTENTS"

  ## OPTIONS has appropriate static headers
  curl -s -X OPTIONS -v --raw "${HOST}/$alphabetFilePath" 2>&1 | tr -d '\r' > "$outputFilePath"
  cat "$outputFilePath"
  expectedHeader='< Access-Control-Allow-Origin: *'
  receivedHeader=$(grep -i 'Access-Control-Allow-Origin:' "$outputFilePath")
  assert_eq "$expectedHeader" "$receivedHeader" "OPTIONS is missing statically-defined Access-Control-Allow-Origin"
  expectedHeader='< Test: 1'
  receivedHeader=$(grep -i 'Test:' "$outputFilePath")
  assert_eq "$expectedHeader" "$receivedHeader" "OPTIONS is missing statically-defined Test header"

  ## GET has appropriate static headers
  curl -s -v --raw "${HOST}/$alphabetFilePath" 2>&1 | tr -d '\r' > "$outputFilePath"
  cat "$outputFilePath"
  expectedHeader='< Foo: Bar'
  receivedHeader=$(grep -i 'Foo: Bar' "$outputFilePath")
  assert_eq "1" "$(echo "$receivedHeader" | wc -l | sed 's/^ *//')" "Incorrect number of 'Foo' header values"
  assert_eq "$expectedHeader" "$receivedHeader" "GET is missing statically-defined 'Foo: Bar' header"
  expectedHeader='< Foo: Baz'
  receivedHeader=$(grep -i 'Foo: Baz' "$outputFilePath")
  assert_eq "1" "$(echo "$receivedHeader" | wc -l | sed 's/^ *//')" "Incorrect number of 'Foo' header values"
  assert_eq "$expectedHeader" "$receivedHeader" "GET is missing statically-defined 'Foo: Baz' header"
  expectedHeader='< Test: 1'
  receivedHeader=$(grep -i 'Test:' "$outputFilePath")
  assert_eq "1" "$(echo "$receivedHeader" | wc -l | sed 's/^ *//')" "Incorrect number of 'Test' header values"
  assert_eq "$expectedHeader" "$receivedHeader" "GET is missing statically-defined Test header"

  ## HEAD has appropriate static headers (note HEAD has no verb-specific headers)
  curl -I -s --raw "${HOST}/$alphabetFilePath" 2>&1 | tr -d '\r' > "$outputFilePath"
  expectedHeader='Test: 1'
  receivedHeader=$(grep -i 'Test:' "$outputFilePath")
  assert_eq "1" "$(echo "$receivedHeader" | wc -l | sed 's/^ *//')" "Incorrect number of 'Test' header values"
  assert_eq "$expectedHeader" "$receivedHeader" "HEAD is missing statically-defined Test header"

  ## Download fails on a read failure
  # Default HTTP request: TCP socket abruptly closes
  assert_failure curl -v --raw "${HOST}/${TMPDIR}/fail_read.txt" --output /dev/null --write-out '%{http_code} %{size_download}' > "$outputFilePath"
  # Note: 'tail -n 1' done here as the assert_failure adds lines to the output
  HTTP_CODE=$(tail -n 1 "$outputFilePath" | awk '{print $1;}')
  DOWNLOAD_SIZE=$(tail -n 1 "$outputFilePath" | awk '{print $2;}')
  assert_eq "200" "$HTTP_CODE"
  assert_ne "4194304" "$DOWNLOAD_SIZE"

  # With transfer status summary enabled, connection is kept and error returned
  curl -v --raw -H 'TE: trailers' -H 'Connection: Keep-Alive' -H 'X-Transfer-Status: true' "${HOST}/${TMPDIR}/fail_read.txt?try=1" -v "${HOST}/${TMPDIR}/fail_read.txt?try=2" > "$outputFilePath" 2> "${TMPDIR}/stderr.txt"
  assert_eq "2" "$(grep -B 1 "X-Transfer-Status: 500: Unable to read" "$outputFilePath" | grep -c -E "^0")" "$(sed -e 's/blah//g' < "$outputFilePath")"
  assert_eq "0" "$(grep -c "Leftovers after chunking" "${TMPDIR}/stderr.txt")" "Incorrect framing in response: $(sed -e 's/blah//g' < "${TMPDIR}/stderr.txt")"
  assert_eq "0" "$(grep -c "Connection died" "${TMPDIR}/stderr.txt")" "Connection reuse did not work.  Server log: $(cat "${XROOTD_SERVER_LOGFILE}") Client log: $(sed -e 's/blah//g' < "${TMPDIR}/stderr.txt") Issue:"

  # Test CORS origin functionality
  curl -v -H 'Origin: does_not_exist' "${HOST}/$alphabetFilePath" 2>&1 | tr -d '\r' > "$outputFilePath"
  assert_eq 0 $(grep -c 'Access-Control-Allow-Origin' "$outputFilePath")

  curl -v -H 'Origin: https://webserver.bli.bla.blo' "${HOST}/$alphabetFilePath" 2>&1 | tr -d '\r' > "$outputFilePath"

  assert_eq 1 $(grep -c 'Access-Control-Allow-Origin' "$outputFilePath")
  accessControlAllowOrigin=$(cat "$outputFilePath" | grep 'Access-Control-Allow-Origin' | awk -F'< ' '{print $2}')
  assert_eq 'Access-Control-Allow-Origin: https://webserver.bli.bla.blo' "$accessControlAllowOrigin"

  run_and_assert_http_and_error_code() {
    local expected_http_code="$1"
    local expected_trailer_code="$2"
    shift 2

    local use_trailer=false
    local curl_args=()

    # Look for --with-trailer option
    for arg in "$@"; do
      if [[ "$arg" == "--with-trailer" ]]; then
        use_trailer=true
      else
        curl_args+=("$arg")
      fi
    done

    local body_file
    body_file=$(mktemp)
    local http_code

    # Add headers if trailers requested
    if $use_trailer; then
      curl_args+=(
        -H 'TE: trailers'
        -H 'X-Transfer-Status: true'
        -H 'Transfer-Encoding: chunked'
        --raw
      )
    fi

    # Run curl: body (with possible trailers) into body_file
    http_code=$(curl -s -L -v \
      -w "%{http_code}" \
      -o "$body_file" \
      "${curl_args[@]}")

    local body
    body=$(< "$body_file")

    # Assert HTTP code
    assert_eq "$expected_http_code" "$http_code"

    if $use_trailer; then
      # Look for trailer
      local trailer_line
      trailer_line=$(grep -i '^X-Transfer-Status:' "$body_file" | sed -E 's/^[^:]+: *//')

      if [[ -n "$trailer_line" ]]; then
        local trailer_code
        trailer_code=$(echo "$trailer_line" | cut -d: -f1 | xargs)
        assert_eq "$expected_trailer_code" "$trailer_code" "$trailer_line"
      fi
    fi

    rm -f "$body_file"
  }

  bigFilePath="${TMPDIR}/fail_read.txt"
  assert davix-put "$alphabetFilePath" "${HOST}/$alphabetFilePath"
  assert davix-put "$bigFilePath" "${HOST}/$bigFilePath"

  # Test writing to a readonly file system
  # Writing to a read-only file should return 403 Forbidden
  readOnlyFilePath="/readonly/file";
  run_and_assert_http_and_error_code 403 "" \
    --upload-file "$alphabetFilePath" "${HOST}/$readOnlyFilePath"

  # Overwrite a directory with a file - File / Directory conflict
  run_and_assert_http_and_error_code 409 "" \
    --upload-file "$alphabetFilePath" "${HOST}/$TMPDIR"

  # Test a file does not exist
  fileDoesNotExistFilePath="$TMPDIR/file_does_not_exist"
  run_and_assert_http_and_error_code 404 "" \
    "${HOST}/$fileDoesNotExistFilePath"

  # Test parent directory does not exist
  # XrootD Does not error on missing parent directory, it instead creates one
  # parentDirDoesNotExistFilePath="$TMPDIR/parent_dir_does_not_exist"
  # run_and_assert_http_and_error_code 200 404 \
  #   --upload-file "$alphabetFilePath" "${HOST}/$parentDirDoesNotExistFilePath" --with-trailer

  # Upload a file that should fail due to insufficient inodes
  noInodeFilePath="$TMPDIR/no_inode.txt"
  run_and_assert_http_and_error_code 507 "" \
    --upload-file "$alphabetFilePath" "${HOST}/$noInodeFilePath"

  # Fail upload due to insufficient user quota for inodes
  outOfInodeQuotaFilePath="$TMPDIR/out_of_inode_quota.txt"
  run_and_assert_http_and_error_code 507 "" \
    --upload-file "$alphabetFilePath" "${HOST}/$outOfInodeQuotaFilePath"

  # Upload a file that should fail due to insufficient space
  # The server can only close the connection if no space if left mid write
  # noSpaceFilePath="$TMPDIR/no_space.txt"
  # run_and_assert_http_and_error_code 507 507 \
  #   --upload-file "$bigFilePath" "${HOST}/$noSpaceFilePath"

  # Fail upload due to insufficient user quota for space
  # Not handled yet - connection is closed instead
  # outOfSpaceQuotaFilePath="$TMPDIR/out_of_space_quota.txt"
  # run_and_assert_http_and_error_code 507 200 \
  #   --upload-file "$bigFilePath" "${HOST}/$outOfSpaceQuotaFilePath"

  # Test file unreadable
  unreadableFilePath="$bigFilePath"
  run_and_assert_http_and_error_code 200 500 \
    "${HOST}/$unreadableFilePath" --with-trailer

  run_and_assert_http_and_error_code 200 "" \
    --header "Want-Digest: crc32c" -I "${HOST}/$alphabetFilePath"

}
