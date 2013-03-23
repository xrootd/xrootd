
    
class XRootDStatus(object):
    """Parse the dict and morph it back into a proper object"""
    
    def __init__(self, _dict):
        # get this thing to add functions/methods to itself
        # based on mappings? You can build me if you give me a dict of myself.
        self.status = _dict['status']
        self.code = _dict['code']
        self.errNo = _dict['errNo']
        
class StatInfo(object):
    
    def __init__(self, _dict):
        self.id = _dict['id']
        self.flags = _dict['flags']
        self._modTime = _dict['modTime']
        self.modTimeAsString = _dict['modTimeAsString']