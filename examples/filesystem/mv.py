from XRootD import client

myclient = client.FileSystem("root://localhost")

status, response = myclient.mv("/tmp/spam", "/tmp/ham")
print status
print response