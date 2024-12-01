#!/bin/sh

ISSUERDIR="$1"

mkdir -p "$ISSUERDIR"

if [ -z "$HOSTNAME" ]; then
	HOSTNAME=`hostname`
fi

###
## Generate two different issuer keys
###

for idx in 1 2; do
  # Generate signing key and public version
  openssl ecparam -name prime256v1 -genkey -noout -out "$ISSUERDIR/issuer_key_$idx.pem"
  if [ "$?" -ne 0 ]; then
    echo "Failed to generate EC private key"
    exit 1
  fi

  openssl ec -in "$ISSUERDIR/issuer_key_$idx.pem" -pubout -out "$ISSUERDIR/issuer_pub_$idx.pem"
  if [ "$?" -ne 0 ]; then
    echo "Failed to generate EC public key"
    exit 1
  fi

  # Generate the JWKS file
  $BINARY_DIR/tests/scitokens/xrdscitokens-create-jwks "$ISSUERDIR/issuer_pub_$idx.pem" "$ISSUERDIR/issuer_$idx.jwks" "test_$idx"
  if [ "$?" -ne 0 ]; then
    echo "Failed to generate JWKS file"
    exit 1
  fi
done

###
## Create the CA & host certificate setup
###

openssl genrsa -out "$ISSUERDIR/tlscakey.pem" 4096
if [ "$?" -ne 0 ]; then
  echo "Failed to generate CA private key"
  exit 1
fi

mkdir -p "$ISSUERDIR/ca"
touch "$ISSUERDIR/ca/index.txt"
echo '01' > "$ISSUERDIR/ca/serial.txt"

cat > "$ISSUERDIR/tlsca.ini" <<EOF

[ ca ]
default_ca = CA_test

[ CA_test ]

default_days = 365
default_md = sha256
private_key = $ISSUERDIR/tlscakey.pem
certificate = $ISSUERDIR/tlsca.pem
new_certs_dir = $ISSUERDIR/ca
database = $ISSUERDIR/ca/index.txt
serial = $ISSUERDIR/ca/serial.txt

[ req ]
default_bits = 4096
distinguished_name = ca_test_dn
x509_extensions = ca_extensions
string_mask = utf8only

[ ca_test_dn ]

commonName_default = Xrootd CA

[ ca_extensions ]

basicConstraints = critical,CA:true
keyUsage = keyCertSign,cRLSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid

[ signing_policy ]
countryName            = optional
stateOrProvinceName    = optional
localityName           = optional
organizationName       = optional
organizationalUnitName = optional
commonName             = supplied
emailAddress           = optional

[ cert_extensions ]

basicConstraints = critical,CA:false
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = critical, serverAuth, clientAuth

EOF

# Create the CA certificate
echo "Creating TLS CA certificate in $ISSUERDIR/tlsca.pem"
openssl req -x509 -key "$ISSUERDIR/tlscakey.pem" -config "$ISSUERDIR/tlsca.ini" -out "$ISSUERDIR/tlsca.pem" -outform PEM -subj "/CN=XRootD CA" 0<&-
if [ "$?" -ne 0 ]; then
  echo "Failed to generate CA request"
  exit 1
fi

# Create the host certificate request
openssl genrsa -out "$ISSUERDIR/tls.key" 4096
chmod 0400 "$ISSUERDIR/tls.key"
openssl req -new -key "$ISSUERDIR/tls.key" -config "$ISSUERDIR/tlsca.ini" -out "$ISSUERDIR/tls.csr" -outform PEM -subj "/CN=$HOSTNAME" 0<&-
if [ "$?" -ne 0 ]; then
  echo "Failed to generate host certificate request"
  exit 1
fi

openssl ca -config "$ISSUERDIR/tlsca.ini" -batch -policy signing_policy -extensions cert_extensions -out "$ISSUERDIR/tls.crt" -infiles "$ISSUERDIR/tls.csr" 0<&-
if [ "$?" -ne 0 ]; then
  echo "Failed to sign host certificate request"
  exit 1
fi
chmod 0600 "$ISSUERDIR/tls.crt"

###
## Create the directory structure to export
###

mkdir -p "$ISSUERDIR/export/one/.well-known"
cp "$ISSUERDIR/issuer_1.jwks" "$ISSUERDIR/export/one/issuer.jwks"
cat > "$ISSUERDIR/export/one/.well-known/openid-configuration" << EOF
{
  "jwks_uri": "https://$HOSTNAME:8095/issuer/one/issuer.jwks"
}
EOF

mkdir -p "$ISSUERDIR/export/two/.well-known"
cp "$ISSUERDIR/issuer_2.jwks" "$ISSUERDIR/export/two/issuer.jwks"
cat > "$ISSUERDIR/export/two/.well-known/openid-configuration" << EOF
{
  "jwks_uri": "https://$HOSTNAME:8095/issuer/two/issuer.jwks"
}
EOF

