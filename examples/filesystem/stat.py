from XRootD import client, handlers

myclient = client.Client(url="root://localhost")
print 'URL:', myclient.url

#-------------------------------------------------------------------------------
# Synchronous example
#-------------------------------------------------------------------------------
status, response = myclient.stat(path="/tmp")
print "Status:", status
print "Response:", response
if response: print "Modification time:", response['modTimeAsString']

#-------------------------------------------------------------------------------
# Asynchronous non-waiting example
#-------------------------------------------------------------------------------
def callback(status, response, hostList):
  print 'Status:', status
  print 'Response:', response
  if response: print 'Modification time:', response['modTimeAsString']

  for host in hostList:
    print "Host:", host

status = myclient.stat(path="/tmp", callback=callback)
print 'Send status:', status['message']

# Halt script (todo: implement callback class w/semaphore and/or callback 
# decorator w/generator)
# x = raw_input()

#-------------------------------------------------------------------------------
# Asynchronous waiting example
#-------------------------------------------------------------------------------
handler = handlers.AsyncResponseHandler()
status = myclient.stat(path="/tmp", callback=handler)
print 'Request status:', status
status, response, hostList = handler.waitFor()
print 'Status:', status
print 'Response:', response
if response: print 'Modification time:', response['modTimeAsString']

for host in hostList:
  print "Host:", host


