from XRootD import client

myclient = client.Client("root://localhost")
print myclient.url.url

status = myclient.stat("/tmp")

print "status: ", status.status
print 'code: ', status.code
print 'errNo: ', status.errNo