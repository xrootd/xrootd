"""
Common FileSystem workflows.

This example maps common ``xrdfs`` and ``xrdcp`` operations to the Python
bindings, including object-scoped authentication, endpoint checks, checksum
queries, copy, rename, and delete.
"""

from __future__ import print_function

from XRootD import client


endpoint = 'root://localhost/'
source = endpoint + '/tmp/source.dat'
target = endpoint + '/tmp/target.dat'
renamed = endpoint + '/tmp/renamed.dat'


auth = client.AuthContext.bearer(token='replace-with-token')
fs = client.FileSystem(endpoint, auth=auth)

status, _ = fs.ping(timeout=10)
print(status)

status, protocol = fs.protocol(timeout=10)
print(status, protocol)

status, _ = fs.copy(source, target, force=True)
print(status)

status, checksum = fs.checksum(target, timeout=10)
print(status, checksum)

status, _ = fs.mv(target, renamed, timeout=10)
print(status)

status, _ = fs.rm(renamed, timeout=10)
print(status)

auth.close()
