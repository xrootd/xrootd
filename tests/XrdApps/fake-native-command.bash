#!/usr/bin/env bash

printf '%s\n' "$@" > "$TRACE_FILE"
{
    printf 'XRD_LOGLEVEL=%s\n' "${XRD_LOGLEVEL-}"
    printf 'XRD_REQUESTTIMEOUT=%s\n' "${XRD_REQUESTTIMEOUT-}"
    printf 'XRD_CPTIMEOUT=%s\n' "${XRD_CPTIMEOUT-}"
    printf 'XRD_NETWORKSTACK=%s\n' "${XRD_NETWORKSTACK-}"
    printf 'X509_USER_CERT=%s\n' "${X509_USER_CERT-}"
    printf 'X509_USER_KEY=%s\n' "${X509_USER_KEY-}"
    printf 'X509_USER_PROXY=%s\n' "${X509_USER_PROXY-unset}"
    printf 'XrdSecGSIUSERCERT=%s\n' "${XrdSecGSIUSERCERT-}"
    printf 'XrdSecGSIUSERKEY=%s\n' "${XrdSecGSIUSERKEY-}"
    printf 'XrdSecGSIUSERPROXY=%s\n' "${XrdSecGSIUSERPROXY-}"
    printf 'XrdSecCREDS=%s\n' "${XrdSecCREDS-unset}"
    printf 'XRD_HTTPCLIENTCERTFILE=%s\n' "${XRD_HTTPCLIENTCERTFILE-}"
    printf 'XRD_HTTPCLIENTKEYFILE=%s\n' "${XRD_HTTPCLIENTKEYFILE-}"
    printf 'XRD_LOGFILE=%s\n' "${XRD_LOGFILE-}"
} > "$ENV_FILE"
printf 'delegated output\n'
exit "${FAKE_STATUS:-0}"
