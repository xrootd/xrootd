
XrdCeph
============

The XrdCeph Plugin allows XRootD to access a ceph object store directly (trough the RADOS ceph interface) and provides a posix-like frontend for such storges to work without mounting a cephFS frontend into the server. 
Writing a file is done with the libradosstriper library for higher performance over large files, while reading can be optionally configured to use RADOS directly, improving performance.

This provides significant performance and stability benefits, especially for large distributed storages (>30PB), as it avoids the metadata overhead to maintain folder structures storage side.

Configuration
-------------

The following configuration options are available:

Required for enabling the plugin:
```
ofs.osslib +cksio /usr/lib64/libXrdCeph.so user@,nbStripes,stripeUnit,objectSize 
ofs.xattrlib /usr/lib64/libXrdCephXattr.so
```

These parameters set the default values for the ceph operations.
user - username for the file operations, usually xrootd 
nbStripes - number of stripes per file, recommended value 1
stripeUnit - size of stripes
objectSize - size of objects

Config Options:
```
ceph.buffermaxpersimul N # max number of simultaneous buffers, default 10
ceph.nbconnections N # max number of handlers to the cluster (max 100)
ceph.usedefaultpreadalg [0/1] # flag to enable default readv algorithm
ceph.aiowaitthresh N # aio wait timeout
ceph.usebuffer [0/1] # flag if to use IO buffers when interacting with storage
ceph.buffersize N # size of the buffer in bytes (max 1000000000)
ceph.buffermaxpersimul # size of the buffer in bytes for multibuffers (max 1000000000)
ceph.usereadv [0/1] # use optimized readv code with direct IO
ceph.readvalgname <ALGNAME> # select readv algorithm, recommended passtrough
ceph.bufferiomode [aio/io] # select buffer io mode, recommended io
ceph.reportingpools <LIST OF POOLS> # select pools where data metrics are enabled (for spaceinfo queries)
ceph.namelib NAMELIB file:NAMELIB_CONF?protocol=PROT_LIST # XrdCeph namelib option to enable name2name mapping (XrdCmstfc)
ceph.streamed-cks-adler32 [calc/log/store] # enable streamed checksums and either calculate them (calc), log them as well (log) or log and store them into metadata (store)
ceph.streamed-cks-logfile LOG_FILE_PATH # set up logfile location for streamed checksum log and store modes
```



Additional configs needed
---------------------------
A ceph client keyring and config file must be present in the server's /etc/ceph/ location to allow connection to the cluster.
The xrootd client keyring is usually named ceph.client.xrootd.keyring and sets the access to the pools for the xrootd user.

