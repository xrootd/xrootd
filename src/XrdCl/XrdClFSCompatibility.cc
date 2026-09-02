//------------------------------------------------------------------------------
// Copyright (c) 2026 by European Organization for Nuclear Research (CERN)
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClFSCompatibility.hh"

namespace XrdCl
{
  const char *GetGFALFileStatus( bool offline, bool backupExists )
  {
    const bool onDisk = !offline;
    if( backupExists && onDisk ) return "ONLINE_AND_NEARLINE";
    if( backupExists ) return "NEARLINE";
    if( onDisk ) return "ONLINE";
    return "UNKNOWN";
  }
}
