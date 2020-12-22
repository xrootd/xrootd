#!/bin/bash -xe

OS_VERSION=$1

# Clean the yum cache
yum -y clean all
yum -y clean expire-cache

# First, install all the needed packages.
rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-${OS_VERSION}.noarch.rpm

yum -y install yum-plugin-priorities rpm-build gcc gcc-c++ boost-devel boost-python cmake git tar gzip make autotools python-devel

rpm -Uvh https://repo.opensciencegrid.org/osg/3.5/osg-3.5-el${OS_VERSION}-release-latest.rpm

yum -y install xrootd-server-devel
yum -y install --enablerepo=osg-development scitokens-cpp-devel

# Prepare the RPM environment
mkdir -p /tmp/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

cp xrootd-scitokens/rpm/xrootd-scitokens.spec /tmp/rpmbuild/SPECS

package_version=`grep Version xrootd-scitokens/rpm/xrootd-scitokens.spec | awk '{print $2}'`
pushd xrootd-scitokens
git archive --format=tar --prefix=xrootd-scitokens-${package_version}/ HEAD | \
    gzip > /tmp/rpmbuild/SOURCES/xrootd-scitokens-${package_version}.tar.gz
popd

# Build the RPM
rpmbuild --define '_topdir /tmp/rpmbuild' -ba /tmp/rpmbuild/SPECS/xrootd-scitokens.spec

# After building the RPM, try to install it
# Fix the lock file error on EL7.  /var/lock is a symlink to /run/lock
mkdir -p /run/lock

RPM_LOCATION=/tmp/rpmbuild/RPMS/x86_64

# Enable osg-development repo for new python-jwt
yum localinstall --enablerepo=osg-development -y $RPM_LOCATION/xrootd-scitokens-${package_version}*

# Stand up a web server to server the public key
yum -y install httpd mod_ssl xrootd-server

# Need this for scitokens-admin-create-key
yum -y install python2-scitokens

# Create the public and private key
scitokens-admin-create-key --create-keys --pem-private > private.pem
mkdir /var/www/html/oauth2
scitokens-admin-create-key --private-keyfile private.pem --jwks-public > /var/www/html/oauth2/certs
mkdir /var/www/html/.well-known
cat << EOF > /var/www/html/.well-known/openid-configuration
{  
   "issuer":"https://localhost/",
   "jwks_uri":"https://localhost/oauth2/certs"
}
EOF

# Create the self signed x509 cert so we can use https (required by scitokens)
openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout localhost.key -out localhost.crt -config xrootd-scitokens/test/openssl-selfsigned.conf -subj '/CN=localhost/O=SciTokens/C=US'
cp localhost.crt /etc/ssl/certs/localhost.crt
cat localhost.crt >> /etc/ssl/certs/ca-bundle.crt
mkdir -p /etc/ssl/private
cp localhost.key /etc/ssl/private/localhost.key

cat << EOF > /etc/httpd/conf.d/scitokens.conf

<VirtualHost _default_:443>
DocumentRoot /var/www/html
    
SSLEngine on
SSLCertificateFile /etc/ssl/certs/localhost.crt
SSLCertificateKeyFile /etc/ssl/private/localhost.key
SSLCertificateChainFile /etc/ssl/certs/localhost.crt
</VirtualHost>
EOF

systemctl restart httpd

# Put configs into place
cp -f xrootd-scitokens/test/config/xrootd-http.cfg /etc/xrootd/xrootd-http.cfg
cp -f xrootd-scitokens/test/config/scitokens-no-aud.cfg /etc/xrootd/scitokens.cfg

mkdir /etc/systemd/system/xrootd@http.service.d/
cp xrootd-scitokens/test/config/override.conf /etc/systemd/system/xrootd@http.service.d/override.conf

systemctl daemon-reload
systemctl restart xrootd@http.service

# Generate a random file
NEW_UUID=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 32 | head -n 1)
echo $NEW_UUID > /tmp/random.txt

py_output=$(python xrootd-scitokens/test/create-pubkey.py /var/www/html/oauth2/certs)

if [ "$py_output" != "$NEW_UUID" ]; then
  exit 1
fi

# Test sending aud when there is no audience configured on the server
if python xrootd-scitokens/test/create-pubkey.py --aud="testing.com" /var/www/html/oauth2/certs; then
  exit 1
fi


# Test single aud
cp -f xrootd-scitokens/test/config/scitokens-aud.cfg /etc/xrootd/scitokens.cfg
systemctl restart xrootd@http.service

NEW_UUID=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 32 | head -n 1)
echo $NEW_UUID > /tmp/random.txt

py_output=$(python xrootd-scitokens/test/create-pubkey.py --aud="testing.com" /var/www/html/oauth2/certs)

if [ "$py_output" != "$NEW_UUID" ]; then
  exit 1
fi

# Test sending no aud when an audience is configured
if python xrootd-scitokens/test/create-pubkey.py /var/www/html/oauth2/certs; then
  exit 1
fi

# Test multiple aud
cp -f xrootd-scitokens/test/config/scitokens-multi-aud.cfg /etc/xrootd/scitokens.cfg
systemctl restart xrootd@http.service

NEW_UUID=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 32 | head -n 1)
echo $NEW_UUID > /tmp/random.txt

py_output=$(python xrootd-scitokens/test/create-pubkey.py --aud="testing.com" /var/www/html/oauth2/certs)

if [ "$py_output" != "$NEW_UUID" ]; then
  exit 1
fi

py_output=$(python xrootd-scitokens/test/create-pubkey.py --aud="https://another.com" /var/www/html/oauth2/certs)

if [ "$py_output" != "$NEW_UUID" ]; then
  exit 1
fi

# Test sending no aud when an audience is configured
if python xrootd-scitokens/test/create-pubkey.py /var/www/html/oauth2/certs; then
  exit 1
fi

# Test sending wrong aud when an audience is configured
if python xrootd-scitokens/test/create-pubkey.py --aud="wrong.com" /var/www/html/oauth2/certs; then
  exit 1
fi





