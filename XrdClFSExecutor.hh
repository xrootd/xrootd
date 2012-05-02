//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_FS_EXECUTOR_HH__
#define __XRD_CL_FS_EXECUTOR_HH__

#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClEnv.hh"
#include "XrdCl/XrdClUtils.hh"
#include <vector>
#include <string>
#include <map>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Execute queries given as a commandline
  //----------------------------------------------------------------------------
  class FSExecutor
  {
    public:
      //------------------------------------------------------------------------
      //! Definition of command argument list
      //------------------------------------------------------------------------
      typedef std::vector<std::string> CommandParams;

      //------------------------------------------------------------------------
      //! Definition of a command
      //------------------------------------------------------------------------
      typedef XRootDStatus (*Command)( FileSystem          *fs,
                                       Env                 *env,
                                       const CommandParams &args );

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param url the sercer that the executor should contact
      //! @param env execution environment, the executor takes ownership over it
      //------------------------------------------------------------------------
      FSExecutor( const URL &url, Env *env = 0 );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~FSExecutor();

      //------------------------------------------------------------------------
      //! Add a command to the set of known commands
      //!
      //! @param name    name of the command
      //! @param command function pointer
      //! @return        status
      //------------------------------------------------------------------------
      bool AddCommand( const std::string &name, Command command );

      //------------------------------------------------------------------------
      //! Execute the given commandline
      //!
      //! @param commandline the commandline to be executed, a space separated
      //!                    list of parameters, first of which is the command
      //!                    name
      //! @return            status of the execution
      //------------------------------------------------------------------------
      XRootDStatus Execute( const std::string &commandline );

      //------------------------------------------------------------------------
      //! Get the environment
      //------------------------------------------------------------------------
      Env *GetEnv()
      {
        return pEnv;
      }

    private:
      typedef std::map<std::string, Command> CommandMap;
      FileSystem  *pFS;
      Env         *pEnv;
      CommandMap   pCommands;
  };
}

#endif // __XRD_CL_FS_EXECUTOR_HH__
