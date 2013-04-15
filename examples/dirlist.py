"""
Ask a for a directory listing
-----------------------------

Produces output similar to the following::

  2013-04-12 09:46:51         20 spam
  2013-04-05 08:23:00       4096 .xrootd
  2013-04-12 09:33:25         20 eggs
"""

from XRootD import client
from XRootD.client.flags import DirListFlags

myclient = client.FileSystem('root://localhost')
status, listing = myclient.dirlist('/tmp', DirListFlags.STAT)

print listing.parent
for entry in listing:
  print "{0} {1:>10} {2}".format(entry.statinfo.modtimestr, entry.statinfo.size, entry.name)