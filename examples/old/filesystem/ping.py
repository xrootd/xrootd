from XRootD import client

myclient = client.FileSystem("root://localhost")
print 'URL:', myclient.url

#-------------------------------------------------------------------------------
# Synchronous example
#-------------------------------------------------------------------------------
status = myclient.ping()
print 'Status:', status['message']

#-------------------------------------------------------------------------------
# Asynchronous non-waiting example
#-------------------------------------------------------------------------------
def callback(status, hostList):
  print 'Response status:', status['message']

  for host in hostList:
    print "Host:", host.url

status = myclient.ping(timeout=5, callback=callback)
print 'Send status:', status['message']

x = raw_input()