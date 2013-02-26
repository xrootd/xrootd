from XRootD import client
import sys

myclient = client.Client("root://localhoost")
print 'URL:', myclient.url
 
#-------------------------------------------------------------------------------
# Synchronous example
#-------------------------------------------------------------------------------
status, response = myclient.stat("/tmp")
print "Status:", str(status)
print "Response:", str(response)
#print "Modification time:", response.GetModTimeAsString()
 
#-------------------------------------------------------------------------------
# Asynchronous non-waiting example
#-------------------------------------------------------------------------------
def callback(status, response, hostList):
  print 'Status:', str(status)
  print 'Response:', str(response)
  print 'Modification time:', response.GetModTimeAsString()
   
  for host in hostList:
    print "Host:", host.url
 
myclient.stat("/tmp", callback)
 
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
    print 'Status:', status
    print 'Response:', response
    print 'Modification time:', response.GetModTimeAsString()
    self.sem.release()
    
  def startAndWait(self):
    handler.start()
    handler.join()

#myclient = client.Client('root://localhost')
#handler = AsyncStatHandler(myclient, '/tmp')
#handler.startAndWait()

