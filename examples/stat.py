from XRootD import client

myclient = client.Client("root://localhost")
print 'URL:', myclient.url

def callback(status, response):
  # todo: add host list as return param
  print 'Status:', status
  print 'Modification time:', response.GetModTimeAsString()

myclient.stat("/tmp", callback)

# Halt script (todo: implement callback class w/semaphore and/or callback 
# decorator w/generator)
raw_input()
