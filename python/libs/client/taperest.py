# This file intentionally uses the historical Python style used in this tree.
from __future__ import absolute_import, division, print_function

from pyxrootd import client
from XRootD.client.responses import XRootDStatus
from XRootD.client.responses import TapeRestEndpoint, TapeRestArchiveInfo

class TapeRestClient(object):
  """Synchronous client for the WLCG Tape REST API.

  :param  timeout: Maximum HTTP operation time in seconds
  :param     cert: User certificate or proxy path
  :param      key: User private key path
  :param verbosity: Verbosity level used for HTTP debugging
  """

  def __init__(self, timeout=-1, cert='', key='', verbosity=0):
    self.timeout = timeout
    self.cert = cert
    self.key = key
    self.verbosity = verbosity

  def discover(self, url):
    """Discover the Tape REST API endpoint for a storage URL.

    :returns: tuple containing :mod:`XRootD.client.responses.XRootDStatus`
              and :mod:`XRootD.client.responses.TapeRestEndpoint`
    """
    status, endpoint = client.TapeRestDiscover_cpp(
      url, self.timeout, self.cert, self.key, self.verbosity)
    if endpoint:
      endpoint = TapeRestEndpoint(endpoint)
    return XRootDStatus(status), endpoint

  def archive_info(self, urls):
    """Query archive locality information for one or more storage URLs.

    :returns: tuple containing :mod:`XRootD.client.responses.XRootDStatus`
              and a list of :mod:`XRootD.client.responses.TapeRestArchiveInfo`
    """
    status, results = client.TapeRestArchiveInfo_cpp(
      urls, self.timeout, self.cert, self.key, self.verbosity)
    return XRootDStatus(status), [TapeRestArchiveInfo(r) for r in results]
