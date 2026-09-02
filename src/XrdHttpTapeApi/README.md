# XrdHttpTapeApi

`XrdHttpTapeApi` is an XrdHttp external handler for WLCG Tape REST API
requests.

The initial implementation provides a deterministic, filesystem-backed Tape
REST API simulator for the XrdClHttp Tape client integration tests. It is not
a production tape backend. Stage operations complete synchronously by copying
files from the simulated archive into the disk namespace.

The handler requires a state directory. It creates this layout below it:

```text
archive/   files available on the simulated tape backend
disk/      staged files served by XRootD
requests/  persisted stage request metadata
```

Configure the XRootD local namespace to use the same `disk` directory and pass
the state directory to the handler:

```conf
oss.localroot /var/lib/xrootd-tape-api/disk
http.exthandler xrdhttptapeapi libXrdHttpTapeApi.so /var/lib/xrootd-tape-api 4m
```

The first handler parameter is the state directory. The optional second
parameter limits request bodies and accepts the standard XRootD size suffixes;
it defaults to 4 MiB.

For example, placing an archived test file at
`/var/lib/xrootd-tape-api/archive/store/file` makes `/store/file` report
`TAPE` locality. Staging it creates the corresponding file below `disk/` and
changes its locality to `DISK_AND_TAPE`. Releasing it removes only the disk
copy. Stage request records remain available across server restarts until the
request is deleted through the API.
