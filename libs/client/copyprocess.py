from pyxrootd import client

class CopyProcess(object):
  """TODO: write me"""
  
  def __init__(self):
    print '+++++ new copyprocess'
    self.__process = client.CopyProcess()
    
  def add_job(self, source='', target=''):
    """Add a job"""
    self.__process.add_job(source, target)
    print '+++++ job added'
    
  def prepare(self):
    """Prepare the jobs"""
    self.__process.prepare()
    print '+++++ prepped'
    
  def run(self):
    """Run the jobs"""
    print '+++++ running'
    self.__process.run()