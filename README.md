
Macaroon support for Xrootd
===========================

*WARNING*: This plugin is a work-in-progress and not all functionality described
is present.

This plugin adds support for macaroon-style authorizations in XRootD, particularly
for the XrdHttp protocol implementation.

To enable, you need to add three lines to the configuration file:

```
http.exthandler xrdmacaroons libXrdMacaroons.so
macaroons.secretkey /etc/xrootd/macaroon-secret
all.sitename Example_Site
```

You will need to change `all.sitename` accordingly.  The secret key is a symmetric
key necessary to verify macaroons; the same key must be deployed to all XRootD
servers in your cluster.

The secret key must be base64-encoded.  The most straightforward way to generate
this is the following:

```
openssl rand -base64 -out /etc/xrootd/macaroon-secret 64
```
