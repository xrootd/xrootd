from threading import Lock

class AsyncResponseHandler:
  def __init__(self):
    self.mutex = Lock()
    self.mutex.acquire()

  def __call__(self, status, response, hostList):
    self.mutex.release()
    self.status = status
    self.response = response
    self.hostList = hostList

  def waitFor(self):
    self.mutex.acquire()
    self.mutex.release()
    return self.status, self.response, self.hostList