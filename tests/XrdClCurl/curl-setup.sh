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

if [ -z "$XROOTD_BINDIR" ]; then
  echo "\$XROOTD_BINDIR environment variable is not set; cannot run test"
  exit 1
fi

if [ -z "$XROOTD_LIBDIR" ]; then
  echo "\$XROOTD_LIBDIR environment variable is not set; cannot run test"
  exit 1
fi

#######################################
# Create X509 certificates for server #
#######################################
echo "Setting up X509 certificates for $TEST_NAME test"
if [ -z "$OPENSSL_BIN" ]; then
  echo "openssl binary not found; cannot create X509 certificates"
  exit 1
fi

CA_DIR="$BINARY_DIR/tests/$TEST_NAME/ca"
rm -rf "$CA_DIR"
mkdir -p "$CA_DIR"
cd "$CA_DIR" || exit 1

if ! "$OPENSSL_BIN" genrsa -out tlscakey.pem 4096; then
  echo "Failed to generate CA private key"
  exit 1
fi

mkdir -p state
touch state/index.txt
echo '01' > state/serial.txt

cat > tlsca.ini << EOF

[ ca ]
default_ca = CA_test

[ CA_test ]

default_days = 365
default_md = sha256
private_key = $CA_DIR/tlscakey.pem
certificate = $CA_DIR/tlsca.pem
new_certs_dir = $CA_DIR/state
database = $CA_DIR/state/index.txt
serial = $CA_DIR/state/serial.txt

[ req ]
default_bits = 4096
distinguished_name = ca_test_dn
x509_extensions = ca_extensions
string_mask = utf8only

[ ca_test_dn ]

commonName_default = XrdClCurl CA

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
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
DNS.2 = blah-cache.example.com

EOF

# Create the CA certificate
echo "Creating TLS CA certificate in $CA_DIR/tlsca.pem"
if ! "$OPENSSL_BIN" req -x509 -key tlscakey.pem -config tlsca.ini -out tlsca.pem -outform PEM -subj "/CN=XrdClCurl CA" 0<&-; then
  echo "Failed to generate CA request"
  exit 1
fi

# Create the host certificate request
openssl genrsa -out tls.key 4096
chmod 0400 tls.key
if ! "$OPENSSL_BIN" req -new -key tls.key -config tlsca.ini -out tls.csr -outform PEM -subj /CN=localhost 0<&-; then
  echo "Failed to generate host certificate request"
  exit 1
fi

if ! "$OPENSSL_BIN" ca -config tlsca.ini -batch -policy signing_policy -extensions cert_extensions -out tls.crt -infiles tls.csr 0<&-; then
  echo "Failed to sign host certificate request"
  exit 1
fi
chmod 0600 tls.crt

cd "$OLDPWD" || exit 1

######################
# Setup token issuer #
######################
RUNDIR="$BINARY_DIR/tests/$TEST_NAME"
echo "Using $RUNDIR as the test run's home directory."
cd "$RUNDIR" || exit 1

XROOTD_EXPORTDIR="$RUNDIR/export"
rm -rf "$XROOTD_EXPORTDIR"
mkdir -p "$XROOTD_EXPORTDIR/test" || exit 1
XROOTD_PUBLIC_EXPORTDIR="$XROOTD_EXPORTDIR/test-public"
mkdir -p "$XROOTD_PUBLIC_EXPORTDIR" || exit 1

mkdir -p "$XROOTD_EXPORTDIR"/.well-known || exit 1
cp "$SOURCE_DIR/tests/XrdClCurl/openid-configuration" "$XROOTD_EXPORTDIR/.well-known/openid-configuration" || exit 1

if ! "$OPENSSL_BIN" ecparam -name prime256v1 -genkey -noout -out "issuer_private.pem"; then
  echo "Failed to generate EC private key"
  exit 1
fi

if ! "$OPENSSL_BIN" ec -in "issuer_private.pem" -pubout -out "issuer_public.pem"; then
  echo "Failed to generate EC public key"
  exit 1
fi

if ! "$BINARY_DIR/bin/xrdscitokens-create-jwks" "issuer_public.pem" "$XROOTD_EXPORTDIR/.well-known/issuer.jwks" "test_key"; then
  echo "Failed to generate JWKS file"
  exit 1
fi

# Prevent the issuer's keys from being cached by an earlier test run
XDG_CACHE_HOME="$RUNDIR/tmp"
rm -rf "$XDG_CACHE_HOME"
mkdir -p "$XDG_CACHE_HOME" || exit 1
export XDG_CACHE_HOME

# Ubuntu 22 (currently used for code coverage) has an old version of libscitokens-cpp that doesn't support
# custom CAs.  Pre-load the JWKS to prevent the library from attempting a download (and subsequent TLS failure).
mkdir -p "$RUNDIR/issuer"
cat > "$RUNDIR/issuer/sql" << EOF
BEGIN TRANSACTION;
CREATE TABLE keycache (issuer text UNIQUE PRIMARY KEY NOT NULL,keys text NOT NULL);
INSERT INTO keycache VALUES('https://localhost:9443','{"expires":$(($(date '+%s') + 3600)),"jwks":$(cat "$XROOTD_EXPORTDIR/.well-known/issuer.jwks"),"next_update":$(($(date '+%s') + 3600))}');
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

#######################################
# Setup XRootD runtime environment    #
#######################################

# Create the plugin configuration to utilize the freshly-built plugin
export XRD_PLUGINCONFDIR="$RUNDIR/client.plugins.d"
mkdir -p "$XRD_PLUGINCONFDIR"

PLUGIN_SUFFIX=so
if [ "$(uname)" = Darwin ]; then
  PLUGIN_SUFFIX=dylib
fi

cat > "$XRD_PLUGINCONFDIR/curl-plugin.conf" <<EOF

url = https://*
lib = $BINARY_DIR/lib/libXrdClCurlTesting.$PLUGIN_SUFFIX
enable = true

EOF

# Create configuration and runtime directory structure
# Note that XRootD places unix sockets in the rundirs; these have strict length
# limits.  Hence, we explicitly place the XRootD rundir in /tmp
XROOTD_RUNDIR="$(mktemp -d -p /tmp xrdcl-curl-rundir.XXXXXXXX)"
chmod 0755 "$XROOTD_RUNDIR"
mkdir -p "$XROOTD_RUNDIR/cache" "XROOTD_RUNDIR/origin"
XROOTD_CONFIGDIR="$RUNDIR/xrootd"
rm -rf "$XROOTD_CONFIGDIR"
mkdir -p "$XROOTD_CONFIGDIR"
export ORIGIN_CONFIG="$XROOTD_CONFIGDIR/xrootd-origin.conf"
cat > "$ORIGIN_CONFIG" <<EOF

xrd.port 9443
xrd.protocol http:9443 libXrdHttp.so

xrd.tls $CA_DIR/tls.crt $CA_DIR/tls.key
xrd.tlsca certfile $CA_DIR/tlsca.pem
sec.protbind * none

http.header2cgi Authorization authz

ofs.osslib ++ libXrdOssStats.so
all.adminpath $XROOTD_RUNDIR/origin
all.pidpath $XROOTD_RUNDIR/origin

oss.localroot $XROOTD_EXPORTDIR

xrootd.seclib libXrdSec.so
ofs.authorize 1
acc.audit deny grant
acc.authdb $RUNDIR/authdb
acc.authrefresh 300
ofs.authlib ++ libXrdAccSciTokens.so config=$RUNDIR/scitokens.cfg

# Tell xrootd to make each namespace we export available as a path at the server
all.export /
xrootd.fslib ++ throttle
xrootd.chksum max 2 md5 adler32 crc32 crc32c
xrootd.trace debug
ofs.trace all
oss.trace all
xrd.trace all
cms.trace debug
http.trace all
xrootd.tls all
xrd.network nodnr
scitokens.trace debug info warning error

ofs.osslib ++ $BINARY_DIR/lib/libXrdOssSlowOpen.so

# Required for the COPY tests
http.exthandler xrdtpc libXrdHttpTPC.so

EOF

XROOTD_CACHEDIR=$RUNDIR/cache
rm -rf "$XROOTD_CACHEDIR"
mkdir -p "$XROOTD_CACHEDIR"/namespace || exit 1
mkdir -p "$XROOTD_CACHEDIR"/meta || exit 1
mkdir -p "$XROOTD_CACHEDIR"/data || exit 1

export CACHE_CONFIG="$XROOTD_CONFIGDIR/xrootd-cache.conf"
cat > "$CACHE_CONFIG" <<EOF

xrd.port any
xrd.protocol http:any libXrdHttp.so
  
xrd.tls $CA_DIR/tls.crt $CA_DIR/tls.key
xrd.tlsca certfile $CA_DIR/tlsca.pem
sec.protbind * none

http.header2cgi Authorization authz

ofs.osslib libXrdPss.so
ofs.ckslib * libXrdPss.so
pss.cachelib libXrdPfc.so

ofs.osslib ++ libXrdOssStats.so
all.adminpath $XROOTD_RUNDIR/cache
all.pidpath $XROOTD_RUNDIR/cache

http.header2cgi Authorization authz

xrootd.seclib libXrdSec.so
sec.protocol ztn
ofs.authorize 1
acc.audit deny grant

acc.authdb $RUNDIR/authdb
ofs.authlib ++ libXrdAccSciTokens.so config=$RUNDIR/scitokens.cfg

all.export /
xrootd.chksum max 2 md5 adler32 crc32 crc32c
xrootd.trace emsg login stall redirect
xrootd.tls all
xrd.network nodnr

pfc.blocksize 128k
pfc.prefetch 0
pfc.writequeue 16 4
pfc.ram 4g
pfc.diskusage 0.90 0.95 purgeinterval 300s

xrootd.fslib ++ throttle

pss.origin https://localhost:9443
oss.localroot $XROOTD_CACHEDIR/namespace
pfc.spaces data meta
oss.space data $XROOTD_CACHEDIR/data
oss.space meta $XROOTD_CACHEDIR/meta

pss.debug
pfc.trace debug
pss.setopt DebugLevel 4
pss.trace on
ofs.trace all
xrd.trace all
xrootd.trace all
scitokens.trace debug info warning error
http.trace all

EOF

# Setup the authfile
cat > "$RUNDIR/authdb" << EOF

u * /test-public lr /.well-known lr

EOF

# Setup the scitokens configuration file
cat > "$RUNDIR/scitokens.cfg" << EOF

[Global]
audience = https://localhost:9443

[Issuer Localhost]
issuer = https://localhost:9443
base_path = /test

EOF

# Export some data through the origin
echo "Hello, World" > "$XROOTD_EXPORTDIR/test/hello_world.txt"
echo "Hello, World" > "$XROOTD_EXPORTDIR/test/slow_open.txt"
echo "Hello, World" > "$XROOTD_PUBLIC_EXPORTDIR/hello_world.txt"

mkdir "$XROOTD_PUBLIC_EXPORTDIR/subdir"
touch "$XROOTD_PUBLIC_EXPORTDIR/subdir/test1"
echo 'Hello, world!' > "$XROOTD_PUBLIC_EXPORTDIR/subdir/test2"
mkdir "$XROOTD_PUBLIC_EXPORTDIR/subdir/test3"

dd if=/dev/urandom of="$XROOTD_PUBLIC_EXPORTDIR/hello_world-1mb.txt" count=$((4 * 1024)) bs=1024
IDX=0
while [ $IDX -ne 100 ]; do
  IDX=$((IDX+1))
  ln -s "$XROOTD_PUBLIC_EXPORTDIR/hello_world-1mb.txt" "$XROOTD_PUBLIC_EXPORTDIR/hello_world-$IDX.txt"
done

####################################################
# Configure xrootd wrapper to have custom env vars #
####################################################
# Until Pelican has been updated to use XRD_PELICANCACHETOKENLOCATION, we inject
# it via a wrapper script
cat > "$RUNDIR/cache_token" << EOF
# Some ignored comment text

  pretend_cache_token

EOF

XROOTD_BIN="$XROOTD_BINDIR/xrootd"

BINDIR="$RUNDIR/bin"
mkdir -p -- "$BINDIR"
cat > "$BINDIR/xrootd" << EOF
#!/bin/sh
export XRD_CURLSLOWRATEBYTESSEC=1024
export XRD_CURLSTALLTIMEOUT=2
export XRD_CURLCERTFILE=$CA_DIR/tlsca.pem
export XRD_LOGLEVEL=Debug
# ODR violations are disabled as XRootD currently has some, preventing
# it from starting up.  See: https://github.com/xrootd/xrootd/issues/2471
export ASAN_OPTIONS=detect_odr_violation=0
export LD_LIBRARY_PATH="${XROOTD_LIBDIR}:$LD_LIBRARY_PATH"
set -x
exec "$XROOTD_BIN" "\$@"
EOF
chmod +x "$BINDIR/xrootd"
export PATH="$BINDIR:$PATH"

# Clear out old client logging
echo > "$BINARY_DIR/tests/$TEST_NAME/client.log"

###########################
# Launch XRootD services. #
###########################
echo > "$BINARY_DIR/tests/$TEST_NAME/origin.log"
"$BINDIR/xrootd" -n origin -c "$ORIGIN_CONFIG" 0<&- >"$BINARY_DIR/tests/$TEST_NAME/origin.log" 2>&1 &
ORIGIN_PID=$!
echo "Origin PID: $ORIGIN_PID"

echo "Origin logs are available at $BINARY_DIR/tests/$TEST_NAME/origin.log"

ORIGIN_PORT=$(grep -E -a '\-\-\-\-\-\- xrootd origin@.*:[0-9]+ initialization completed' "$BINARY_DIR/tests/$TEST_NAME/origin.log" | tr ':' ' ' | awk '{print $4}')
IDX=0
while [ -z "$ORIGIN_PORT" ]; do
  sleep 1
  ORIGIN_PORT=$(grep -E -a '\-\-\-\-\-\- xrootd origin@.*:[0-9]+ initialization completed' "$BINARY_DIR/tests/$TEST_NAME/origin.log" | tr ':' ' ' | awk '{print $4}')
  IDX=$((IDX+1))
  if [ $IDX -gt 1 ]; then
    echo "Waiting for origin to start ($IDX seconds so far) ..."
  fi
  if ! kill -0 "$ORIGIN_PID" 2>/dev/null; then
    cat "$BINARY_DIR/tests/$TEST_NAME/origin.log"
    echo "Origin process crashed - failing"
    exit 1
  fi
  if [ $IDX -eq 50 ]; then
    cat "$BINARY_DIR/tests/$TEST_NAME/origin.log"
    echo "Origin failed to start - failing"
    exit 1
  fi
done
echo "Origin started at port $ORIGIN_PORT"

echo > "$BINARY_DIR/tests/$TEST_NAME/cache.log"
"$BINDIR/xrootd" -n cache -c "$CACHE_CONFIG" 0<&- >"$BINARY_DIR/tests/$TEST_NAME/cache.log" 2>&1 &
CACHE_PID=$!
echo "Cache PID: $ORIGIN_PID"

echo "Cache logs are available at $BINARY_DIR/tests/$TEST_NAME/cache.log"

CACHE_PORT=$(grep -E -a '\-\-\-\-\-\- xrootd cache@.*:[0-9]+ initialization completed' "$BINARY_DIR/tests/$TEST_NAME/cache.log" | tr ':' ' ' | awk '{print $4}')
IDX=0
while [ -z "$CACHE_PORT" ]; do
  sleep 1
  CACHE_PORT=$(grep -E -a '\-\-\-\-\-\- xrootd cache@.*:[0-9]+ initialization completed' "$BINARY_DIR/tests/$TEST_NAME/cache.log" | tr ':' ' ' | awk '{print $4}')
  IDX=$((IDX+1))
  if [ $IDX -gt 1 ]; then
    echo "Waiting for cache to start ($IDX seconds so far) ..."
  fi
  if ! kill -0 "$CACHE_PID" 2>/dev/null; then
    cat "$BINARY_DIR/tests/$TEST_NAME/cache.log"
    echo "Cache process crashed - failing"
    exit 1
  fi
  if [ $IDX -eq 50 ]; then
    cat "$BINARY_DIR/tests/$TEST_NAME/cache.log"
    echo "Cache failed to start - failing"
    exit 1
  fi
done
echo "Cache started at port $CACHE_PORT"

if ! "$BINARY_DIR/bin/xrdscitokens-create-token" \
    issuer_public.pem issuer_private.pem test_key \
    https://localhost:9443 storage.read:/ 600 > "$RUNDIR/token"; then
  echo "Failed to generate read token"
  exit 1
fi
echo "Sample read token available at $RUNDIR/token"

if ! "$BINARY_DIR/bin/xrdscitokens-create-token" \
    issuer_public.pem issuer_private.pem test_key \
    https://localhost:9443 storage.modify:/ 600 > "$RUNDIR/write.token"; then
  echo "Failed to generate write token"
  exit 1
fi
echo "Sample write token available at $RUNDIR/write.token"

printf "%s" "Authorization: Bearer " > "$RUNDIR/authz_header"
cat "$RUNDIR/token" >> "$RUNDIR/authz_header"

cat > "$BINARY_DIR/tests/$TEST_NAME/setup.sh" <<EOF
CACHE_PID=$CACHE_PID
ORIGIN_PID=$ORIGIN_PID
ORIGIN_URL=https://localhost:$ORIGIN_PORT
CACHE_URL=https://localhost:$CACHE_PORT
HEADER_FILE=$RUNDIR/authz_header
X509_CA_FILE=$CA_DIR/tlsca.pem
PUBLIC_TEST_FILE=$PELICAN_PUBLIC_EXPORTDIR/hello_world-1mb.txt
WRITE_TOKEN=$RUNDIR/write.token
READ_TOKEN=$RUNDIR/token
XROOTD_RUNDIR=$XROOTD_RUNDIR
EOF

echo "Test environment written to $BINARY_DIR/tests/$TEST_NAME/setup.sh"
