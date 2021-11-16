# XrdClProxyPrefix Plugin

This XRootD Client Plugin can be used to tunnel traffic through an XRootD Proxy machine. The proxy endpoint is specified as an environment variable. To enable this plugin the **XRD_PLUGIN** environment variable needs to point to the **libXrdClProxyPlugin.so** library.

For example:

```bash
XRD_PLUGIN=/usr/lib64/libXrdClProxyPlugin.so          \
XROOT_PROXY=root://esvm000:2010//                     \
xrdcp -f -d 1 root://esvm000//tmp/file1.dat /tmp/dump
[1.812kB/1.812kB][100%][==================================================][1.812kB/s]
```

This will first redirect the client to the XRootD server on port 2010 which is a forwarding proxy and then the request will be served by the default XRootD server on port 1094.

The user can also specify a list of exclusion domains, for which the original URL will not be modified even if the plugin is enabled. For example:

```bash
XRD_PLUGIN=/usr/lib64/libXrdClProxyPlugin.so                        \
XROOT_PROXY=root://esvm000.cern.ch:2010//                           \
XROOT_PROXY_EXCL_DOMAINS="some.domain, some.other.domain, cern.ch " \
xrdcp -f -d 1 root://esvm000.cern.ch//tmp/file1.dat /tmp/dump
```

This will not redirect the traffic since the original url "root://esmv000.cern.ch//" contains the "cern.ch" domain which is in the list of excluded domains. There are several environment variables that control the behaviour of this XRootD Client plugin:

**XROOT_PROXY/xroot_proxy** - XRootD endpoint through which all traffic is tunnelled

**XROOT_PROXY_EXCL_DOMAINS** - list of comma separated domains which are excluded from being tunnelled through the proxy endpoint

**XRD_PLUGIN** - default environment variable used by the XRootD Client plugin loading mechanism which needs to point to the library implementation of the plugin
