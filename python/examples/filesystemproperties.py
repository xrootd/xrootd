"""
Make a directory, remove it and test the filesystem properties
--------------------------------------------------------------
"""
from XRootD import client
from XRootD.client.flags import MkDirFlags

myclient = client.FileSystem("root://localhost")
myclient.mkdir("/tmp/some/dir", MkDirFlags.MAKEPATH)
myclient.rmdir("/tmp/some/dir")
myclient.rmdir("/tmp/some")

print myclient.get_property( "FollowRedirects" )
print myclient.set_property( "FollowRedirects", "false" )
print myclient.get_property( "FollowRedirects" )

