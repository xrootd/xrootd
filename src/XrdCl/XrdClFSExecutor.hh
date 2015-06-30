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
      //! @param url the server that the executor should contact
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
      //! @param args : arguments for the commandline to be executed,
      //!                    first of which is the command name
      //! @return            status of the execution
      //------------------------------------------------------------------------
      XRootDStatus Execute( const CommandParams & args);

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
