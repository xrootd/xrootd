//------------------------------------------------------------------------------
// Copyright (c) 2017 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH 
// Author: Paul-Niklas Kramp <p.n.kramp@gsi.de>
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
#include "XrdCl/XrdClLocalFileTask.hh"

namespace XrdCl
{
   LocalFileTask::LocalFileTask( XRootDStatus *st, AnyObject *obj, HostList *hosts, ResponseHandler *responsehandler )
   {
      this->st = st;
      this->obj = obj;
      this->hosts = hosts;
      this->responsehandler = responsehandler;
   }

   LocalFileTask::~LocalFileTask(){}

   void LocalFileTask::Run( void *arg )
   {
      if( responsehandler )
         responsehandler->HandleResponseWithHosts( st, obj, hosts );
      else{
         delete st;
         delete obj;
         delete hosts;
      }
      delete this;
   }
}
