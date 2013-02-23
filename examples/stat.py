from XRootD import client

myclient = client.Client("root://localhost")
print 'URL:', myclient.url

status = myclient.stat("/foo")
print status

print "status:", status.status
print 'code:', status.code
print 'errNo:', status.errNo
print 'message:', status.GetErrorMessage()
print 'IsError:', status.IsError()
print 'IsFatal:', status.IsFatal()
print 'IsOK:', status.IsOK()