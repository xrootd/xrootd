from XRootD import client

myclient = client.Client("root://localhost")
print 'URL:', myclient.url

def callback(status, response, hostList):
  print 'Status:', status
  print 'Modification time:', response.GetModTimeAsString()
  
  for host in hostList:
    print "Host:", host.url

myclient.stat("/tmp", callback)

# Halt script (todo: implement callback class w/semaphore and/or callback 
# decorator w/generator)
raw_input()
