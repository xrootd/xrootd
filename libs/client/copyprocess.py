from pyxrootd import client
from XRootD.responses import XRootDStatus

class CopyProcess(object):
  """TODO: write me"""

  def __init__(self):
    self.__process = client.CopyProcess()

  def add_job(self, source, target, sourcelimit=1, force=False, 
              posc=False, coerce=False, thirdparty=False, checksumprint=False, 
              chunksize=4194304, parallelchunks=8):
    """Add a job
    
    :param source: 
    :type  source:
    :param target:
    :type  target:
    """
    self.__process.add_job(source, target, sourcelimit, force, posc, coerce, 
                           thirdparty, checksumprint, chunksize, parallelchunks)

  def prepare(self):
    """Prepare the jobs"""
    status = self.__process.prepare()
    return XRootDStatus(status)

  def run(self):
    """Run the jobs"""
    status = self.__process.run()
    return XRootDStatus(status)