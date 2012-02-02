//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClQueryExecutor.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  QueryExecutor::QueryExecutor( const URL &url, Env *env ):
    pQuery( 0 )
  {
    pQuery = new Query( url );
    if( env )
      pEnv = env;
    else
      pEnv = new Env();
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  QueryExecutor::~QueryExecutor()
  {
    delete pQuery;
    delete pEnv;
  }

  //---------------------------------------------------------------------------
  // Add a command to the set of known commands
  //---------------------------------------------------------------------------
  bool QueryExecutor::AddCommand( const std::string &name, Command command )
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
  XRootDStatus QueryExecutor::Execute( const std::string &commandline )
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

    return it->second( pQuery, pEnv, args );
  }
}
