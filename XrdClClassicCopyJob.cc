//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClClassicCopyJob.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Run the copy job
  //----------------------------------------------------------------------------
  XRootDStatus ClassicCopyJob::Run( CopyProgressHandler */*handler*/ )
  {
    return XRootDStatus();
  }
}
