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
#ifndef __XRD_CL_LOCAL_FILE_TASK_HH__
#define __XRD_CL_LOCAL_FILE_TASK_HH__

#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClAnyObject.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

namespace XrdCl{
   class LocalFileTask : public Job{

      public:
      LocalFileTask(  XRootDStatus *st, AnyObject *obj, HostList *hosts, ResponseHandler *responsehandler );
      ~LocalFileTask();
      virtual void Run( void *arg );

   private:
      XRootDStatus *st;
      AnyObject *obj;
      HostList *hosts;
      ResponseHandler *responsehandler;
    };
}

#endif
