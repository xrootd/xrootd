# Prepare GPI query admission

The generic prepare plugin accepts `-qrywait <seconds>` on its `ofs.preplib`
directive. This sets the maximum time a query waits for a free execution slot
when the `-maxquery` limit has been reached. The default is 33 seconds; valid
values are 1 through 600, inclusive. Invalid or missing values prevent plugin
initialization.

```conf
ofs.preplib libXrdOfsPrepGPI.so -admit stage,query -maxquery 8 -qrywait 33 -run /opt/site/bin/prepare
```

The wait uses one monotonic deadline and rechecks it after wakeups. Expiration
returns `ETIMEDOUT` without running the query. Spurious wakeups do not restart
the waiting period. A slot becoming available only after the deadline does not
admit an expired query.

This option limits admission waiting, not the execution time of a started
prepare program. Sites must separately ensure that their program finishes.
Changing the plugin configuration requires a server restart.

`-qrywait` replaces the unreleased `-wait` spelling proposed during review;
`-wait` is not accepted. No released configuration option is renamed.
