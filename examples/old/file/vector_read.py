from XRootD import client
from XRootD.client.flags import OpenFlags

with client.File() as f:
    status, response = f.open('root://localhost//tmp/spam', OpenFlags.UPDATE)
    assert f.is_open()
    f.write(r'The XROOTD project aims at giving high performance, scalable fault'
             + ' tolerant access to data repositories of many kinds')

    size = f.stat()[1]['size']
    v = [(0, 40), (40, 40), (80, size - 80)]

    print f.readlines()
    status, response = f.vector_read(chunks=v)
    print status['message']
    print response

    for chunk in response['chunks']:
        print chunk