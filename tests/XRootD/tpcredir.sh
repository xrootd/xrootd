#!/usr/bin/env bash

function setup_tpcredir() {
	require_commands curl
	# The HTTP TPC handler issues a HEAD request to the Source URL before
	# opening the local destination.  Provide a small file the loopback
	# HEAD can succeed against; the actual transfer never starts because
	# the destination open returns SFS_REDIRECT first.
	echo "test source contents" > "${REMOTE_DIR}/source.txt"
	# The XRootD-protocol sub-test uses xrdcp to upload this local file.
	# The transfer never starts: the destination open returns SFS_REDIRECT
	# (same OFS path as above), so the file content is irrelevant.
	echo "client upload payload" > "${LOCAL_DIR}/upload.txt"
}

function teardown_tpcredir() {
	:
}

function test_tpcredir() {
	export HTTP_HOST="${HOST/root:/http:}"

	echo
	echo "client: XRootD $(xrdcp --version 2>&1)"
	echo "server: XRootD $(xrdfs ${HOST} query config version 2>&1)"
	echo

	local response="${LOCAL_DIR}/copy_response.txt"

	# COPY destination is /destfile?tpc.key=test.  The opaque on the URL
	# is what XrdHttp passes through as 'xrd-http-query' to the TPC ext
	# handler, which the handler then folds into the destination open's
	# opaque; that is what makes the OFS layer return SFS_REDIRECT (see
	# tpcredir.cfg for the full chain).  Putting tpc.key in a real HTTP
	# header would not work: XrdHttp overwrites that header with the URL
	# opaque on entry to the ext handler.  We deliberately do NOT pass
	# -L so curl reports the 307 to us instead of following it to the
	# (unbound) sentinel host.
	curl -s -i -X COPY \
	     -H "Source: ${HTTP_HOST}source.txt" \
	     "${HTTP_HOST}destfile?tpc.key=test" -o "${response}"

	cat "${response}"

	grep -q '^HTTP/.* 307' "${response}" \
		|| error "expected 307 Temporary Redirect; got: $(head -1 "${response}")"

	grep -qi '^Location:.*rdlplugin-rewritten' "${response}" \
		|| error "Location header was not rewritten by the redirect plugin: $(grep -i Location "${response}")"

	grep -qi '^Location:.*:9999' "${response}" \
		|| error "Location header port was not set by the redirect plugin: $(grep -i Location "${response}")"

	# The cfg's "ofs.tpc redirect localhost:9998?sfsopq=sfsval" makes the
	# XrdSfs layer contribute "sfsopq=sfsval" as opaque on the redirect.
	# Combined with the client's own "tpc.key=test" (which XrdHttp surfaces
	# via the xrd-http-fullresource header that RedirectTransfer uses to
	# build the path component of the Location), the redirect URL must
	# carry BOTH values, joined with '&', not '?'.  Two '?' would yield a
	# malformed URL where downstream parsers silently fold the second '?'
	# into the previous parameter's value.
	grep -qi '^Location:.*tpc\.key=test&sfsopq=sfsval' "${response}" \
		|| error "Location header should carry client and SFS opaque joined with '&'; got: $(grep -i Location "${response}")"

	# ------------------------------------------------------------------
	# Second sub-test: same plugin, reached via the XRootD protocol's
	# fsRedirPI() instead of HTTP TPC's RedirectTransfer.  We reuse the
	# same OFS-level trigger: an open carrying tpc.key=test in the URL
	# opaque makes XrdOfs::open return SFS_REDIRECT (per "ofs.tpc
	# redirect" in the cfg).  In the XRootD protocol that SFS_REDIRECT
	# is dispatched through fsRedirPI() -> XrdXrootdRedirHelper::Redirect(),
	# where the test plugin rewrites the target host to
	# "rdlplugin-rewritten" and the port to 9999.  xrdcl logs the
	# redirect target it received from the server in its Debug log
	# (XRD_LOGFILE / client.log) -- we grep that for both the
	# plugin-rewritten host and the plugin-rewritten port.  The first
	# sub-test uses curl, not xrdcl, so XRD_LOGFILE is not polluted by
	# it.
	# ------------------------------------------------------------------
	XRD_CONNECTIONRETRY=0 \
		xrdcp -f "${LOCAL_DIR}/upload.txt" \
		      "${HOST}/destfile?tpc.key=test" || true

	local expected='rdlplugin-rewritten:9999'
	local received
	received="$(grep -oE 'Redirected from: [^[:space:]]+ to: [^[:space:]]+' "${XRD_LOGFILE}" | tail -1)"
	if ! printf '%s' "${received}" | grep -qE "to:.*${expected}"; then
		error "xrdcl client did not receive plugin-rewritten redirect target.
  expected (host+port substring): ${expected}
  received: ${received:-<no Redirected line in ${XRD_LOGFILE}>}"
	fi

	# ------------------------------------------------------------------
	# Third sub-test: URL-form SFS_REDIRECT.  The XrdOssUrlRedirTestPlugin
	# OSS wrapper (loaded in tpcredir.cfg) returns -EDESTADDRREQ +
	# FileURL=root://urlredir-target:9996/... for any path under
	# /urlredir/, so XrdOfs::open emits SFS_REDIRECT with port=-1 and the
	# URL in the SfsError.  XrdXrootdProtocol::fsRedirPI() routes that
	# through the URL-form branch of XrdXrootdRedirHelper::Redirect(),
	# where the redirect plugin's RedirectURL() override rewrites the
	# target URL to "root://rdlplugin-rewritten-url:9997//destfile".
	# This sub-test asserts that distinct URL-form sentinel; finding it
	# proves the URL branch (not the host:port branch) was exercised.
	# ------------------------------------------------------------------
	XRD_CONNECTIONRETRY=0 \
		xrdcp -f "${LOCAL_DIR}/upload.txt" \
		      "${HOST}/urlredir/destfile" || true

	local url_expected='rdlplugin-rewritten-url:9997'
	local url_received
	url_received="$(grep -oE 'Redirected from: [^[:space:]]+ to: [^[:space:]]+' "${XRD_LOGFILE}" | tail -1)"
	if ! printf '%s' "${url_received}" | grep -qE "to:.*${url_expected}"; then
		error "xrdcl client did not receive plugin-rewritten URL-form redirect.
  expected (host+port substring): ${url_expected}
  received: ${url_received:-<no Redirected line in ${XRD_LOGFILE}>}"
	fi
}
