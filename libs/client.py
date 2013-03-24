from pyxrootd import client
from XRootD.returntypes import XRootDStatus, StatInfo, LocationInfo

class Client(object):
  # Doing both the fast backend and the pythonic frontend
  # Moving the border of the bindings, more python-heavy
  # Syntactic sugar
  # There are now 2 client objects... hmmm
  # This is more stable and self-documenting
  # Remove keywords in libpyxrootd? Let Python handle them up here?
  # I can put enums and handlers inside the actual class
  # Only unmodifiable immutable objects can live here, there's no way we're
  # reimplementing any methods...
  def __init__(self, url):
    self.__filesystem = client.FileSystem(url)
    self.url = self.__filesystem.url

  def locate(self, path, flags, timeout=0, callback=None):
    status, response = self.__filesystem.locate(path, flags, timeout, callback)
    return XRootDStatus(status), LocationInfo(response)
  
  def deeplocate(self):
    pass
    
  def query(self):
    pass
    
  def truncate(self):
    pass
    
  def mv(self):
    pass
    
  def chmod(self):
    pass
    
  def rm(self):
    pass
    
  def mkdir(self):
    pass
    
  def rmdir(self):
    pass
    
  def ping(self):
    pass
    
  def stat(self, path, timeout=0, callback=None):
    status, response = self.__filesystem.stat(path, timeout)
    return XRootDStatus(status), StatInfo(response)
  
  def statvfs(self):
    pass
    
  def protocol(self):
    pass
    
  def dirlist(self):
    pass
    
  def sendinfo(self):
    pass
    
  def prepare(self):
    pass

