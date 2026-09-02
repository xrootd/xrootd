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

#ifndef __XRD_CL_FS_COMPATIBILITY_HH__
#define __XRD_CL_FS_COMPATIBILITY_HH__

namespace XrdCl
{
  const char *GetGFALFileStatus( bool offline, bool backupExists );
}

#endif // __XRD_CL_FS_COMPATIBILITY_HH__
