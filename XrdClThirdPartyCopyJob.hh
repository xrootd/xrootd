//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_THIRD_PARTY_COPY_JOB_HH__
#define __XRD_CL_THIRD_PARTY_COPY_JOB_HH__

#include "XrdCl/XrdClCopyProcess.hh"

namespace XrdClient
{
  class ThirdPartyCopyJob: public CopyJob
  {
    public:
      //------------------------------------------------------------------------
      //! Run the copy job
      //!
      //! @param handler the handler to be notified about the copy progress
      //! @return        status of the copy operation
      //------------------------------------------------------------------------
      virtual XRootDStatus Run( CopyProgressHandler *handler = 0 );
  };
}

#endif // __XRD_CL_THIRD_PARTY_COPY_JOB_HH__
