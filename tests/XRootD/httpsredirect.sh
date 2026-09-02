#!/usr/bin/env bash

function setup_httpsredirect() {
	require_commands curl
}

function test_httpsredirect() {
	local headers
	headers=$(curl --silent --show-error --http1.1 \
		--cacert ../tls/ca.pem \
		--header 'Connection: Keep-Alive' \
		--dump-header - --output /dev/null \
		"https://localhost:${XRD_PORT}/test")

	headers=$(printf '%s\n' "${headers}" | tr -d '\r')
	printf '%s\n' "${headers}"
	printf '%s\n' "${headers}" | grep -Fqx 'HTTP/1.1 302 Found' \
		|| error "HTTPS-to-HTTP self-redirect did not return HTTP 302"
	printf '%s\n' "${headers}" | grep -Fiqx 'Connection: Close' \
		|| error "HTTPS-to-HTTP self-redirect did not close a keep-alive request"
}
