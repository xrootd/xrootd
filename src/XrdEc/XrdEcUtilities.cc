//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
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

#include "XrdEc/XrdEcUtilities.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

namespace XrdEc
{
  //---------------------------------------------------------------------------
  // A job for scheduling the user callback
  //---------------------------------------------------------------------------
  class ResponseJob : public XrdCl::Job
  {
    public:
      //-----------------------------------------------------------------------
      // Constructor
      //-----------------------------------------------------------------------
      ResponseJob( XrdCl::ResponseHandler *handler,
                   XrdCl::XRootDStatus    *status,
                   XrdCl::AnyObject       *response ):
        pHandler( handler ), pStatus( status ), pResponse( response )
      {
      }

      virtual void Run( void *arg )
      {
        pHandler->HandleResponse( pStatus, pResponse );
        delete this;
      }

    private:

      XrdCl::ResponseHandler *pHandler;  //< user callback
      XrdCl::XRootDStatus    *pStatus;   //< operation status
      XrdCl::AnyObject       *pResponse; //< user response
  };

  //---------------------------------------------------------------------------
  // A utility function for scheduling read operation handler
  //---------------------------------------------------------------------------
  void ScheduleHandler( uint64_t offset, uint32_t size, void *buffer, XrdCl::ResponseHandler *handler )
  {
    if( !handler ) return;

    XrdCl::ChunkInfo *chunk = new XrdCl::ChunkInfo();
    chunk->offset = offset;
    chunk->length = size;
    chunk->buffer = buffer;

    XrdCl::AnyObject *resp = new XrdCl::AnyObject();
    resp->Set( chunk );

    ResponseJob *job = new ResponseJob( handler, new XrdCl::XRootDStatus(), resp );
    XrdCl::DefaultEnv::GetPostMaster()->GetJobManager()->QueueJob( job );
  }

  //---------------------------------------------------------------------------
  // A utility function for scheduling an operation handler
  //---------------------------------------------------------------------------
  void ScheduleHandler( XrdCl::ResponseHandler *handler, const XrdCl::XRootDStatus &st )
  {
    if( !handler ) return;

    ResponseJob *job = new ResponseJob( handler, new XrdCl::XRootDStatus( st ), 0 );
    XrdCl::DefaultEnv::GetPostMaster()->GetJobManager()->QueueJob( job );
  }

}
