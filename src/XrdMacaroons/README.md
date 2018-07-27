
Macaroon support for Xrootd
===========================

This plugin adds support for macaroon-style authorizations in XRootD, particularly
for the XrdHttp protocol implementation.

Configuration
=============

To enable, you need to add three lines to the configuration file:

```
ofs.authlib libXrdMacaroons.so
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

Usage
=====

To generate a macaroon for personal use, you can run:

```
macaroon-init https://host.example.com//path/to/directory/ --validity 60 --activity DOWNLOAD,UPLOAD
```

(the `macaroon-init` CLI can be found as part of the `x509-scitokens-issuer-client` package).  This
will generate a macaroon with 60 minutes of validity that has upload and download access to the path
specified at `/path/to/directory`, provided that your X509 identity has that access.

The output will look like the following:

```
Querying https://host.example.com//path/to/directory/ for new token.
Validity: PT60M, activities: DOWNLOAD,UPLOAD,READ_METADATA.
Successfully generated a new token:
{
  "macaroon":"MDAxY2xvY2F0aW9uIFQyX1VTX05lYnJhc2thCjAwMzRpZGVudGlmaWVyIGMzODU3MjQ3LThjYzItNGI0YS04ZDUwLWNiZDYzN2U2MzJhMQowMDUyY2lkIGFjdGl2aXR5OlJFQURfTUVUQURBVEEsVVBMT0FELERPV05MT0FELERFTEVURSxNQU5BR0UsVVBEQVRFX01FVEFEQVRBLExJU1QKMDAyZmNpZCBhY3Rpdml0eTpET1dOTE9BRCxVUExPQUQsUkVBRF9NRVRBREFUQQowMDM2Y2lkIHBhdGg6L2hvbWUvY3NlNDk2L2Jib2NrZWxtL3RtcC94cm9vdGRfZXhwb3J0LwowMDI0Y2lkIGJlZm9yZToyMDE4LTA2LTE1VDE4OjE5OjI5WgowMDJmc2lnbmF0dXJlIFXI_x3v8Tq1jYcP-2WUvPV-BIewn5MHRODVu8UszyYkCg"
}
```

The contents of the `macaroon` key is your new security token.  Anyone you share it with will be able to read and write from the same path.
You can use this token as a bearer token for HTTPS authorization.  For example, it can authorize the following transfer:

```
curl -v
     -H 'Authorization: Bearer MDAxY2xvY2F0aW9uIFQyX1VTX05lYnJhc2thCjAwMzRpZGVudGlmaWVyIGMzODU3MjQ3LThjYzItNGI0YS04ZDUwLWNiZDYzN2U2MzJhMQowMDUyY2lkIGFjdGl2aXR5OlJFQURfTUVUQURBVEEsVVBMT0FELERPV05MT0FELERFTEVURSxNQU5BR0UsVVBEQVRFX01FVEFEQVRBLExJU1QKMDAyZmNpZCBhY3Rpdml0eTpET1dOTE9BRCxVUExPQUQsUkVBRF9NRVRBREFUQQowMDM2Y2lkIHBhdGg6L2hvbWUvY3NlNDk2L2Jib2NrZWxtL3RtcC94cm9vdGRfZXhwb3J0LwowMDI0Y2lkIGJlZm9yZToyMDE4LTA2LTE1VDE4OjE5OjI5WgowMDJmc2lnbmF0dXJlIFXI_x3v8Tq1jYcP-2WUvPV-BIewn5MHRODVu8UszyYkCg' \
     https://host.example.com//path/to/directory/hello_world
```
