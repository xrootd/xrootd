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
}
