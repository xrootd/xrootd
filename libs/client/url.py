#-------------------------------------------------------------------------------
# Copyright (c) 2012-2013 by European Organization for Nuclear Research (CERN)
# Author: Justin Salmon <jsalmon@cern.ch>
#-------------------------------------------------------------------------------
# XRootD is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# XRootD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with XRootD.  If not, see <http:#www.gnu.org/licenses/>.
#-------------------------------------------------------------------------------

from pyxrootd import client

class URL(object):
  """Server URL object.

  This class has each portion of an `XRootD` URL split up as attributes. For
  example, given the URL::

    >>> url = URL(root://user1:passwd1@host1:1234//path?param1=val1&param2=val2)

  then ``url.hostid`` would return `user1:passwd1@host1:1234`.

  :var           hostid: The host part of the URL, i.e. ``user1:passwd1@host1:1234``
  :var         protocol: The protocol part of the URL, i.e. ``root``
  :var         username: The username part of the URL, i.e. ``user1``
  :var         password: The password part of the URL, i.e. ``passwd1``
  :var         hostname: The name of the target host part of the URL, i.e. ``host1``
  :var             port: The target port part of the URL, i.e. ``1234``
  :var             path: The path part of the URL, i.e. ``path``
  :var path_with_params: The path part of the URL with parameters, i.e.
                         ``path?param1=val1&param2=val2``
  """

  def __init__(self, url):
    self.__url = client.URL(url)

  def __str__(self):
    return str(self.__url)

  @property
  def hostid(self):
    return self.__url.hostid

  @property
  def protocol(self):
    return self.__url.protocol

  @property
  def username(self):
    return self.__url.username

  @property
  def password(self):
    return self.__url.password

  @property
  def hostname(self):
    return self.__url.hostname

  @property
  def port(self):
    return self.__url.port

  @property
  def path(self):
    return self.__url.path

  @property
  def path_with_params(self):
    return self.__url.path_with_params

  def is_valid(self):
    """Return the validity of the URL"""
    return self.__url.is_valid()

  def clear(self):
    """Clear the URL"""
    return self.__url.clear()
