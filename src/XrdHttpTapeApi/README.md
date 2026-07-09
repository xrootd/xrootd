# XrdHttpTapeApi

`XrdHttpTapeApi` is an XrdHttp external handler for WLCG Tape REST API
requests.

The initial implementation provides a deterministic in-process Tape REST API
surface used by the XrdClHttp Tape client integration tests. It is built and
installed as a regular server plugin so the handler can evolve into a real
server-side Tape REST API implementation.

Example configuration:

```conf
http.exthandler xrdhttptapeapi libXrdHttpTapeApi.so
```
