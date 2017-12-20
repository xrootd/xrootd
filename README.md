# HTTPS Third Party Copy for XRootD

The `xrootd-tpc` module provides an implementation of HTTPS third-party-copy
for the HTTPS implementation inside XRootD.

To enable, set the following in the configuration file:

```
http.exthandler xrdtpc libXrdHttpTPC.so
```


## HTTPS TPC technical details.

Third-party-copy is done using the standard WebDAV copy verb.  The actors involved are:

1.  The client orchestrating the transfer.
2.  The source server where the resource is read from.
3.  The destination server where the resource is written to.

The client initiates the third party copy by issuing a COPY request to _either_ the source or destination
endpoint.  Typically, when this is done, the initial endpoint redirects the client to a second service
(the disk server) that will performs the actual transfer.

When the client contacts the source server, it should set the `Destination` header so the source knows
where to put the data.  Analogously, if the client first contacts the destination server, it should set
the `Source` header.

For the former case, the interaction looks like this:

```
-> COPY /sfn/of/source/replica HTTP/1.1
   Destination: https://<dest-endpoint>/pfn/of/dest/replica
<- HTTP/1.1 302 Found
   Location: https://<source-disk-server>/pfn/of/source/replica
```

where `<source-disk-server>` is the service that will actually perform the transfer.

The client would follow the redirection and retry the `COPY`:

```
-> COPY /sfn/of/source/replica HTTP/1.1
   Destination: https://<dest-endpoint>/pfn/of/dest/replica
<- HTTP/1.1 201 Created
   Transfer-Encoding: chunked
```

The source server *should* respond with an appropriate status code (such as 201) in a timely manner.
As a TPC can take a significant amount of time, the source server SHOULD NOT wait until the transfer is
finished before responding with a success.  In the case when a transfer can be started, it is recommended
that the response be started as soon as possible.

In the case of a transfer being started (or queued) by the source server, it SHOULD use chunked encoding
and send a multipart response.

Next `<source-disk-server>` will connect to the destination endpoint and actually perform the `PUT`:

```
-> PUT /pfn/of/destination/replica HTTP/1.1
<- HTTP/1.1 201 Created
```

As the PUT is ongoing, the source disk server should send back a periodic transfer chunk of the following
form:

```
Perf Marker
	Timestamp: 1360578938
	Stripe Index: 0
	Stripe Bytes Transferred: 49397760
	Total Stripe Count: 1
End
```

Timestamps should be seconds from Unix epoch.  It is recommended that the time period between chunks be
less than 30 seconds.

If the transfer ultimately succeeds, then the last chunk should be of the following form:

```
success: Created
```

Here, `success` indicates the transfer status: subsequent text is informational for the user.

Failure of the transfer can be indicated with the prefix `failed` or `failure`:

```
failure: Network connection unexpectedly closed.
```

Finally, if the source disk server decides to cancel or abort the transfer, use the `aborted` prefix:

```
aborted: Transfer took too long.
```

There is an analogous case when the client contacts the destination server to perform the `COPY` and
sets the `Source` header.  In such a case, the response to the client looks identical but the destination
server will perform a `GET` instead of a `PUT`.

In some cases, the user may want the server performing the transfer to append additional headers
(such as an HTTP `Authorization` header) to the transfer it initiates.  In such case, the client should
utilize the `TransferHeader` mechanism.  Add a header of the form:

```
TransferHeader<header> : <value>
```

For example, if the client sends the following to the source server as part of its `COPY` request:

```
TransferHeaderAuthorization: Basic cGF1bDpwYXNzd29yZA==
```

then the source server should set the following header as part of its `PUT` request to the destination:

```
Authorization: Basic cGF1bDpwYXNzd29yZA==
```
