#!/usr/bin/env bash

function setup_httpsredirect() {
	require_commands python3
}

function test_httpsredirect() {
	local headers
	headers=$(python3 "${SOURCE_DIR}/httpsredirect.py" \
		"${XRD_PORT}" ../tls/ca.pem) \
		|| error "HTTPS-to-HTTP self-redirect did not close the TLS connection"

	headers=$(printf '%s\n' "${headers}" | tr -d '\r')
	printf '%s\n' "${headers}"
	printf '%s\n' "${headers}" | grep -Fqx 'HTTP/1.1 302 Found' \
		|| error "HTTPS-to-HTTP self-redirect did not return HTTP 302"
	printf '%s\n' "${headers}" | grep -Fiqx 'Connection: Close' \
		|| error "HTTPS-to-HTTP self-redirect did not close a keep-alive request"
}
