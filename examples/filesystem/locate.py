from XRootD import client

myclient = client.FileSystem("root://localhost")
print 'URL:', myclient.url

#-------------------------------------------------------------------------------
# Synchronous example
#-------------------------------------------------------------------------------
status, response = myclient.locate("/tmp", 0)
print "Status:", status
print "Response:", response

status, response = myclient.deeplocate("/tmp", 0)
print "Status:", status
print "Response:", response