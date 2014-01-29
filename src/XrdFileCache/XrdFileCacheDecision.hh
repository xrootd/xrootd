#ifndef __XRDFILECACHE_DECISION_HH__
#define __XRDFILECACHE_DECISION_HH__
//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University
// Author: Alja Mrak-Tadel, Matevz Tadel, Brian Bockelman
//----------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------------------

#include <string>
#include <iostream>
#include <stdio.h>
#include "XrdOss/XrdOss.hh"

class XrdSysError;

namespace XrdFileCache
{
   //----------------------------------------------------------------------------
   //! Base class for selecting which files should be cached.
   //----------------------------------------------------------------------------
   class Decision
   {
      public:
         //--------------------------------------------------------------------------
         //! Destructor
         //--------------------------------------------------------------------------
         virtual ~Decision() {}

         //---------------------------------------------------------------------
         //! Decide if original source will be cached.
         //!
         //! @param & path
         //! @param & file system
         //!
         //! @return decision
         //---------------------------------------------------------------------
         virtual bool Decide(std::string &, XrdOss &) const = 0;

         //------------------------------------------------------------------------------
         //! Parse configuration arguments.
         //!
         //! @param char* configuration parameters
         //!
         //! @return status of configuration
         //------------------------------------------------------------------------------
         virtual bool ConfigDecision(const char*) { return true; }
   };
}

#endif
