//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClQuery.hh"
#include "XrdCl/XrdClQueryExecutor.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <ctime>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

using namespace XrdClient;

//------------------------------------------------------------------------------
// Build a path
//------------------------------------------------------------------------------
XRootDStatus BuildPath( std::string &newPath, Env *env,
                        const std::string &path )
{
  if( path.empty() )
    return XRootDStatus( stError, errInvalidArgs );

  if( path[0] == '/' )
  {
    newPath = path;
    return XRootDStatus();
  }

  std::string cwd = "/";
  env->GetString( "CWD", cwd );
  newPath  = cwd;
  newPath += "/";
  newPath += path;

  //----------------------------------------------------------------------------
  // Collapse the dots
  //----------------------------------------------------------------------------
  std::list<std::string> pathComponents;
  std::list<std::string>::iterator it;
  XrdClient::Utils::splitString( pathComponents, newPath, "/" );
  newPath = "/";
  for( it = pathComponents.begin(); it != pathComponents.end(); )
  {
    if( *it == "." )
    {
      it = pathComponents.erase( it );
      continue;
    }

    if( *it == ".." )
    {
      if( it == pathComponents.begin() )
        return XRootDStatus( stError, errInvalidArgs );
      std::list<std::string>::iterator it1 = it;
      --it1;
      it = pathComponents.erase( it1 );
      it = pathComponents.erase( it );
      continue;
    }
    ++it;
  }

  newPath = "/";
  for( it = pathComponents.begin(); it != pathComponents.end(); ++it )
  {
    newPath += *it;
    newPath += "/";
  }

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Convert mode string to uint16_t
//------------------------------------------------------------------------------
XRootDStatus ConvertMode( uint16_t &mode, const std::string &modeStr )
{
  if( modeStr.length() != 9 )
    return XRootDStatus( stError, errInvalidArgs );

  mode = 0;
  for( int i = 0; i < 3; ++i )
  {
    if( modeStr[i] == 'r' )
      mode |= Access::UR;
    else if( modeStr[i] == 'w' )
      mode |= Access::UW;
    else if( modeStr[i] == 'x' )
      mode |= Access::UX;
    else if( modeStr[i] != '-' )
      return XRootDStatus( stError, errInvalidArgs );
  }
  for( int i = 3; i < 6; ++i )
  {
    if( modeStr[i] == 'r' )
      mode |= Access::GR;
    else if( modeStr[i] == 'w' )
      mode |= Access::GW;
    else if( modeStr[i] == 'x' )
      mode |= Access::GX;
    else if( modeStr[i] != '-' )
      return XRootDStatus( stError, errInvalidArgs );
  }
  for( int i = 6; i < 9; ++i )
  {
    if( modeStr[i] == 'r' )
      mode |= Access::OR;
    else if( modeStr[i] == 'w' )
      mode |= Access::OW;
    else if( modeStr[i] == 'x' )
      mode |= Access::OX;
    else if( modeStr[i] != '-' )
      return XRootDStatus( stError, errInvalidArgs );
  }
  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Change current working directory
//------------------------------------------------------------------------------
XRootDStatus DoCD( Query *query, Env *env,
                   const QueryExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log *log = DefaultEnv::GetLog();
  if( args.size() != 2 )
  {
    log->Error( AppMsg, "Invalid arguments. Expected a path." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  std::string newPath;
  if( !BuildPath( newPath, env, args[1] ).IsOK() )
  {
    log->Error( AppMsg, "Invalid path." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  //----------------------------------------------------------------------------
  // Check if the path exist and is not a directory
  //----------------------------------------------------------------------------
  StatInfo *info;
  XRootDStatus st = query->Stat( newPath, StatFlags::Object, info );
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable to stat the path: %s", st.ToStr().c_str() );
    return st;
  }

  if( !info->TestFlags( StatInfo::IsDir ) )
  {
    log->Error( AppMsg, "%s is not a directory.", newPath.c_str() );
    return XRootDStatus( stError, errInvalidArgs );
  }

  env->PutString( "CWD", newPath );
  return XRootDStatus();
}

//------------------------------------------------------------------------------
// List a directory
//------------------------------------------------------------------------------
XRootDStatus DoLS( Query *query, Env *env,
                   const QueryExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log *log = DefaultEnv::GetLog();
  uint32_t    argc = args.size();
  uint8_t     flags = DirListFlags::Locate;
  bool        stats = false;
  std::string path;

  if( argc > 3 )
  {
    log->Error( AppMsg, "Too many arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  QueryExecutor::CommandParams::const_iterator argIt = args.begin();
  ++argIt;
  for( ; argIt != args.end(); ++argIt )
  {
    if( *argIt == "-l" )
    {
      stats = true;
      flags |= DirListFlags::Stat;
    }
    else
      path = *argIt;
  }

  std::string newPath;
  if( path.empty() )
    env->GetString( "CWD", newPath );
  else
  {
    if( !BuildPath( newPath, env, path ).IsOK() )
    {
      log->Error( AppMsg, "Invalid arguments. Invalid path." );
      return XRootDStatus( stError, errInvalidArgs );
    }
  }

  log->Debug( AppMsg, "Attempting to list: %s", newPath.c_str() );

  //----------------------------------------------------------------------------
  // Ask for the list
  //----------------------------------------------------------------------------
  DirectoryList *list;
  XRootDStatus st = query->DirList( newPath, flags, list );
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable to list the path: %s", st.ToStr().c_str() );
    return st;
  }

  if( st.code == suPartial )
    log->Info( AppMsg, "Some of the requests failed. The result may be "
                       "incomplete" );

  //----------------------------------------------------------------------------
  // Print the results
  //----------------------------------------------------------------------------
  DirectoryList::Iterator it;
  for( it = list->Begin(); it != list->End(); ++it )
  {
    if( stats )
    {
      StatInfo *info = (*it)->GetStatInfo();
      if( !info )
      {
        std::cout << "---- 0000-00-00 00:00:00            ? ";
      }
      else
      {
        if( info->TestFlags( StatInfo::IsDir ) )
          std::cout << "d";
        else
          std::cout << "-";

        if( info->TestFlags( StatInfo::IsReadable ) )
          std::cout << "r";
        else
          std::cout << "-";

        if( info->TestFlags( StatInfo::IsWritable ) )
          std::cout << "w";
        else
          std::cout << "-";

        if( info->TestFlags( StatInfo::XBitSet ) )
          std::cout << "x";
        else
          std::cout << "-";

        char ts[256];
        time_t modTime = info->GetModTime();
        tm *t = gmtime( &modTime );
        strftime( ts, 255, "%F %T", t );
        std::cout << " " << ts;

        std::cout << std::setw(12) << info->GetSize() << " ";
      }
    }
    std::cout << "root://" << (*it)->GetHostAddress() << "/";
    std::cout << list->GetParentName() << (*it)->GetName() << std::endl;
  }

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Create a directory
//------------------------------------------------------------------------------
XRootDStatus DoMkDir( Query *query, Env *env,
                      const QueryExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log         *log     = DefaultEnv::GetLog();
  uint32_t     argc    = args.size();

  if( argc < 2 || argc > 4 )
  {
    log->Error( AppMsg, "Too few arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  uint8_t      flags   = MkDirFlags::None;
  uint16_t     mode    = 0;
  std::string  modeStr = "rwxr-x---";
  std::string  path    = "";

  for( int i = 1; i < args.size(); ++i )
  {
    if( args[i] == "-p" )
      flags |= MkDirFlags::MakePath;
    else if( !args[i].compare( 0, 2, "-m" ) )
      modeStr = args[i].substr( 2, 9 );
    else
      path = args[i];
  }

  XRootDStatus st = ConvertMode( mode, modeStr );
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Invalid mode string." );
    return st;
  }

  std::string newPath;
  if( !BuildPath( newPath, env, path ).IsOK() )
  {
    log->Error( AppMsg, "Invalid path." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  //----------------------------------------------------------------------------
  // Run the query
  //----------------------------------------------------------------------------
  st = query->MkDir( newPath, flags, mode );
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable create directory %s: %s",
                        newPath.c_str(),
                        st.ToStr().c_str() );
    return st;
  }

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Remove a directory
//------------------------------------------------------------------------------
XRootDStatus DoRmDir( Query *query, Env *env,
                      const QueryExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log         *log     = DefaultEnv::GetLog();
  uint32_t     argc    = args.size();

  if( argc != 2 )
  {
    log->Error( AppMsg, "Wrong number of arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  std::string fullPath;
  if( !BuildPath( fullPath, env, args[1] ).IsOK() )
  {
    log->Error( AppMsg, "Invalid path." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  //----------------------------------------------------------------------------
  // Run the query
  //----------------------------------------------------------------------------
  XRootDStatus st = query->RmDir( fullPath );
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable remove directory %s: %s",
                        fullPath.c_str(),
                        st.ToStr().c_str() );
    return st;
  }

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Move a file or directory
//------------------------------------------------------------------------------
XRootDStatus DoMv( Query *query, Env *env,
                   const QueryExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log         *log     = DefaultEnv::GetLog();
  uint32_t     argc    = args.size();

  if( argc != 3 )
  {
    log->Error( AppMsg, "Wrong number of arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  std::string fullPath1;
  if( !BuildPath( fullPath1, env, args[1] ).IsOK() )
  {
    log->Error( AppMsg, "Invalid source path." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  std::string fullPath2;
  if( !BuildPath( fullPath2, env, args[2] ).IsOK() )
  {
    log->Error( AppMsg, "Invalid destination path." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  //----------------------------------------------------------------------------
  // Run the query
  //----------------------------------------------------------------------------
  XRootDStatus st = query->Mv( fullPath1, fullPath2 );
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable move %s to %s: %s",
                        fullPath1.c_str(), fullPath2.c_str(),
                        st.ToStr().c_str() );
    return st;
  }

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Print help
//------------------------------------------------------------------------------
XRootDStatus PrintHelp( Query *query, Env *env,
                        const QueryExecutor::CommandParams &args )
{
  std::cout << "Usage:" << std::endl;
  std::cout << "   xrdquery host[:port]              - interactive mode";
  std::cout << std::endl;
  std::cout << "   xrdquery host[:port] command args - batch mode";
  std::cout << std::endl << std::endl;

  std::cout << "Available commands:" << std::endl << std::endl;

  std::cout << "   chmod <path> <user><group><other>" << std::endl;
  std::cout << "     Modify file permissions. Permission example:";
  std::cout << std::endl;
  std::cout << "     rwxr-x--x" << std::endl << std::endl;

  std::cout << "   cd <path>" << std::endl;
  std::cout << "     Change the current working directory";
  std::cout << std::endl << std::endl;

  std::cout << "   ls [-l] [dirname]" << std::endl;
  std::cout << "     Get directory listing." << std::endl << std::endl;

  std::cout << "   exit" << std::endl;
  std::cout << "     Exits from the program." << std::endl << std::endl;

  std::cout << "   help" << std::endl;
  std::cout << "     This help screen." << std::endl << std::endl;

  std::cout << "   stat <path>" << std::endl;
  std::cout << "     Get info about the file or directory." << std::endl;
  std::cout << std::endl;

  std::cout << "   statvfs [path]" << std::endl;
  std::cout << "     Get info about a virtual file system." << std::endl;
  std::cout << std::endl;

  std::cout << "   locate <path> [NoWait|Refresh]" << std::endl;
  std::cout << "     Get the locations of the path." << std::endl;
  std::cout << std::endl;

  std::cout << "   deep-locate <path> [NoWait|Refresh]" << std::endl;
  std::cout << "     Find file servers hosting the path." << std::endl;
  std::cout << std::endl;

  std::cout << "   mv <path1> <path2>" << std::endl;
  std::cout << "     Move path1 to path2 locally on the same server.";
  std::cout << std::endl << std::endl;

  std::cout << "   mkdir <dirname> [-p] [-m<user><group><other>]";
  std::cout << std::endl;
  std::cout << "     Creates a directory/tree of directories.";
  std::cout << std::endl << std::endl;

  std::cout << "   rm <filename>" << std::endl;
  std::cout << "     Remove a file." << std::endl << std::endl;

  std::cout << "   rmdir <dirname>" << std::endl;
  std::cout << "     Remove a directory." << std::endl << std::endl;

  std::cout << "   query <code> <parms>";
  std::cout << "Obtain server information. Query codes:" << std::endl;

  std::cout << "     Config <what>              ";
  std::cout << "Query server configuration"     << std::endl;

  std::cout << "     ChecksumCancel <path>      ";
  std::cout << "File checksum cancelation"      << std::endl;

  std::cout << "     Checksum <path>            ";
  std::cout << "Query file checksum"            << std::endl;

  std::cout << "     Opaque <arg>               ";
  std::cout << "Implementation dependent"       << std::endl;

  std::cout << "     OpaqueFile <arg>           ";
  std::cout << "Implementation dependent"       << std::endl;

  std::cout << "     Space <spacename>          ";
  std::cout << "Query logical space stats"      << std::endl;

  std::cout << "     Stats <what>               ";
  std::cout << "Query server stats"             << std::endl;

  std::cout << "     XAttr <path>               ";
  std::cout << "Query file extended attributes" << std::endl;
  std::cout << std::endl;

  std::cout << "   truncate <filename> <length> " << std::endl;
  std::cout << "     Truncate a file." << std::endl << std::endl;

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Create the executor object
//------------------------------------------------------------------------------
QueryExecutor *CreateExecutor( const URL &url )
{
  Env *env = new Env();
  env->PutString( "CWD", "/" );
  QueryExecutor *executor = new QueryExecutor( url, env );
  executor->AddCommand( "cd",          DoCD      );
  executor->AddCommand( "chmod",       PrintHelp );
  executor->AddCommand( "ls",          DoLS      );
  executor->AddCommand( "help",        PrintHelp );
  executor->AddCommand( "stat",        PrintHelp );
  executor->AddCommand( "statvfs",     PrintHelp );
  executor->AddCommand( "locate",      PrintHelp );
  executor->AddCommand( "deep-locate", PrintHelp );
  executor->AddCommand( "mv",          DoMv      );
  executor->AddCommand( "mkdir",       DoMkDir   );
  executor->AddCommand( "rm",          PrintHelp );
  executor->AddCommand( "rmdir",       DoRmDir   );
  executor->AddCommand( "query",       PrintHelp );
  executor->AddCommand( "truncate",    PrintHelp );
  return executor;
}

//------------------------------------------------------------------------------
// Execute command
//------------------------------------------------------------------------------
int ExecuteCommand( QueryExecutor *ex, const std::string &commandline )
{
  Log *log = DefaultEnv::GetLog();
  XRootDStatus st = ex->Execute( commandline );
  if( !st.IsOK() )
    log->Error( AppMsg, "Error executing %s: %s", commandline.c_str(),
                        st.ToStr().c_str() );
  return st.GetShellCode();
}

//------------------------------------------------------------------------------
// Define some functions required to function when build without readline
//------------------------------------------------------------------------------
#ifndef HAVE_READLINE
char *readline(const char *prompt)
{
    std::cout << prompt << std::flush;
    std::string input;
    std::getline( std::cin, input );

    if( !std::cin.good() )
        return 0;

    char *linebuf = (char *)malloc( input.size()+1 );
    strncpy( linebuf, input.c_str(), input.size()+1 );

    return linebuf;
}

void add_history( const char * )
{
}

void rl_bind_key( char c, uint16_t action )
{
}

uint16_t rl_abort = 0;

int read_history( const char * )
{
  return 0;
}

int write_history( const char * )
{
  return 0;
}
#endif

//------------------------------------------------------------------------------
// Build the command prompt
//------------------------------------------------------------------------------
std::string BuildPrompt( Env *env, const URL &url )
{
  std::ostringstream prompt;
  std::string cwd = "/";
  env->GetString( "CWD", cwd );
  prompt << "[" << url.GetHostId() << "] " << cwd << " > ";
  return prompt.str();
}

//------------------------------------------------------------------------------
// Execute interactive
//------------------------------------------------------------------------------
int ExecuteInteractive( const URL &url )
{
  //----------------------------------------------------------------------------
  // Set up the environment
  //----------------------------------------------------------------------------
  std::string historyFile = getenv( "HOME" );
  historyFile += "/.xrdquery.history";
  rl_bind_key( '\t', rl_abort );
  read_history( historyFile.c_str() );
  QueryExecutor *ex = CreateExecutor( url );

  //----------------------------------------------------------------------------
  // Execute the commands
  //----------------------------------------------------------------------------
  while(1)
  {
    char *linebuf = 0;
    linebuf = readline( BuildPrompt( ex->GetEnv(), url ).c_str() );
    if( !linebuf || !strcmp( linebuf, "exit" ))
    {
      std::cout << "Goodbye." << std::endl << std::endl;
      break;
    }
    if( !*linebuf)
    {
      free( linebuf );
      continue;
    }
    Status xs = XRootDStatus();
    ex->Execute( linebuf );
    add_history( linebuf );
    free( linebuf );
  }

  //----------------------------------------------------------------------------
  // Cleanup
  //----------------------------------------------------------------------------
  delete ex;
  write_history( historyFile.c_str() );
  return 0;
}

//------------------------------------------------------------------------------
// Execute command
//------------------------------------------------------------------------------
int ExecuteCommand( const URL &url, int argc, char **argv )
{
  //----------------------------------------------------------------------------
  // Build the command to be executed
  //----------------------------------------------------------------------------
  std::string commandline;
  for( int i = 0; i < argc; ++i )
  {
    commandline += argv[i];
    commandline += " ";
  }

  QueryExecutor *ex = CreateExecutor( url );
  int st = ExecuteCommand( ex, commandline );
  delete ex;
  return st;
}

//------------------------------------------------------------------------------
// Start the show
//------------------------------------------------------------------------------
int main( int argc, char **argv )
{
  //----------------------------------------------------------------------------
  // Check the commandline parameters
  //----------------------------------------------------------------------------
  XrdClient::QueryExecutor::CommandParams params;
  if( argc == 1 )
  {
    PrintHelp( 0, 0, params );
    return 1;
  }

  if( !strcmp( argv[1], "--help" ) ||
      !strcmp( argv[1], "-h" ) )
  {
    PrintHelp( 0, 0, params );
    return 0;
  }

  URL url( argv[1] );
  if( !url.IsValid() )
  {
    PrintHelp( 0, 0, params );
    return 1;
  }

  if( argc == 2 )
    return ExecuteInteractive( url );
  return ExecuteCommand( url, argc-2, argv+2 );
}
