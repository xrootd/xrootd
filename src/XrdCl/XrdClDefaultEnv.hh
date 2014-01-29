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
#include "XrdVersion.hh"

class XrdSysPlugin;

namespace XrdCl
{
  class PostMaster;
  class Log;
  class ForkHandler;
  class Monitor;
  class CheckSumManager;
  class TransportManager;
  class FileTimer;
  class PlugInManager;

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
      //! Set log level
      //!
      //! @param level Dump, Debug, Info, Warning or Error
      //------------------------------------------------------------------------
      static void SetLogLevel( const std::string &level );

      //------------------------------------------------------------------------
      //! Set log file
      //!
      //! @param filepath path to the log file
      //------------------------------------------------------------------------
      static bool SetLogFile( const std::string &filepath );

      //------------------------------------------------------------------------
      //! Set log mask.
      //! Determines which diagnostics topics should be printed. It's a
      //! "|" separated list of topics. The first element may be "All" in which
      //! case all the topics are enabled and the subsequent elements may turn
      //! them off, or "None" in which case all the topics are disabled and
      //! the subsequent flags may turn them on. If the topic name is prefixed
      //! with "^", then it means that the topic should be disabled. If the
      //! topic name is not prefixed, then it means that the topic should be
      //! enabled.
      //!
      //! The default for each level is "All", except for the "Dump" level,
      //! where the default is "All|^PollerMsg". This means that, at the
      //! "Dump" level, all the topics but "PollerMsg" are enabled.
      //!
      //! Available topics: AppMsg, UtilityMsg, FileMsg, PollerMsg,
      //! PostMasterMsg, XRootDTransportMsg, TaskMgrMsg, XRootDMsg,
      //! FileSystemMsg, AsyncSockMsg
      //!
      //! @param level log level or "All" for all levels
      //! @param mask  log mask
      //------------------------------------------------------------------------
      static void SetLogMask( const std::string &level,
                              const std::string &mask );

      //------------------------------------------------------------------------
      //! Get the fork handler
      //------------------------------------------------------------------------
      static ForkHandler *GetForkHandler();

      //------------------------------------------------------------------------
      //! Get file timer task
      //------------------------------------------------------------------------
      static FileTimer *GetFileTimer();

      //------------------------------------------------------------------------
      //! Get the monitor object
      //------------------------------------------------------------------------
      static Monitor *GetMonitor();

      //------------------------------------------------------------------------
      //! Get checksum manager
      //------------------------------------------------------------------------
      static CheckSumManager *GetCheckSumManager();

      //------------------------------------------------------------------------
      //! Get transport manager
      //------------------------------------------------------------------------
      static TransportManager *GetTransportManager();

      //------------------------------------------------------------------------
      //! Get plug-in manager
      //------------------------------------------------------------------------
      static PlugInManager *GetPlugInManager();

      //------------------------------------------------------------------------
      //! Initialize the environment
      //------------------------------------------------------------------------
      static void Initialize();

      //------------------------------------------------------------------------
      //! Finalize the environment
      //------------------------------------------------------------------------
      static void Finalize();

      //------------------------------------------------------------------------
      //! Re-initialize the logging
      //------------------------------------------------------------------------
      static void ReInitializeLogging();

    private:
      static void SetUpLog();

      static XrdSysMutex        sInitMutex;
      static Env               *sEnv;
      static PostMaster        *sPostMaster;
      static Log               *sLog;
      static ForkHandler       *sForkHandler;
      static FileTimer         *sFileTimer;
      static Monitor           *sMonitor;
      static XrdSysPlugin      *sMonitorLibHandle;
      static bool               sMonitorInitialized;
      static CheckSumManager   *sCheckSumManager;
      static TransportManager  *sTransportManager;
      static PlugInManager     *sPlugInManager;
  };
}

#endif // __XRD_CL_DEFAULT_ENV_HH__
