from XRootD import client

myclient = client.Client("root://localhost")
print 'URL:', myclient.url
 
#-------------------------------------------------------------------------------
# Synchronous example
#-------------------------------------------------------------------------------
# status, response = myclient.stat("/tmpp")
# print "Status:", status['message']
# print "Response:", str(response)
# if response: print "Modification time:", response.GetModTimeAsString()
 
#-------------------------------------------------------------------------------
# Asynchronous non-waiting example
#-------------------------------------------------------------------------------
def callback(status, response, hostList):
  print 'Status:', status['message']
  print 'Response:', str(response)
  if response: print 'Modification time:', response.GetModTimeAsString()
   
  for host in hostList:
    print "Host:", host.url

status = myclient.stat("/tmp", callback)
print 'Status:', status['message']
 
# Halt script (todo: implement callback class w/semaphore and/or callback 
# decorator w/generator)
x = raw_input()

#-------------------------------------------------------------------------------
# Asynchronous waiting example
#-------------------------------------------------------------------------------
from threading import BoundedSemaphore, Thread

class AsyncStatHandler(Thread):
  def __init__(self, client, path):
    self.client = client
    self.path   = path
    self.sem    = BoundedSemaphore(1)
    Thread.__init__(self)
    
  def run(self):
    self.sem.acquire()
    status, response = self.client.stat(self.path)
    print 'Status:', status['message']
    print 'Response:', response
    if response: print 'Modification time:', response.GetModTimeAsString()
    self.sem.release()
    
  def startAndWait(self):
    handler.start()
    handler.join()

myclient = client.Client('root://localhost')
handler = AsyncStatHandler(myclient, '/tmpp')
handler.startAndWait()

