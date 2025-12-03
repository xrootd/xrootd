#!/bin/sh

TEST_NAME=$1

if [ -z "$BINARY_DIR" ]; then
  echo "\$BINARY_DIR environment variable is not set; cannot run test"
  exit 1
fi
if [ ! -d "$BINARY_DIR" ]; then
  echo "$BINARY_DIR is not a directory; cannot run test"
  exit 1
fi
if [ -z "$SOURCE_DIR" ]; then
  echo "\$SOURCE_DIR environment variable is not set; cannot run test"
  exit 1
fi
if [ ! -d "$SOURCE_DIR" ]; then
  echo "\$SOURCE_DIR environment variable is not set; cannot run test"
  exit 1
fi

echo "Setting up S3 server for $TEST_NAME test"

if [ -z "$MINIO_BIN" ]; then
  echo "minio binary not found; cannot run unit test"
  exit 1
fi

if [ -z "$MC_BIN" ]; then
  echo "mc binary not found; cannot run unit test"
  exit 1
fi

XROOTD_BIN="$XROOTD_BINDIR/xrootd"
echo "XROOTD_BIN=$XROOTD_BIN"
if [ -z "XROOTD_BIN" ]; then
  echo "xrootd binary not found; cannot run unit test"
  exit 1
fi

mkdir -p "$BINARY_DIR/tests/$TEST_NAME"
RUNDIR=$(mktemp -d -p "$BINARY_DIR/tests/$TEST_NAME" test_run.XXXXXXXX)

if [ ! -d "$RUNDIR" ]; then
  echo "Failed to create test run directory; cannot run minio"
  exit 1
fi

echo "Using $RUNDIR as the test run's home directory."
cd "$RUNDIR"

####
#   Create CA for Minio server to use
####

MINIO_DATADIR="$RUNDIR/minio-data"
MINIO_CLIENTDIR="$RUNDIR/minio-client"
MINIO_CERTSDIR="$RUNDIR/minio-certs"
XROOTD_CONFIGDIR="$RUNDIR/xrootd-config"
mkdir -p "$XROOTD_CONFIGDIR"
XROOTD_RUNDIR=$(mktemp -d -p /tmp xrootd_test.XXXXXXXX)

mkdir -p "$MINIO_DATADIR"
mkdir -p "$MINIO_CERTSDIR/ca"
mkdir -p "$MINIO_CERTSDIR/CAs"
mkdir -p "$MINIO_CLIENTDIR"

echo > "$BINARY_DIR/tests/$TEST_NAME/server.log"

# Create the TLS credentials for the test
openssl genrsa -out "$MINIO_CERTSDIR/tlscakey.pem" 4096 >> "$BINARY_DIR/tests/$TEST_NAME/server.log"
touch "$MINIO_CERTSDIR/ca/index.txt"
echo '01' > "$MINIO_CERTSDIR/ca/serial.txt"

cat > "$MINIO_CERTSDIR/tlsca.ini" <<EOF

[ ca ]
default_ca = CA_test

[ CA_test ]

default_days = 365
default_md = sha256
private_key = $MINIO_CERTSDIR/tlscakey.pem
certificate = $MINIO_CERTSDIR/CAs/tlsca.pem
new_certs_dir = $MINIO_CERTSDIR/ca
database = $MINIO_CERTSDIR/ca/index.txt
serial = $MINIO_CERTSDIR/ca/serial.txt

[ req ]
default_bits = 4096
distinguished_name = ca_test_dn
x509_extensions = ca_extensions
string_mask = utf8only

[ ca_test_dn ]

commonName_default = Minio CA

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
keyUsage = digitalSignature
extendedKeyUsage = critical, serverAuth, clientAuth

EOF

# Create the CA certificate
openssl req -x509 -key "$MINIO_CERTSDIR/tlscakey.pem" -config "$MINIO_CERTSDIR/tlsca.ini" -out "$MINIO_CERTSDIR/CAs/tlsca.pem" -outform PEM -subj "/CN=Minio CA" 0<&- >> "$BINARY_DIR/tests/$TEST_NAME/server.log"
if [ "$?" -ne 0 ]; then
  echo "Failed to generate CA request"
  exit 1
fi

HOST="${HOSTNAME:-localhost}"

# Create the host certificate request
openssl genrsa -out "$MINIO_CERTSDIR/private.key" 4096 >> "$BINARY_DIR/tests/$TEST_NAME/server.log"
openssl req -new -key "$MINIO_CERTSDIR/private.key" -config "$MINIO_CERTSDIR/tlsca.ini" -out "$MINIO_CERTSDIR/public.csr" -outform PEM -subj "/CN=${HOST}" 0<&- >> "$BINARY_DIR/tests/$TEST_NAME/server.log"
if [ "$?" -ne 0 ]; then
  echo "Failed to generate host certificate request"
  exit 1
fi

openssl ca -config "$MINIO_CERTSDIR/tlsca.ini" -batch -policy signing_policy -extensions cert_extensions -out "$MINIO_CERTSDIR/public.crt" -infiles "$MINIO_CERTSDIR/public.csr" 0<&- 2>> "$BINARY_DIR/tests/$TEST_NAME/server.log"
if [ "$?" -ne 0 ]; then
  echo "Failed to sign host certificate request"
  exit 1
fi

####
#   Configure and launch minio server
####

# Set the minio root credentials:

export MINIO_ROOT_USER=minioadmin
export MINIO_ROOT_PASSWORD=QXDEiQxQw8qY
MINIO_USER=miniouser
MINIO_PASSWORD=2Z303QCzRI7s
printf "%s" "$MINIO_USER" > "$RUNDIR/access_key"
printf "%s" "$MINIO_PASSWORD" > "$RUNDIR/secret_key"

# Launch minio
"$MINIO_BIN" --certs-dir "$MINIO_CERTSDIR" server --address "${HOST}:0" "$MINIO_DATADIR" 0<&- >"$BINARY_DIR/tests/$TEST_NAME/server.log" 2>&1 &
MINIO_PID=$!
echo "minio daemon PID: $MINIO_PID"
sleep 1
MINIO_URL=$(grep "API: " "$BINARY_DIR/tests/$TEST_NAME/server.log" | tr ':' ' ' | awk '{print $NF}' | tail -n 1)
IDX=0
while [ -z "$MINIO_URL" ]; do
  sleep 1
  MINIO_URL=$(grep "API: " "$BINARY_DIR/tests/$TEST_NAME/server.log" | tr ':' ' ' | awk '{print $NF}' | tail -n 1)
  IDX=$(($IDX+1))
  if [ $IDX -gt 1 ]; then
    echo "Waiting for minio to start ($IDX seconds so far) ..."
  fi
  if [ $IDX -eq 60 ]; then
    echo "minio failed to start - failing"
    exit 1
  fi
done
MINIO_PORT=$MINIO_URL
MINIO_URL=https://${HOST}:$MINIO_URL
echo "Minio API server started on $MINIO_URL"

cat > "$BINARY_DIR/tests/$TEST_NAME/setup.sh" <<EOF
MINIO_URL=$MINIO_URL
MINIO_PID=$MINIO_PID
ACCESS_KEY_FILE=$RUNDIR/access_key
SECRET_KEY_FILE=$RUNDIR/secret_key
X509_CA_FILE=$MINIO_CERTSDIR/CAs/tlsca.pem
EOF
echo "Test environment written to $BINARY_DIR/tests/$TEST_NAME/setup.sh"

echo "minio logs are available at $BINARY_DIR/tests/$TEST_NAME/server.log"

echo "Starting configuration of minio"

"$MC_BIN" --insecure --config-dir "$MINIO_CLIENTDIR" alias set adminminio "$MINIO_URL" "$MINIO_ROOT_USER" "$MINIO_ROOT_PASSWORD"
"$MC_BIN" --insecure --config-dir "$MINIO_CLIENTDIR" admin user add adminminio "$MINIO_USER" "$MINIO_PASSWORD"
"$MC_BIN" --insecure --config-dir "$MINIO_CLIENTDIR" alias set userminio "$MINIO_URL" "$MINIO_USER" "$MINIO_PASSWORD"
"$MC_BIN" --insecure --config-dir "$MINIO_CLIENTDIR" admin policy attach adminminio readwrite --user "$MINIO_USER"
"$MC_BIN" --insecure --config-dir "$MINIO_CLIENTDIR" mb userminio/test-bucket
if [ $? -ne 0 ]; then
  echo "Failed to create test bucket in minio server"
  exit 1
fi

echo "Hello, World" > "$RUNDIR/hello_world.txt"
"$MC_BIN" --insecure --config-dir "$MINIO_CLIENTDIR" cp "$RUNDIR/hello_world.txt" userminio/test-bucket/hello_world.txt
"$MC_BIN" --insecure --config-dir "$MINIO_CLIENTDIR" cp "$RUNDIR/hello_world.txt" userminio/test-bucket/hello_world2.txt

IDX=0
COUNT=25
while [ $IDX -ne $COUNT ]; do
  if ! dd if=/dev/urandom "of=$RUNDIR/test_file" bs=1024 count=3096 2> /dev/null; then
    echo "Failed to create random file to upload"
    exit 1
  fi
  if ! "$MC_BIN" --insecure --config-dir "$MINIO_CLIENTDIR" cp "$RUNDIR/test_file" "userminio/test-bucket/test_file_$IDX.random" > /dev/null; then
    echo "Failed to upload random file to S3 instance"
    exit 1
  fi
  IDX=$((IDX+1))
done

####
#    Configure the token issuer and sample tokens
####

mkdir "$RUNDIR/issuer"
if ! "$OPENSSL_BIN" ecparam -name prime256v1 -genkey -noout -out "$RUNDIR/issuer/issuer_private.pem"; then
  echo "Failed to generate EC private key"
  exit 1
fi

if ! "$OPENSSL_BIN" ec -in "$RUNDIR/issuer/issuer_private.pem" -pubout -out "$RUNDIR/issuer/issuer_public.pem"; then
  echo "Failed to generate EC public key"
  exit 1
fi

if ! "$BINARY_DIR/bin/xrdscitokens-create-jwks" "$RUNDIR/issuer/issuer_public.pem" "$RUNDIR/issuer/issuer.jwks" "test_key"; then
  echo "Failed to generate JWKS file"
  exit 1
fi

# Prevent the issuer's keys from being cached by an earlier test run
XDG_CACHE_HOME="$RUNDIR/tmp"
rm -rf "$XDG_CACHE_HOME"
mkdir -p "$XDG_CACHE_HOME" || exit 1
export XDG_CACHE_HOME

# Pre-load the cache with the JWKS; prevents us from having to mock up an issuer via HTTPS
cat > "$RUNDIR/issuer/sql" << EOF
BEGIN TRANSACTION;
CREATE TABLE keycache (issuer text UNIQUE PRIMARY KEY NOT NULL,keys text NOT NULL);
INSERT INTO keycache VALUES('https://localhost:9443','{"expires":$(($(date '+%s') + 3600)),"jwks":$(cat "$RUNDIR/issuer/issuer.jwks"),"next_update":$(($(date '+%s') + 3600))}');
COMMIT;
EOF

if ! mkdir -p "$XDG_CACHE_HOME/scitokens"; then
  echo "Failed to generate sqlite database directory"
  exit 1
fi

if ! $(cat "$RUNDIR/issuer/sql" | sqlite3 "$XDG_CACHE_HOME/scitokens/scitokens_cpp.sqllite"); then
  echo "Failed to generate sqlite database"
  exit 1
fi

if ! "$BINARY_DIR/bin/xrdscitokens-create-token" \
    "$RUNDIR/issuer/issuer_public.pem" "$RUNDIR/issuer/issuer_private.pem" test_key \
    https://localhost:9443 storage.read:/ 600 > "$RUNDIR/token"; then
  echo "Failed to generate read token"
  exit 1
fi
echo "Sample read token available at $RUNDIR/token"
  
if ! "$BINARY_DIR/bin/xrdscitokens-create-token" \
    "$RUNDIR/issuer/issuer_public.pem" "$RUNDIR/issuer/issuer_private.pem" test_key \
    https://localhost:9443 storage.modify:/ 600 > "$RUNDIR/write.token"; then
  echo "Failed to generate write token"
  exit 1 
fi
echo "Sample write token available at $RUNDIR/write.token"

####
#    Setup the client plugin directory for the PSS to use
####
export XRD_PLUGINCONFDIR="$RUNDIR/client.plugins.d"
mkdir -p "$XRD_PLUGINCONFDIR"
  
cat > "$XRD_PLUGINCONFDIR/curl-plugin.conf" <<EOF

url = https://*
lib = libXrdClCurl.so
enable = true

EOF

cat > "$XRD_PLUGINCONFDIR/s3-plugin.conf" <<EOF

url = s3://*
lib = libXrdClS3.so
enable = true

EOF

# We create a link to the `client.plugins.d` directory at a fixed, known
# location because it must be provided to the gtest executable via an
# environment variable specified in the CMakeLists.txt (which is not
# dynamically generated).
if [ -L "$BINARY_DIR/tests/$TEST_NAME/client.plugins.d" ]; then
  rm "$BINARY_DIR/tests/$TEST_NAME/client.plugins.d" || exit 1
fi

if ! ln -sf "$RUNDIR/client.plugins.d" "$BINARY_DIR/tests/$TEST_NAME/client.plugins.d"; then
  echo "Failed to create client plugins link; cannot run test"
  exit 1
fi

####
#    Starting XRootD config with S3 backend
####

export XROOTD_CONFIG="$XROOTD_CONFIGDIR/xrootd.cfg"
BUCKET_NAME=test-bucket
cat > "$XROOTD_CONFIG" <<EOF

all.trace    all
http.trace   all
xrd.trace    all
xrootd.trace all
scitokens.trace all

xrd.port any

all.export /
all.sitename  XRootD
all.adminpath $XROOTD_RUNDIR
all.pidpath   $XROOTD_RUNDIR

xrootd.seclib libXrdSec.so

ofs.authorize 1

ofs.authlib ++ libXrdAccSciTokens.so config=$XROOTD_CONFIGDIR/scitokens.cfg
acc.authdb $XROOTD_CONFIGDIR/authdb

xrd.protocol XrdHttp:any libXrdHttp.so
http.header2cgi Authorization authz

xrd.tlsca certfile $MINIO_CERTSDIR/CAs/tlsca.pem
xrd.tls $MINIO_CERTSDIR/public.crt $MINIO_CERTSDIR/private.key

oss.local_root /

pss.setopt DebugLevel 4
ofs.osslib libXrdPss.so
pss.origin s3://${HOST}:$MINIO_PORT

#s3.begin
#s3.path_name /test
#s3.bucket_name $BUCKET_NAME
#s3.service_url $MINIO_URL
#s3.service_name ${HOST}
#s3.url_style path
#s3.region us-east-1
#s3.access_key_file $XROOTD_CONFIGDIR/access_key
#s3.secret_key_file $XROOTD_CONFIGDIR/secret_key
#s3.end

EOF

cat > "$XROOTD_CONFIGDIR/authdb" <<EOF
u * / lr
EOF

# Setup the scitokens configuration file
cat > "$XROOTD_CONFIGDIR/scitokens.cfg" << EOF

[Global] 
audience = https://localhost:9443

[Issuer Localhost]
issuer = https://localhost:9443
base_path = /test-bucket

EOF

echo "$MINIO_USER" > $XROOTD_CONFIGDIR/access_key
echo "$MINIO_PASSWORD" > $XROOTD_CONFIGDIR/secret_key

export XRDCLS3_ACCESSKEYLOCATION="$RUNDIR/access_key"
export XRDCLS3_SECRETKEYLOCATION="$RUNDIR/secret_key"
export XRDCLS3_URLSTYLE=path
export X509_CERT_FILE=$MINIO_CERTSDIR/CAs/tlsca.pem
export ASAN_OPTIONS=detect_odr_violation=0
export LD_LIBRARY_PATH="${XROOTD_LIBDIR}:$LD_LIBRARY_PATH"
"$XROOTD_BIN" -c "$XROOTD_CONFIG" -l "$BINARY_DIR/tests/$TEST_NAME/server.log" 0<&- 2>>"$BINARY_DIR/tests/$TEST_NAME/server.log" >>"$BINARY_DIR/tests/$TEST_NAME/server.log" &
XROOTD_PID=$!
echo "xrootd daemon PID: $XROOTD_PID"

XROOTD_URL=$(grep "Xrd_ProtLoad: enabling port" "$BINARY_DIR/tests/$TEST_NAME/server.log" | grep 'for protocol XrdHttp' | awk '{print $7}')
IDX=0
while [ -z "$XROOTD_URL" ]; do
  sleep 1
  XROOTD_URL=$(grep "Xrd_ProtLoad: enabling port" "$BINARY_DIR/tests/$TEST_NAME/server.log" | grep 'for protocol XrdHttp' | awk '{print $7}')
  IDX=$(($IDX+1))
  if ! kill -0 "$XROOTD_PID" 2>/dev/null; then
    echo "xrootd process (PID $XROOTD_PID) failed to start" >&2
    echo "Server log:"
    cat "$BINARY_DIR/tests/$TEST_NAME/server.log"
    echo "End server log"
    exit 1
  fi
  if [ $IDX -gt 1 ]; then
    echo "Waiting for xrootd to start ($IDX seconds so far) ..."
  fi
  if [ $IDX -eq 20 ]; then
    echo "xrootd failed to start - failing"
    exit 1
  fi
done
XROOTD_URL="https://${HOST}:$XROOTD_URL/"
echo "xrootd started at $XROOTD_URL"

cat >> "$BINARY_DIR/tests/$TEST_NAME/setup.sh" <<EOF
XROOTD_PID=$XROOTD_PID
XROOTD_URL=$XROOTD_URL
BUCKET_NAME=$BUCKET_NAME
ORIGIN_URL=$MINIO_URL
X509_CA_FILE=$MINIO_CERTSDIR/CAs/tlsca.pem
CACHE_URL=$XROOTD_URL
WRITE_TOKEN=$RUNDIR/write.token
READ_TOKEN=$RUNDIR/token
EOF
