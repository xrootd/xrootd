//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Krzysztof Jamrog <krzysztof.piotr.jamrog@cern.ch>,
//         Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include <stdexcept>
#include <string>
#include "XrdCl/XrdClOperations.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  // OperationHandler Constructor.
  //----------------------------------------------------------------------------
  PipelineHandler::PipelineHandler( ResponseHandler *handler, bool own ) :
      responseHandler( handler ), ownHandler( own )
  {
  }

  //----------------------------------------------------------------------------
  // Default Constructor.
  //----------------------------------------------------------------------------
  PipelineHandler::PipelineHandler() : responseHandler( nullptr ), ownHandler( false )
  {
  }

  //----------------------------------------------------------------------------
  // OperationHandler::AddOperation
  //----------------------------------------------------------------------------
  void PipelineHandler::AddOperation( Operation<true> *operation )
  {
    if( nextOperation )
    {
      nextOperation->AddOperation( operation );
    }
    else
    {
      nextOperation.reset( operation );
    }
  }

  //----------------------------------------------------------------------------
  // OperationHandler::HandleResponseImpl
  //----------------------------------------------------------------------------
  void PipelineHandler::HandleResponseImpl( XRootDStatus *status,
      AnyObject *response, HostList *hostList )
  {
    std::unique_ptr<PipelineHandler> myself( this );

    // We need to copy status as original status object is destroyed in HandleResponse function
    XRootDStatus st( *status );
    if( responseHandler )
    {
      responseHandler->HandleResponseWithHosts( status, response, hostList );
      ownHandler = false;
    }
    else
      dealloc( status, response, hostList );

    if( !st.IsOK() || !nextOperation )
    {
      if( final ) final( st );
      prms.set_value( st );
      return;
    }

    nextOperation->Run( std::move( prms ), std::move( final ) );
  }

  //----------------------------------------------------------------------------
  // OperationHandler::HandleResponseWithHosts
  //----------------------------------------------------------------------------
  void PipelineHandler::HandleResponseWithHosts( XRootDStatus *status,
      AnyObject *response, HostList *hostList )
  {
    HandleResponseImpl( status, response, hostList );
  }

  //----------------------------------------------------------------------------
  // OperationHandler::HandleResponse
  //----------------------------------------------------------------------------
  void PipelineHandler::HandleResponse( XRootDStatus *status,
      AnyObject *response )
  {
    HandleResponseImpl( status, response );
  }

  //----------------------------------------------------------------------------
  // OperationHandler Destructor
  //----------------------------------------------------------------------------
  PipelineHandler::~PipelineHandler()
  {
    if( ownHandler ) delete responseHandler;
  }

  //----------------------------------------------------------------------------
  // OperationHandler::AssignToWorkflow
  //----------------------------------------------------------------------------
  void PipelineHandler::Assign( std::promise<XRootDStatus>                p,
                                std::function<void(const XRootDStatus&)>  f )
  {
    prms  = std::move( p );
    final = std::move( f );
  }

}

