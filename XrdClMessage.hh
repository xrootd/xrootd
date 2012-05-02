//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_MESSAGE_HH__
#define __XRD_CL_MESSAGE_HH__

#include "XrdCl/XrdClBuffer.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! The message representation used throughout the system
  //----------------------------------------------------------------------------
  class Message: public Buffer
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Message( uint32_t size = 0 ): Buffer( size )
      {
        if( size )
          Zero();
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~Message() {}
  };
}

#endif // __XRD_CL_MESSAGE_HH__
