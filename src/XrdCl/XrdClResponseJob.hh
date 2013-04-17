//------------------------------------------------------------------------------
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
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

#ifndef __XRD_CL_RESPONSE_JOB_HH__
#define __XRD_CL_RESPONSE_JOB_HH__

#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Call the user callback
  //----------------------------------------------------------------------------
  class ResponseJob: public Job
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      ResponseJob( ResponseHandler *handler,
                   XRootDStatus    *status,
                   AnyObject       *response,
                   HostList        *hostList ):
        pHandler( handler ), pStatus( status ), pResponse( response ),
        pHostList( hostList )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~ResponseJob()
      {
      }


      //------------------------------------------------------------------------
      //! Run the user handler
      //------------------------------------------------------------------------
      virtual void Run( void *arg )
      {
        pHandler->HandleResponseWithHosts( pStatus, pResponse, pHostList );
        delete this;
      }

    private:
      ResponseHandler *pHandler;
      XRootDStatus    *pStatus;
      AnyObject       *pResponse;
      HostList        *pHostList;
  };
}

#endif // __XRD_CL_RESPONSE_JOB_HH__
