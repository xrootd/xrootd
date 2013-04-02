from pyxrootd import client

class URL(object):
  """Server URL object.
  
  This class has each portion of an `XRootD` URL split up as attributes. For
  example, given the URL::
  
    >>> url = URL(root://user1:passwd1@host1:1234//path?param1=val1&param2=val2)
    
  then ``url.hostid`` would return `user1:passwd1@host1:1234`.
  """
  
  def __init__(self, url):
    self.__url = url

  def __str__(self):
    return str(self.__url)

  @property
  def hostid(self):
    """The host part of the URL, i.e. ``user1:passwd1@host1:1234``"""
    return self.__url.hostid

  @property
  def protocol(self):
    """The protocolpart of the URL, i.e. ``root``"""
    return self.__url.protocol

  @property
  def username(self):
    """The username part of the URL, i.e. ``user1``"""
    return self.__url.username

  @property
  def password(self):
    """The password part of the URL, i.e. ``passwd1``"""
    return self.__url.password

  @property
  def hostname(self):
    """The name of the target host part of the URL, i.e. ``host1``"""
    return self.__url.hostname

  @property
  def port(self):
    """The target port part of the URL, i.e. ``1234``"""
    return self.__url.port

  @property
  def path(self):
    """The path part of the URL, i.e. ``path``"""
    return self.__url.path
    
  @property
  def path_with_params(self):
    """The path part of the URL with parameters, i.e. 
    ``path?param1=val1&param2=val2``
    """
    return self.__url.path_with_params

  def is_valid(self):
    """Return the validity of the URL
    
    :rtype: boolean
    """
    return self.__url.is_valid()
  
  def clear(self):
    """Clear the URL"""
    return self.__url.clear()