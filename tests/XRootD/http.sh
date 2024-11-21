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
	export HOST=http://localhost:8094

	# create local files with random contents using OpenSSL

	FILES=$(seq -w 1 "${NFILES:-10}")

	for i in $FILES; do
		assert openssl rand -base64 -out "${TMPDIR}/${i}.ref" $((1024 * (RANDOM + 1)))
	done

	# upload local files to the server in parallel with davix-put

	for i in $FILES; do
		assert davix-put "${TMPDIR}/${i}.ref" "${HOST}/${TMPDIR}/${i}.ref"
	done

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
	echo -n "abcdefghijklmnopqrstuvw987" > $alphabetFilePath
	assert curl -v -L -H 'Transfer-Encoding: chunked' "${HOST}/$alphabetFilePath" --upload-file $alphabetFilePath
  outputFilePath=${TMPDIR}/output.txt
  ## Download the file to a file and sanitize the output (remove '\r')
  curl -v -L --silent -H 'range: bytes=0-3,24-26' "${HOST}/$alphabetFilePath" --output - | tr -d '\r' > $outputFilePath
  ## Check the first content range header received
  contentRange=`cat $outputFilePath | grep -i 'Content-range' | awk 'NR==1'`
  expectedContentRange='Content-range: bytes 0-3/26'
  [[ "$expectedContentRange" == "$contentRange" ]] || error "GET range-request test failed: first Content-range expected: $expectedContentRange,  first Content-range received: $contentRange"
  ## Check the first body received
  expectedBody='abcd'
  receivedBody=`cat $outputFilePath | egrep 'abcd$'`
  [[ "$expectedBody" == "$receivedBody" ]] || error "GET range-request test failed: first expected body: $expectedBody, first received body: $receivedBody"
  ## Check the second content range header received
  contentRange=`cat $outputFilePath | grep -i 'Content-range' | awk 'NR==2'`
  expectedContentRange='Content-range: bytes 24-25/26'
  [[ "$expectedContentRange" == "$contentRange" ]] || error "GET range-request test failed: second Content-range expected: $expectedContentRange,  second Content-range received: $contentRange"
  ## Check the second body received
  expectedBody='87'
  receivedBody=`cat $outputFilePath | egrep '87'`
  [[ "$expectedBody" == "$receivedBody" ]] || error "GET range-request test failed: second expected body: $expectedBody, second received body: $receivedBody"
  ## Check the amount of boundary delimiters there is in the body
  expectedDelimiters=3
  receivedDelimiters=`cat $outputFilePath | grep -c '\-\-123456'`
  [[ "$expectedDelimiters" == "$receivedDelimiters" ]] || error "GET range-request test failed: received $receivedDelimiters boundary delimiters instead of $expectedDelimiters"
  ## GET with trailers
  file1GPath=${TMPDIR}/file.1G
  curl -v -L --raw -H "X-Transfer-Status: true" -H "TE: trailers" "${HOST}/$alphabetFilePath" --output - | tr -d '\r' > $outputFilePath
  cat $outputFilePath
  expectedTransferStatus='X-Transfer-Status: 200: OK'
  receivedTransferStatus=`grep -i 'X-Transfer-Status' $outputFilePath`
  [[ "$expectedTransferStatus" == "$receivedTransferStatus" ]] || error "GET request with trailers test failed: expected $expectedTransferStatus, received $receivedTransferStatus"
  # HEAD request
  curl -v -I -H 'Want-Digest: adler32' "${HOST}/$alphabetFilePath" | tr -d '\r' > $outputFilePath
  cat $outputFilePath
  grep '200 OK' $outputFilePath || error "HEAD request test failed: Failed to perform HEAD request on ${HOST}/$alphabetFilePath"
  expectedDigest="Digest: adler32="`xrdadler32 $alphabetFilePath | cut -d' ' -f1`
  receivedDigest=`grep -i "Digest" $outputFilePath`
  [[ "$expectedDigest" == "$receivedDigest" ]] || error "HEAD request test failed: adler32 expected $expectedDigest, received $receivedDigest"
  expectedContentLength="Content-Length: `wc -c < $alphabetFilePath | sed 's/^ *//'`"
  # Explanation of the above line: Use wc -c for getting the size in bytes of a file, MacOS does not support stat --printf.
  # In addition, remove all spaces coming from `wc -c` as MacOS adds extra spaces in front of the number returned by wc -c...
  receivedContentLength=`cat $outputFilePath | grep -i 'Content-Length'`
  [[ "$expectedContentLength" == "$receivedContentLength" ]] || error "HEAD request test failed: expected $expectedContentLength, received $receivedContentLength"

  xrdcrc32c -s $alphabetFilePath
  curl -v -I -H 'Want-Digest: crc32c' "${HOST}/$alphabetFilePath" | tr -d '\r' > $outputFilePath
  cat $outputFilePath
  expectedDigest="Digest: crc32c=7iTyng=="
  receivedDigest=`grep "Digest" $outputFilePath`
  [[ "$expectedDigest" == "$receivedDigest" ]] || error "HEAD request test failed: crc32c expected $expectedDigest, received $receivedDigest"

  curl -v -I -H 'Want-Digest: NotSupported, adler32, crc32c' "${HOST}/$alphabetFilePath" | tr -d '\r' > $outputFilePath
  cat $outputFilePath
  expectedDigest="Digest: adler32="`xrdadler32 $alphabetFilePath | cut -d' ' -f1`
  receivedDigest=`grep -i "Digest" $outputFilePath`
  [[ "$expectedDigest" == "$receivedDigest" ]] || error "HEAD request test failed: digest-not-supported-test: expected $expectedDigest, received $receivedDigest"
	wait
}
