# XRootD support for Macaroons

This plugin adds support for macaroons over HTTP in XRootD.

## Introduction

Macaroons are a type of cryptographic authorization token designed to provide
flexible, decentralized access control. They were introduced by
[Google researchers](https://research.google/pubs/pub41892/) as an alternative
to traditional bearer tokens and OAuth-style access tokens, with a focus on
delegation and attenuation of privileges.

Unlike opaque tokens, macaroons embed structured *caveats* that restrict what
the token can do. These restrictions can be added by different parties without
needing to contact a central authority, making them especially useful in
distributed systems. Caveats are always monotonic: they can only reduce
permissions, never increase them.

### Caveats

Caveats take the form `KEY:VALUE`. In XRootD, the following caveats are
supported:

- `name`: this is the identity of the user that obtained the initial macaroon
- `path`: a path restriction such that the macaroon only allows access within it
- `activity`: pre-defined set of allowed activities for the given macaroon
- `before`: defines the date/time when the macaroon expires and becomes invalid

XRootD supports the following activity types: `READ_METADATA`, `UPDATE_METADATA`,
`LIST`, `DOWNLOAD`, `UPLOAD`, `MANAGE`, and `DELETE`. Below are the operations
that each of the activities allow:

- `READ_METADATA`: `stat`
- `UPDATE_METADATA`: `chmod`, `chown`
- `LIST`: `readdir`
- `DOWNLOAD`: `read`
- `UPLOAD`: `rename`, `create`, `insert`
- `MANAGE`: `insert`, `lock`, `mkdir`, `update`, `create`, `overwrite`
- `DELETE`: `rm`, `rmdir`

### Requesting a macaroon with limited permissions

In order to request a macaroon with restricted ability relative to what
the user requesting it can obtain, a list of activities can be passed in
in the `POST` request, like in the example below:

```json
{
    "caveats": [
        "activity:DOWNLOAD,LIST"
    ],
    "validity": "PT1H"
}
```

Unsupported caveats are rejected by the server. A macaroon can be used to
request another macaroon which is further limited in its privileges.

## Obtaining and using a macaroon

To obtain a macaroon, the user can use `curl` or other tool to issue a `POST`
request, like so:

```
curl -X POST -d '{ "validity": "PT1M" }' https://example.org/path/to/file
```

The above will result in a JSON response object containing the macaroon and in
how many seconds it expires (60 seconds, as requested in the `validity` above):

```json
{
  "macaroon":"MDAwZmxvY<...>A6amTbHX7qisDoeEI-P3uQrJfFBCZHPIANixFhgO682wo",
  "expires_in":60
}
```

Then, to use the macaroon, use the value of the `macaroon` element in the JSON
object as your bearer token, like so:

```
export MACAROON="MDAwZmxvY<...>A6amTbHX7qisDoeEI-P3uQrJfFBCZHPIANixFhgO682wo"
curl --header "Authorization: Bearer $MACAROON" https://example.org/path/to/file
```

## Configuration

In order to enable support for macaroons in your XRootD server/cluster, the
following lines are required:

```
all.sitename <sitename>
ofs.authlib libXrdMacaroons.so
http.exthandler xrdmacaroons libXrdMacaroons.so
macaroons.secretkey /etc/xrootd/macaroon-secret
```

where `<sitename>` is the name of your site (it must be set to use macaroons).
The site name is used to set the macaroon's standard `location` field.

The `macaroons.secretkey` is what is used by the server as symmetric key to sign
the macaroons it emits, so make sure it's not too short. The macaroons library
defines a suggested secret length of 32 bytes, but we recommend using at least
64 bytes for your secret keys used with XRootD. You can use any string for this.
However, the recommended best practice is to generate a random key with openssl,
as shown below:

```
openssl rand -base64 -out /etc/xrootd/macaroon-secret 64
```

## References

- [Macaroons: Cookies with Contextual Caveats for Decentralized Authorization in the Cloud](http://research.google.com/pubs/pub41892.html)
- [Macaroons at the NDSS Symposium 2014](https://www.ndss-symposium.org/ndss2014/ndss-2014-programme/macaroons-cookies-contextual-caveats-decentralized-authorization-cloud/)
- [Mozilla Macaroon Tech Talk](https://air.mozilla.org/macaroons-cookies-with-contextual-caveats-for-decentralized-authorization-in-the-cloud/)
- [libmacaroons](https://github.com/rescrv/libmacaroons)
