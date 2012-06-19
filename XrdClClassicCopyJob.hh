//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_CLASSIC_COPY_JOB_HH__
#define __XRD_CL_CLASSIC_COPY_JOB_HH__

#include "XrdCl/XrdClCopyProcess.hh"

namespace XrdCl
{
  class ClassicCopyJob: public CopyJob
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      ClassicCopyJob( const URL *source, const URL *destination );

      //------------------------------------------------------------------------
      //! Run the copy job
      //!
      //! @param progress the handler to be notified about the copy progress
      //! @return         status of the copy operation
      //------------------------------------------------------------------------
      virtual XRootDStatus Run( CopyProgressHandler *progress = 0 );
  };
}

#endif // __XRD_CL_CLASSIC_COPY_JOB_HH__
