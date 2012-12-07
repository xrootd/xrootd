//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
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

#ifndef __XRD_CL_TRANSPORT_MANAGER_HH__
#define __XRD_CL_TRANSPORT_MANAGER_HH__

#include <map>
#include <string>

namespace XrdCl
{
  class TransportHandler;

  //----------------------------------------------------------------------------
  //! Manage transport handler objects
  //----------------------------------------------------------------------------
  class TransportManager
  {
    public:
      typedef TransportHandler *(*TransportFactory)();

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      TransportManager();

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      virtual ~TransportManager();

      //------------------------------------------------------------------------
      //! Register a transport factory function for a given protocol
      //------------------------------------------------------------------------
      bool RegisterFactory( const std::string &protocol,
                            TransportFactory   factory );

      //------------------------------------------------------------------------
      //! Get a transport handler object for a given protocol
      //------------------------------------------------------------------------
      TransportHandler *GetHandler( const std::string &protocol );

    private:
      typedef std::map<std::string, TransportHandler*> HandlerMap;
      typedef std::map<std::string, TransportFactory>  FactoryMap;
      HandlerMap  pHandlers;
      FactoryMap  pFactories;
  };
}

#endif // __XRD_CL_TRANSPORT_MANAGER_HH__
