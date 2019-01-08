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

#include "XrdClZipListHandler.hh"

namespace XrdCl
{

  void ZipListHandler::HandleResponse( XrdCl::XRootDStatus *statusptr,
                                       XrdCl::AnyObject    *responseptr )
  {
    std::unique_ptr<XRootDStatus> status( statusptr );
    std::unique_ptr<AnyObject>    response( responseptr );

    if( pStep == DONE )
    {
      delete this;
      return;
    }

    if( !status->IsOK() )
    {
      pHandler->HandleResponse( status.release(), response.release() );
      delete this;
      return;
    }

    time_t took = time( 0 ) - pStartTime;
    if( took > pTimeout )
    {
      *status = XRootDStatus( stError, errOperationExpired );
      pHandler->HandleResponse( status.release(), 0 );
      if( pZip.IsOpen() )
      {
        DoZipClose( 1 );
        pStep = DONE;
      }
      else
        delete this;
      return;
    }
    uint16_t left = pTimeout - took;

    switch( pStep )
    {
      case STAT:
      {
        StatInfo *info = 0;
        response->Get( info );

        if( info->TestFlags( StatInfo::IsDir ) )
          DoDirList( left );
        else
          DoZipOpen( left );

        break;
      }

      case OPEN:
      {
        DirectoryList *list = 0;
        XRootDStatus st = pZip.List( list );
        if( !st.IsOK() )
        {
          pHandler->HandleResponse( new XRootDStatus( st ), 0 );
          pStep = DONE;
        }
        else
        {
          pDirList.reset( list );
          DoZipClose( left );
        }
        break;
      }

      case CLOSE:
      {
        AnyObject *resp = new AnyObject();
        resp->Set( pDirList.release() );
        pHandler->HandleResponse( new XRootDStatus(), resp );
        pStep = DONE;
        break;
      }
    }

    if( pStep == DONE )
      delete this;
  }

  void ZipListHandler::DoDirList( time_t timeLeft )
  {
    FileSystem fs( pUrl );
    XRootDStatus st = fs.DirList( pUrl.GetPath(), pFlags, pHandler , timeLeft );
    pStep = DONE; // no matter whether it works or not, either way we are done
    if( !st.IsOK() )
      pHandler->HandleResponse( new XRootDStatus( st ), 0 );
  }

  void ZipListHandler::DoZipOpen( time_t timeLeft )
  {
    XRootDStatus st = pZip.Open( pUrl.GetURL(), this, timeLeft );
    if( !st.IsOK() )
    {
      pHandler->HandleResponse( new XRootDStatus( st ), 0 );
      pStep = DONE;
    }
    else
      pStep = OPEN;
  }

  void ZipListHandler::DoZipClose( time_t timeLeft )
  {
    XRootDStatus st = pZip.Close( this, timeLeft );
    if( !st.IsOK() )
    {
      pHandler->HandleResponse( new XRootDStatus( st ), 0 );
      pStep = DONE;
    }
    else
      pStep = CLOSE;
  }

} /* namespace XrdCl */
