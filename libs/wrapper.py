from XRootD import client, XRootDStatus

class Client(object):
    # Doing both the fast backend and the pythonic frontend
    # Moving the border of the bindings, more python-heavy
    # There are now 2 client objects... hmmm
    # This is more stable and self-documenting
    # Remove keywords in libpyxrootd? Let Python handle them up here?
    # I can put enums and handlers inside the actual class
    # Only unmodifiable immutable objects can live here, there's no way we're
    # reimplementing any methods...
    def __init__(self, url):
        self._client = client.Client(url)
        
    def stat(self, path, timeout=0, callback=None):
        status, response = self._client.stat(path, timeout)
        return XRootDStatus.XRootDStatus(status), XRootDStatus.StatInfo(response)