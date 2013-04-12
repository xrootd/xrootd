from XRootD import client
from XRootD.client.flags import OpenFlags

# Text file
# with client.File() as f:
#     status, response = f.open('root://localhost//tmp/bigfile', OpenFlags.DELETE)
#     assert f.is_open()
#     f.write('green\neggs\nand\nham')
# 
# #     s, d = f.read(offset=0, size=100)
# #     print d
# #     print len(d)
# 
#     print ">>> f.readline()"
#     print f.readline() # Reads single line, up to \n
#     
#     print ">>> for line in f"
#     for line in f: # Exact same semantics as built-in file. Will not return status
#         print line
# 
#     print ">>> f.read()"
#     print f.read() # Reads entire file
# 
#     print ">>> f.readlines()"
#     print f.readlines()
# 
#     print ">>> for line in f.readlines()"
#     for line in f.readlines(): # Reads whole file into list, split by \n
#         print line
#   
#     print ">>> for chunk in f.readchunks()"
#     for chunk in f.readchunks(blocksize=2, offset=0):
#         print chunk

# Binary file
f = client.File()
f.open('root://localhost//tmp/bigfile')

status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
status, response = f.stat(force=False)
size = response.size
print ">>>>> size:", size

#     status, response = f.read()
#     assert status.ok
#     assert len(response) == size
print 'lol'

#     print ">>> for chunk in f.readchunks()"
#     for chunk in f.readchunks(blocksize=10):
#         print chunk

#     print ">>> for line in f.readlines()"
#     for line in f.readlines(): # Reads whole file into list, split by \n
#         print '%r' % line

#    print ">>> for line in f"
#    for line in f: # Exact same semantics as built-in file. Will not return status
#        print ">>>>> %r" % line
