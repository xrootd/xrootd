from pyxrootd import client

class File(object):
  def __init__(self):
    self.__file = client.File()
