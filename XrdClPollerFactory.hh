//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------

#ifndef __XRD_CL_POLLER_FACTORY_HH__
#define __XRD_CL_POLLER_FACTORY_HH__

#include "XrdCl/XrdClPoller.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Helper for creating poller objects
  //----------------------------------------------------------------------------
  class PollerFactory
  {
    public:
      //------------------------------------------------------------------------
      //! Create a poller object, try in order of preference, if none of the
      //! poller types is known then return 0
      //!
      //! @param preference comma separated list of poller types in order of
      //!                   preference
      //! @return           poller object or 0 if non of the poller types
      //!                   is known
      //------------------------------------------------------------------------
      static Poller *CreatePoller( const std::string &preference );
  };
}

#endif // __XRD_CL_POLLER_FACTORY_HH__
