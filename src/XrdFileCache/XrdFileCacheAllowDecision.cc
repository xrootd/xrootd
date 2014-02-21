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

#include "XrdFileCacheDecision.hh"
#include "XrdSys/XrdSysError.hh"


class AllowDecision : public XrdFileCache::Decision
{
   //----------------------------------------------------------------------------
   //! The simplest example of plugin - always allow the file to be prefetched.
   //----------------------------------------------------------------------------

   public:
      virtual bool Decide(std::string &, XrdOss &) const {return true; }

};

/******************************************************************************/
/*                          XrdFileCacheGetDecision                           */
/******************************************************************************/

// Return a decision object to use.
extern "C"
{
XrdFileCache::Decision *XrdFileCacheGetDecision(XrdSysError &)
{
   return new AllowDecision();
}
}

