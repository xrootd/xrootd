from pyxrootd import client

class CopyJob(object):
  """TODO: write me"""
  
  def __init__(self, source, target):
    self.__job = client.CopyJob(source, target)
