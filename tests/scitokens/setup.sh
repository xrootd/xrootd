#!/bin/bash

ISSUERDIR="$1"

mkdir -p "$ISSUERDIR"
pushd "$ISSUERDIR" || exit 1

###
## Generate two different issuer keys
###

for idx in 1 2; do
  # Generate signing key and public version
  if ! openssl ecparam -name prime256v1 -genkey -noout -out "issuer_key_$idx.pem"; then
    echo "Failed to generate EC private key"
    exit 1
  fi

  if ! openssl ec -in "issuer_key_$idx.pem" -pubout -out "issuer_pub_$idx.pem"; then
    echo "Failed to generate EC public key"
    exit 1
  fi

  # Generate the JWKS file
  if ! "$BINARY_DIR/bin/xrdscitokens-create-jwks" "issuer_pub_$idx.pem" "issuer_$idx.jwks" "test_$idx"; then
    echo "Failed to generate JWKS file"
    exit 1
  fi
done

###
## Create the CA & host certificate setup
###

if ! openssl genrsa -out tlscakey.pem 4096; then
  echo "Failed to generate CA private key"
  exit 1
fi

mkdir -p ca
touch ca/index.txt
echo '01' > ca/serial.txt

cat > tlsca.ini <<EOF

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
if ! openssl req -x509 -key tlscakey.pem -config tlsca.ini -out tlsca.pem -outform PEM -subj "/CN=XRootD CA" 0<&-; then
  echo "Failed to generate CA request"
  exit 1
fi

# Create the host certificate request
openssl genrsa -out tls.key 4096
chmod 0400 tls.key
if ! openssl req -new -key tls.key -config tlsca.ini -out tls.csr -outform PEM -subj /CN=localhost 0<&-; then
  echo "Failed to generate host certificate request"
  exit 1
fi

if ! openssl ca -config tlsca.ini -batch -policy signing_policy -extensions cert_extensions -out tls.crt -infiles tls.csr 0<&-; then
  echo "Failed to sign host certificate request"
  exit 1
fi
chmod 0600 tls.crt

###
## Create the directory structure to export
###
: "${SOURCE_DIR:="$(realpath "$(dirname "${BASH_SOURCE[0]}")")"}"

mkdir -p export/one/.well-known
cp issuer_1.jwks export/one/issuer.jwks
cp "$SOURCE_DIR/one-openid-configuration" export/one/.well-known/openid-configuration

mkdir -p export/two/.well-known
cp issuer_2.jwks export/two/issuer.jwks
cp "$SOURCE_DIR/two-openid-configuration" export/two/.well-known/openid-configuration

popd || exit 1
