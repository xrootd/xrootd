//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_UTILS_HH__
#define __XRD_CL_UTILS_HH__

#include <string>
#include <vector>

namespace XrdClient
{
  class Log;

  //----------------------------------------------------------------------------
  //! Random utilities
  //----------------------------------------------------------------------------
  class Utils
  {
    public:
      //------------------------------------------------------------------------
      //! Split a string
      //------------------------------------------------------------------------
      static void splitString( std::vector<std::string> &result,
                               const std::string        &input,
                               const std::string        &delimiter );
  };

  //----------------------------------------------------------------------------
  //! Smart descriptor - closes the descriptor on destruction
  //----------------------------------------------------------------------------
  class ScopedDescriptor
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      ScopedDescriptor( int descriptor ): pDescriptor( descriptor ) {}

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~ScopedDescriptor() { if( pDescriptor >= 0 ) close( pDescriptor ); }

      //------------------------------------------------------------------------
      //! Release the descriptor being held
      //------------------------------------------------------------------------
      int Release()
      {
        int desc = pDescriptor;
        pDescriptor = -1;
        return desc;
      }

      //------------------------------------------------------------------------
      //! Get the descriptor
      //------------------------------------------------------------------------
      int GetDescriptor()
      {
        return pDescriptor;
      }

    private:
      int pDescriptor;
  };
}

#endif // __XRD_CL_UTILS_HH__
