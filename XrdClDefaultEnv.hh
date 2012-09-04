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

#ifndef __XRD_CL_DEFAULT_ENV_HH__
#define __XRD_CL_DEFAULT_ENV_HH__

#include "XrdSys/XrdSysPthread.hh"
#include "XrdCl/XrdClEnv.hh"

namespace XrdCl
{
  class PostMaster;
  class Log;

  //----------------------------------------------------------------------------
  //! Default environment for the client. Responsible for setting/importing
  //! defaults for the variables used by the client. And holding other
  //! global stuff.
  //----------------------------------------------------------------------------
  class DefaultEnv: public Env
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      DefaultEnv();

      //------------------------------------------------------------------------
      //! Get default client environment
      //------------------------------------------------------------------------
      static Env *GetEnv();

      //------------------------------------------------------------------------
      //! Get default post master
      //------------------------------------------------------------------------
      static PostMaster *GetPostMaster();

      //------------------------------------------------------------------------
      //! Get default log
      //------------------------------------------------------------------------
      static Log *GetLog();

      //------------------------------------------------------------------------
      //! Free the globals, called by a static finalizer, no need to call
      //! by hand
      //------------------------------------------------------------------------
      static void Release();

    private:
      static XrdSysMutex  sEnvMutex;
      static Env         *sEnv;
      static XrdSysMutex  sPostMasterMutex;
      static PostMaster  *sPostMaster;
      static Log         *sLog;
  };
}

#endif // __XRD_CL_DEFAULT_ENV_HH__
