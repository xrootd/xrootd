# CORS Plugin

Provides HTTP CORS (Cross Origin Resource Sharing) functionality.
For now, the plugin only supports Access-Allow-Control-Origin headers.

## Configuration

Enable the CORS plugin via the XRootD server configuration file:
```
http.cors libXrdHttpCors.so
```

## Trusted origins

### Add multiple trusted origins

In the XRootD server configuration file, add trusted origins like the following:

```
cors.origin https://myhttpserver1.cern.ch
cors.origin https://myhttpserver2.cern.ch
```

One can also merge the two lines into a single one:

```
cors.origin https://myhttpserver1.cern.ch https://myhttpserver2.cern.ch
```

### Processing expectation

Any HTTP request towards an XRootD HTTP server with CORS configured will receive an 'Access-Allow-Control-Origin: <trusted_origin>' header in all the server replies, provided the `Origin` header matches
one of the trusted origin.

Example:

```
> PUT /tmp/file.txt HTTP/1.1
> Host: xrootd.server.ch:1096
> User-Agent: curl/7.76.1
> Accept: */*
> Origin: https://myhttpserver1.cern.ch
> Content-Length: 5242880
> Expect: 100-continue
> 

< HTTP/1.1 100 Continue
< Connection: Close
< Server: XrootD/v5.7.1-22-g82a3fc0e8
< Access-Control-Allow-Origin: https://myhttpserver1.cern.ch
```