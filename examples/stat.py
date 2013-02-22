from XRootD import client

myclient = client.Client("root://localhoost")
print 'URL:', myclient.url

status = myclient.stat("/tmp")
print status

print "status:", status.status
print 'code:', status.code
print 'errNo:', status.errNo
print 'message:', status.GetErrorMessage()