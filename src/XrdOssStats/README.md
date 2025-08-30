XRootD OSS Stats
================

The plugin in this directory provides Prometheus-style statistics
for the OSS layer, allowing a server administrator to track the
storage load on the server and slow I/O operations

The statistics plugin can be loaded with the following configuration:

```
ofs.osslib ++ libXrdOssStats.so
```

Additionally, the following configurations are available:

```
fsstats.trace all
fsstats.slowop 1.5s
```

The options are:
- `fsstats.trace`: The log level for the plugin; valid settings are
  `all|err|warning|info|debug|none`.  Default is `warning`.
- `fsstats.slowop`: A cutoff where, over this value, operations are
  labeled as "slow".  Slow operations are tracked separately from the
  overall server operations, allowing administrators to observe periods
  of overload.  A unit is required; valid units include `m` (minutes), `s`
  (seconds), and `ms`.

Using the Statistics
--------------------

The statistics plugin is only activated if loaded and the `oss` type is enabled
in the g-stream monitoring.  This can be done through this example configuration:

```
xrootd.mongstream oss throttle use send json dflthdr localhost:1234
```

Such setting will send a JSON-formatted UDP packet to the socket at `localhost:1234`.

The statistics recorded (explained in detail below) count the operations performed
by the filesystem and their total duration.  They are useful in an external monitoring
system such as [Prometheus](https://prometheus.io/) that can manage time series data.

For example, the first derivative of the read operation total time represents, on average,
how many read operations are active.

Operation times are incremented periodically while the operation is running, avoiding a
large "spike" at the end of long-running operations.

The plugin keeps separate statistics for "slow" operations, allowing the administrator
to understand frequency and duration of operations while the server is performing poorly.
The threshold for "slow" can be set by the administrator; it defaults to 2.0 seconds.

When the server restarts all counters are reset to 0.

Statistics Recorded
-------------------

When loaded, the OSS plugin provides statistics via the XRootD server g-stream
functionality with the following JSON object:

```
    {
        "event:"oss_stats_XX",
        "reads: XX,
        "writes": XX,
        "stats": XX,
        "pgreads": XX,
        "pgwrites": XX,
        "readvs": XX,
        "readv_segs": XX,
        "dirlists": XX,
        "dirlist_ents": XX,
        "truncates": XX,
        "unlinks": XX,
        "chmods": XX,
        "opens": XX,
        "renames": XX,
        "slow_reads": XX,
        "slow_writes": XX,
        "slow_stats": XX,
        "slow_pgreads": XX,
        "slow_pgwrites": XX,
        "slow_readvs": XX,
        "slow_readv_segs": XX,
        "slow_dirlists": XX,
        "slow_dirlist_ents": XX,
        "slow_truncates": XX,
        "slow_unlinks": XX,
        "slow_chmods": XX,
        "slow_opens": XX,
        "slow_renames": XX,
        "open_t": YY,
        "read_t": YY,
        "readv_t": YY,
        "pgread_t": YY,
        "write_t": YY,
        "pgwrite_t": YY,
        "dirlist_t": YY,
        "stat_t": YY,
        "truncate_t": YY,
        "unlink_t": YY,
        "rename_t": YY,
        "chmod_t": YY,
        "slow_open_t": YY,
        "slow_read_t": YY,
        "slow_readv_t": YY,
        "slow_pgread_t": YY,
        "slow_write_t": YY,
        "slow_pgwrite_t": YY,
        "slow_dirlist_t": YY,
        "slow_stat_t": YY,
        "slow_truncate_t": YY,
        "slow_unlink_t": YY,
        "slow_rename_t": YY,
        "slow_chmod_t": YY
    }
```

The keys have the following definition:

- `event`: The type of g-stream information.  `oss_stats` for the default OSS path; `oss_stats_pfc` for the statistics
  from the PFC storage
- `read`: Count of read operations
- `writes`: Count of write operations
- `stats`: Count of "stat" operations
- `pgreads`: Count of aligned page reads with checksums
- `pgwrites`: Count of aligned page writes with checksums
- `readvs`: Count of vector read operations
- `readv_segs`: Sum of the number of segments in vector reads
- `dirlists`: Count of the number of times a directory listing has begun
- `dirlist_ents`: Sum of the number of entries in listed directories
- `truncates`: Count of truncate operations
- `unlinks`: Count of unlink (remove) operations
- `chmods`: Count of "change mode" (chmod) operations
- `opens`: Count of how many files have been opened
- `renames`: Count of rename operations
- `$FOO_t` (where `$FOO` is a named operation above): Total duration of operations of type `$FOO` in floating point
  seconds .  For example, if `open_t` is equal to 20.3, then the sum of all `open` operation durations is 20.3 seconds.
- `slow_$FOO` (where `$FOO` is another counter): Total count or duration of type `$FOO` for slow operations.
  A "slow operation" is any operation whose duration is larger than the slow operation threshold (defaults to
  2 seconds but can be managed by setting `fsstats.slowop`). For example, if an open operation takes 2.5 seconds
  then the values of `slow_open_t` and `open_t` would increase by 2.5 while `slow_opens` and `opens` would increase
  by 1.  If an open operation takes 1.0 seconds then only the value of `open_t` would increase by 1.0 and `opens` would
  increase by 1.

