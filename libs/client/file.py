import XRootD
from pyxrootd import client
from XRootD.responses import XRootDStatus, StatInfo, VectorReadInfo

q = lambda a, b, c: (b, c)[not a]

class File(object):
  """The file class"""
  
  def __init__(self):
    self.__file = client.File()
    
  def __enter__(self):
    return self
  
  def __exit__(self, type, value, traceback):
    self.__file.__exit__()
    
  def __iter__(self):
    return self
    
  def __iternext__(self):
    return self.__file.__iternext__()
    
  def open(self, url, flags=0, mode=0, timeout=0, callback=None):
    """Open the file pointed to by the given URL.
    
    :param   url: url of the file to be opened
    :type    url: string
    :param flags: An `ORed` combination of :mod:`XRootD.enums.OpenFlags`
                  where the default is `OpenFlags.NONE`
    :param  mode: access mode for new files, an `ORed` combination of
                 :mod:`XRootD.enums.AccessMode` where the default is
                 `AccessMode.NONE`
    :returns:    tuple containing status dictionary and None
    """
    if callback:
      callback = XRootD.client.CallbackWrapper(callback, None)
      return XRootDStatus(self.__file.open(url, flags, mode, timeout, callback))
    
    status, response = self.__file.open(url, flags, mode, timeout)
    return XRootDStatus(status), None
      
  def close(self, timeout=0, callback=None):
    """Close the file.
    
    :returns: tuple containing status dictionary and None
     
    As of Python 2.5, you can avoid having to call this method explicitly if you
    use the :keyword:`with` statement.  For example, the following code will
    automatically close *f* when the :keyword:`with` block is exited::

      from __future__ import with_statement # This isn't required in Python 2.6

      with client.File() as f:
        f.open("root://someserver//somefile")
        for line in f:
          print line,
    """
    if callback:
      callback = XRootD.client.CallbackWrapper(callback, None)
      return XRootDStatus(self.__file.close(timeout, callback))
    
    status, response = self.__file.close(timeout)
    return XRootDStatus(status), None
      
  def stat(self, force=False, timeout=0, callback=None):
    """Obtain status information for this file.
    
    :param force: do not use the cached information, force re-stating
    :type  force: boolean
    :returns:     tuple containing status dictionary and None
    """
    if callback:
      callback = XRootD.client.CallbackWrapper(callback, StatInfo)
      return XRootDStatus(self.__file.stat(force, timeout, callback))
    
    status, response = self.__file.stat(force, timeout)
    if response: response = StatInfo(response)
    return XRootDStatus(status), response
      
  def read(self, offset=0, size=0, timeout=0, callback=None):
    """Read a data chunk from a given offset.
    
    :param offset: offset from the beginning of the file
    :type  offset: integer
    :param   size: number of bytes to be read
    :type    size: integer
    :returns:      tuple containing status dictionary and None
    """
    if callback:
      callback = XRootD.client.CallbackWrapper(callback, None)
      return XRootDStatus(self.__filesystem.read(offset, size, timeout, callback))
    
    status, response = self.__file.read(offset, size, timeout)
    return XRootDStatus(status), response
      
  def readline(self):
    """Read a data chunk from a given offset, until the first newline or EOF
    encountered.

    :returns:      data that was read, including the trailing newline
    :rtype:        string
    """
    response = self.__file.readline()
    return response
      
  def readlines(self, offset, size):
    """Read lines from a given offset until EOF encountered. Return list of
    lines read.
     
    :param offset: offset from the beginning of the file
    :type  offset: integer
    :param   size: maximum number of bytes to be read
    :type    size: integer
    :returns:      data that was read, including trailing newlines
    :rtype:        list of strings
    """
    status, response = self.__file.readlines(offset, size)
    return XRootDStatus(status), response
      
  def readchunks(self, offset, blocksize):
    """Read data chunks from a given offset of the given size until EOF.
    Return list of chunks read.
    
    :param    offset: offset from the beginning of the file
    :type     offset: integer
    :param blocksize: size of chunk to read, in bytes
    :type  blocksize: integer
    :returns:         chunks that were read
    :rtype:           list of strings
    """
    status, response = self.__file.readchunks(offset, blocksize)
    return XRootDStatus(status), response
      
  def write(self, buffer, offset=0, size=0, timeout=0, callback=None):
    """Write a data chunk at a given offset.
    
    :param buffer: data to be written
    :param offset: offset from the beginning of the file
    :type  offset: integer
    :param   size: number of bytes to be written
    :type    size: integer
    :returns:      tuple containing status dictionary and None
    """
    if callback:
      callback = XRootD.client.CallbackWrapper(callback, None)
      return XRootDStatus(self.__file.write(buffer, offset, size, timeout, callback))
    
    status, response = self.__file.write(buffer, offset, size, timeout)
    return XRootDStatus(status), None

  def sync(self, timeout=0, callback=None):
    """Commit all pending disk writes.
    
    :returns: tuple containing status dictionary and None
    """
    if callback:
      callback = XRootD.client.CallbackWrapper(callback, None)
      return XRootDStatus(self.__file.sync(timeout, callback))
    
    status, response = self.__file.sync(timeout)
    return XRootDStatus(status), None
      
  def truncate(self, size, timeout=0, callback=None):
    """Truncate the file to a particular size.
    
    :param size: desired size of the file
    :type  size: integer
    :returns:    tuple containing status dictionary and None
    """
    if callback:
      callback = XRootD.client.CallbackWrapper(callback, None)
      return XRootDStatus(self.__file.truncate(size, timeout, callback))
    
    status, response = self.__file.truncate(size, timeout)
    return XRootDStatus(status), None
      
  def vector_read(self, chunks, timeout=0, callback=None):
    """Read scattered data chunks in one operation.
    
    :param chunks: list of the chunks to be read. The default maximum
                   chunk size is 2097136 bytes and the default maximum
                   number of chunks per request is 1024. The server may
                   be queried using :mod:`query` for the actual settings.
    :type  chunks: list of 2-tuples of the form (offset, size)
    :returns:      tuple containing status dictionary and vector read
                   info dictionary (see below)
    """
    if callback:
      callback = XRootD.client.CallbackWrapper(callback, VectorReadInfo)
      return XRootDStatus(self.__file.vector_read(chunks, timeout, callback))
    
    status, response = self.__file.vector_read(chunks, timeout)
    if response: response = VectorReadInfo(response)
    return XRootDStatus(status), response
      
  def is_open(self):
    """Check if the file is open.
    
    :rtype: boolean
    """
    return self.__file.is_open()
      
  def enable_read_recovery(self, enable):
    """Enable/disable state recovery procedures while the file is open for
    reading.
    
    :param enable: is read recovery enabled
    :type  enable: boolean
    """
    self.__file.enable_read_recovery(enable)
      
  def enable_write_recovery(self, enable):
    """Enable/disable state recovery procedures while the file is open for
    writing or read/write.
    
    :param enable: is write recovery enabled
    :type  enable: boolean
    """
    self.__file.enable_write_recovery(enable)
      
  def get_data_server(self):
    """Get the data server the file is accessed at.
    
    :returns: the address of the data server
    :rtype:   string
    """
    return self.__file.get_data_server()
