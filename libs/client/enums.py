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

def enum(**enums):
  """Build the equivalent of a C++ enum"""
  reverse = dict((value, key) for key, value in enums.iteritems())
  enums['reverse_mapping'] = reverse
  return type('Enum', (), enums)

QueryCode = enum(
  STATS          = 1,
  PREPARE        = 2,
  CHECKSUM       = 3,
  XATTR          = 4,
  SPACE          = 5,
  CHECKSUMCANCEL = 6,
  CONFIG         = 7,
  VISA           = 8,
  OPAQUE         = 16,
  OPAQUEFILE     = 32
)

OpenFlags = enum(
  NONE      = 0,
# COMPRESS  = 1,
  DELETE    = 2,
  FORCE     = 4,
  NEW       = 8,
  READ      = 16,
  UPDATE    = 32,
# ASYNC     = 64,
  REFRESH   = 128,
  MAKEPATH  = 256,
  APPEND    = 512,
# RETSTAT   = 1024,
  REPLICA   = 2048,
  POSC      = 4096,
  NOWAIT    = 8192,
  SEQIO     = 16384
)

AccessMode = enum(
  NONE = 0,
  UR   = 0x100,
  UW   = 0x080, 
  UX   = 0x040,
  GR   = 0x020,
  GW   = 0x010,
  GX   = 0x008,
  OR   = 0x004,
  OW   = 0x002,
  OX   = 0x001
)

MkDirFlags = enum(
  NONE     = 0,
  MAKEPATH = 1
)

DirListFlags = enum(
  NONE   = 0,
  STAT   = 1,
  LOCATE = 2
)

PrepareFlags = enum(
# CANCEL    = 1,
# NOTIFY    = 2,
# NOERRS    = 4,
  STAGE     = 8,
  WRITEMODE = 16,
  COLOCATE  = 32,
  FRESH     = 64
)