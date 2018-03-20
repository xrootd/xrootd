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

#include "XrdCl/XrdClTransportManager.hh"
#include "XrdCl/XrdClXRootDTransport.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  TransportManager::TransportManager()
  {
    pHandlers["root"]  = new XRootDTransport();
    pHandlers["xroot"] = new XRootDTransport();
    pHandlers["roots"] = new XRootDTransport();
    pHandlers["xroots"] = new XRootDTransport();
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  TransportManager::~TransportManager()
  {
    HandlerMap::iterator it;
    for( it = pHandlers.begin(); it != pHandlers.end(); ++it )
      delete it->second;
  }

  //----------------------------------------------------------------------------
  // Register a transport factory function for a given protocol
  //----------------------------------------------------------------------------
  bool TransportManager::RegisterFactory( const std::string &protocol,
                                          TransportFactory   factory )
  {
    FactoryMap::iterator it = pFactories.find( protocol );
    if( it == pFactories.end() )
      return false;
    pFactories[protocol] = factory;
    return true;
  }

  //----------------------------------------------------------------------------
  // Get a transport handler object for a given protocol
  //----------------------------------------------------------------------------
  TransportHandler *TransportManager::GetHandler( const std::string &protocol )
  {
    HandlerMap::iterator itH = pHandlers.find( protocol );
    if( itH != pHandlers.end() )
      return itH->second;

    FactoryMap::iterator itF = pFactories.find( protocol );
    if( itF == pFactories.end() )
      return 0;

    TransportHandler *handler = (*itF->second)();
    pHandlers[protocol] = handler;
    return handler;
  }
}
