from threading import Lock
from XRootD.responses import XRootDStatus, HostInfo
import inspect

class CallbackWrapper(object):
  def __init__(self, callback, responsetype):
    if not hasattr(callback, '__call__'):
      raise TypeError('callback must be callable function, class or lambda')
    self.callback = callback
    self.responsetype = responsetype
    
  def __call__(self, status, response, hostlist):
    self.status = XRootDStatus(status)
    self.response = response
    if self.responsetype: 
      if inspect.isclass(self.response):
        self.response = self.response.__init__(response)
    self.hostlist = HostInfo(hostlist)
    self.callback(self.status, self.response, self.hostlist)


class AsyncResponseHandler(object):
  """Utility class to handle asynchronous method calls."""
  def __init__(self):
    self.mutex = Lock()
    self.mutex.acquire()

  def __call__(self, status, response, hostlist):
    self.status = status
    self.response = response
    self.hostlist = hostlist
    self.mutex.release()

  def waitFor(self):
    """Block and wait for the async response"""
    self.mutex.acquire()
    self.mutex.release()
    return self.status, self.response, self.hostlist
  