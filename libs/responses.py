
class XRootDStatus(object):
  """Status of an operation. Returned with every request."""

  def __init__(self, _dict):
    self.status = _dict['status']       #: = 0, the status
    self.code = _dict['code']
    self.errNo = _dict['errNo']
    self.message = _dict['message']
    self.shellcode = _dict['shellCode']
    self.error = _dict['isError']
    self.fatal = _dict['isFatal']
    self.ok = _dict['isOK']
  
  def __str__(self):
    return self.message

class StatInfo(object):

  def __init__(self, _dict):
    self.id = _dict['id']
    self.flags = _dict['flags']
    self.size = _dict['size']
    self.modTime = _dict['modTime']
    self.modTimeAsString = _dict['modTimeAsString']
    
class StatInfoVFS(object):
  def __init__(self, _dict):
    pass

class LocationInfo(object):
  def __init__(self, _list):
    self.locations = _list
  
  class Location(object):
    def __init__(self, _dict):
      pass
  
class HostInfo(object):
  def __init__(self, _list):
    self.hosts = _list
    
class VectorReadInfo(object):
  def __init__(self, _dict):
    self.d = _dict
  
class ProtocolInfo(object):
  def __init__(self, _dict):
    pass

class DirListInfo(object):
  def __init__(self, _dict):
    pass

