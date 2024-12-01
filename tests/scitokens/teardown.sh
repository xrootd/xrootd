#!/bin/sh

ISSUERDIR="$1"

rm -rf "$ISSUERDIR/issuer_key_1.pem" "$ISSUERDIR/issuer_key_2.pem"
rm -rf "$ISSUERDIR/issuer_pub_1.pem" "$ISSUERDIR/issuer_pub_2.pem"
rm -rf "$ISSUERDIR/issuer_1.jwks" "$ISSUERDIR/issuer_2.jwks"
rm -rf "$ISSUERDIR/ca"
rm -rf "$ISSUERDIR/export"
rm -rf "$ISSUERDIR/tlsca.ini" "$ISSUERDIR/tlsca.pem" "$ISSUERDIR/tlscakey.pem"
rm -rf "$ISSUERDIR/tls.crt" "$ISSUERDIR/tls.key" "$ISSUERDIR/tls.csr"
