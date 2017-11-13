# HTTPS Third Party Copy for XRootD

The `xrootd-tpc` module provides an implementation of HTTPS third-party-copy
for the HTTPS implementation inside XRootD.

To enable, set the following in the configuration file:

```
http.exthandler xrdtpc libXrdHttpTPC.so
```
