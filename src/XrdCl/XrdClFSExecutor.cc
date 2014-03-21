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

#include "XrdCl/XrdClFSExecutor.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  FSExecutor::FSExecutor( const URL &url, Env *env ):
    pFS( 0 )
  {
    pFS = new FileSystem( url );
    if( env )
      pEnv = env;
    else
      pEnv = new Env();

    pEnv->PutString( "ServerURL", url.GetURL() );
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  FSExecutor::~FSExecutor()
  {
    delete pFS;
    delete pEnv;
  }

  //---------------------------------------------------------------------------
  // Add a command to the set of known commands
  //---------------------------------------------------------------------------
  bool FSExecutor::AddCommand( const std::string &name, Command command )
  {
    Log *log = DefaultEnv::GetLog();
    CommandMap::iterator it = pCommands.find( name );
    if( it != pCommands.end() )
    {
      log->Error( AppMsg, "Unable to register command %s. Already exists.",
                          name.c_str() );
      return false;
    }
    pCommands.insert( std::make_pair( name, command ) );
    return true;
  }

  //----------------------------------------------------------------------------
  // Execute the given commandline
  //----------------------------------------------------------------------------
  XRootDStatus FSExecutor::Execute( const std::string &commandline )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( AppMsg, "Executing: %s", commandline.c_str() );

    //--------------------------------------------------------------------------
    // Split the commandline string
    //--------------------------------------------------------------------------
    CommandParams args;
    Utils::splitString( args, commandline, " " );
    if( args.empty() )
    {
      log->Dump( AppMsg, "Empty commandline." );
      return 1;
    }

    CommandParams::iterator parIt;
    int i = 0;
    for( parIt = args.begin(); parIt != args.end(); ++parIt, ++i )
      log->Dump( AppMsg, "  Param #%02d: '%s'", i, parIt->c_str() );

    //--------------------------------------------------------------------------
    // Extract the command name
    //--------------------------------------------------------------------------
    std::string commandName = args.front();
    CommandMap::iterator it = pCommands.find( commandName );
    if( it == pCommands.end() )
    {
      log->Error( AppMsg, "Unknown command: %s", commandName.c_str() );
      return XRootDStatus( stError, errUnknownCommand );
    }

    return it->second( pFS, pEnv, args );
  }
}
