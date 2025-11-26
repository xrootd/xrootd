
XrdThrottle: Managing I/O load in XRootD
========================================

The XrdThrottle provides a mechanism for managing I/O load in an XRootD
server.  It is implemented as a stackable "filesystem" by monitoring the
I/O requests that are passed through the storage and, for clients that are
over a set load threshold, delays or produces an error as necessary.
The module has two goals:

- Prevent users from overloading a filesystem through Xrootd.
- Provide a level of fairness between different users.

Here, the "fairness" algorithm permits short spikes over a user's allocation
if the entire server is below the resource usage threshold. The user's I/O usage
is tracked at approximate second intervals (1.0 seconds by default), meaning only
the most recent usage intervals are factored into the calculated share.
Fairness is enforced by trying to delaying IO the same amount *per user*,
regardless of how many open file handles there are.

When loaded, in order for the plugin to perform timings for IO, asynchronous
requests are handled synchronously and mmap-based reads are disabled.

Once a throttle limit is hit, the plugin will start delaying the start of
new IO requests until the server is back below the throttle.  Users under their
limit will be preferentially allowed to start new I/O first.

Loading the Plugin
------------------

To load the plugin, add it to the Open Storage System (OSS) stack:

```
ofs.osslib ++ libXrdThrottle.so
```

The module historically was provided as an OFS plugin and can still be loaded as follows:

```
xrootd.fslib throttle default
```

The historical usage is discouraged as it prevents the plugin from retrieving usernames
iin some cases.

Unless limits are explicitly set, the plugin will only record (and, if configured,
log or send to monitoring) usage statistics.

Throttling Resource Usage
-------------------------

To set a throttle, add a line as follows:

```
throttle.throttle [concurrency CONCUR] [data RATE] [iops IRATE] [interval ITVL_MS]
```

The two options are:

- `CONCUR`: Set the level of IO concurrency allowed.  This works in a similar
  manner to system load in Linux; we sum up the total amount of time spent
  waiting on all IO requests per second.  So, if there are two simultaneous
  requests, each of which take 1 second to service, we have a concurrency of
  2.  If we have 100 simultaneous IO requests, each of which is services in
  1 millisecond, then the IO load is 0.1.
- `RATE`: Limit for the total data rate (MB/s) from the underlying filesystem.
  This number is measured in bytes.
- `IRATE`: Limit for the I/O operations per second for the storage system.  This
  is a poor way to limit disk-based storage systems but may be useful for proxies
  where the upstream (such as various AWS services) that charge per-request.
- `ITVL_MS`: The time, in milliseconds, when the usage statistics are recomputed.
  The default value is 1000 (1.0 seconds) and it is not recommended to be changed.

Notes:
- The throttles are applied to the aggregate of reads and writes; they are not
  considered seperately.
- In almost all cases, service administrators will want to set concurrency, NOT,
  data or IOPS.  Concurrency measures how much work is being done by the filesystem;
  data rate is weak indicator of filesystem activity due to the effects of
  the page cache.  We do not offer the option to limit number of clients or
  IOPS in this plugin for similar reasons: neither metric strongly corresponds
  to filesystem activity (due to idle clients or IO requests serviced from cache).
- As sites commonly run multiple servers, setting the data rate throttle is
  not useful to limit wide area network traffic.  If you want to limit network
  activity, investigate QoS on the site's network router.  Configuring
  the host-level network queueing will be more CPU-efficient than setting the
  data rates from within Xrootd.  The advantage of throttling data rates
  from within Xrootd is being able to provide fairness across users.

When a server is heavily loaded, an I/O request may be heavily delayed before
it is passed to the underlying storage.  This often triggers clients to disconnect,
assuming the server is unresponsive; the result is the server still does the
storage I/O only to find it is unable to send the response to the client.

By default, any delay over 30s results in an error.  This can be changed with
the following setting:

```
throttle.max_wait_time LIMIT_SECS
```

where `LIMIT_SECS` is specified in seconds.

Setting Resource Limits
-----------------------

The plugin can also enforce limits on open files or "active connections"

To limit the active connections (defined as a connection with at least one
open file handle; server connections with no open files are excluded), set

```
throttle.max_active_connections LIMIT
```

where `LIMIT` is an integer value; any user with more than `LIMIT` connections
will be given an error when the exceed the limit.

Additionally, since a user can open multiple files per connection, the administrator
can limit the total open files per user:

```
throttle.max_open_files LIMIT
```

Per-User Connection Limits
---------------------------

The plugin supports per-user connection limits in addition to the global limit.
This allows administrators to set different connection limits for individual users
or groups of users.

To enable per-user limits, specify a configuration file:

```
throttle.userconfig /etc/xrootd/throttle-users.conf
```

The per-user configuration file uses an INI-style format with sections for each
user or user pattern:

```
[default]
name = *
maxconn = 200

[user1]
name = user1
maxconn = 25

[user2]
name = user2
maxconn = 50

[wildcarduser]
name = wildcarduser*
maxconn = 10
```

- The `[default]` section with `name = *` applies to all users who don't match any specific rule
- Each section must have a `name` parameter specifying the user or pattern
- The `maxconn` parameter sets the maximum number of active connections
- Wildcard patterns are supported in the `name` field (e.g., `name = user*` matches `user1`, `user2`, etc.)
- Special pattern `name = *` acts as a catch-all for all users
- Matching priority: exact user match > wildcard pattern match (longest prefix) > `*` catch-all > global limit

**Note:** Per-user limits are checked in addition to global limits. If a global limit
is set and a user's per-user limit is higher than the global limit, the global limit
takes precedence.

Log Configuration
-----------------

To log throttle-related activity, set:

```
throttle.trace [all] [off|none] [bandwidth] [ioload] [debug]
```

- `all`: All debugging statements are enabled.
- `off` or `none`: No debugging statements are enabled.
- `bandwidth`: Log bandwidth-usage-related statistics.
- `ioload`: Log concurrency-related statistics.
- `debug`: Log all throttle-related information; this is verbose and aims
  to provide developers with enough information to debug the throttle's activity.

