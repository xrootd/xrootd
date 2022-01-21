# XrdClRecorder Plugin

This XRootD Client Plugin can be used to record all user's actions on XrdCl::File object and store them into a csv file. Afterwards, using the xrdreplay utily the actions can be replayed preserving the original timing.

Config file format:

**recorder.conf:**

```bash
url = *
lib = /home/simonm/git/xrootd-xrdreply/build/src/libXrdClRecorder-5.so
enable = true
output = /tmp/out.csv
```
