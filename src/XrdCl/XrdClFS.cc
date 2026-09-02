//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
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

#include "XProtocol/XProtocol.hh"
#include "XrdCl/XrdClFSRemove.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClCopyProcess.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClFSCompatibility.hh"
#include "XrdCl/XrdClFSExecutor.hh"
#include "XrdCl/XrdClFSURLCommand.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClFileSystemOperations.hh"
#include "XrdCl/XrdClFileSystemUtils.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClParallelOperation.hh"
#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdOuc/XrdOucPrivateUtils.hh"
#include "XrdOuc/XrdOucJson.hh"
#include "XrdSys/XrdSysE2T.hh"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>

#ifdef HAVE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

using namespace XrdCl;

bool IsXRootDProtocol( Env *env );

namespace
{
  class ScopedGetoptState
  {
    public:
      ScopedGetoptState():
        pOptind( optind ), pOpterr( opterr ), pOptopt( optopt ),
        pOptarg( optarg )
      {
        // optind == 0 requests a complete parser reset with both GNU and BSD
        // getopt implementations. This is required for repeated interactive
        // xrdfs commands and for the earlier top-level option parse.
        optind = 0;
        opterr = 0;
        optopt = 0;
        optarg = nullptr;
      }

      ~ScopedGetoptState()
      {
        optind = pOptind;
        opterr = pOpterr;
        optopt = pOptopt;
        optarg = pOptarg;
      }

    private:
      int   pOptind;
      int   pOpterr;
      int   pOptopt;
      char *pOptarg;
  };

  bool ParseTokenValidity( const char *argument, std::uint64_t &validity )
  {
    if( !argument || !*argument ) return false;
    for( const unsigned char character : std::string( argument ) )
      if( !std::isdigit( character ) ) return false;

    errno = 0;
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull( argument, &end, 10 );
    if( errno == ERANGE || !end || *end ) return false;

    validity = static_cast<std::uint64_t>( parsed );
    return true;
  }

  bool NeedsWebDAVRemovalGuard( Env *env )
  {
    std::string server;
    if( !env->GetString( "ServerURL", server ) ) return false;
    return IsWebDAVProtocol( URL( server ).GetProtocol() );
  }

  XRootDStatus RemovalDecisionStatus(
    NonRecursiveRemovalDecision decision )
  {
    switch( decision )
    {
      case NonRecursiveRemovalDecision::Allow:
        return XRootDStatus();
      case NonRecursiveRemovalDecision::IsDirectory:
        return XRootDStatus(
          stError, errErrorResponse, kXR_isDirectory,
          "Target is a directory; recursive removal was not requested." );
      case NonRecursiveRemovalDecision::NotDirectory:
        return XRootDStatus(
          stError, errErrorResponse, kXR_InvalidRequest,
          "Target is not a directory." );
      case NonRecursiveRemovalDecision::NotEmpty:
        return XRootDStatus(
          stError, errErrorResponse, kXR_ItExists,
          "Directory is not empty." );
    }
    return XRootDStatus( stError, errInternal, 0,
                         "Unknown removal safety decision." );
  }

  XRootDStatus GuardWebDAVRemoval( FileSystem          *fs,
                                   const std::string   &path,
                                   NonRecursiveRemoval  removal )
  {
    StatInfo *rawStat = nullptr;
    XRootDStatus status = fs->Stat( path, rawStat );
    std::unique_ptr<StatInfo> stat( rawStat );
    if( !status.IsOK() ) return status;
    if( !IsCompleteSuccess( status ) )
      return XRootDStatus( stError, errInvalidResponse, 0,
                           "Stat did not return a complete response." );
    if( !stat )
      return XRootDStatus( stError, errInvalidResponse, 0,
                           "Stat succeeded without target metadata." );

    const bool isDirectory = stat->TestFlags( StatInfo::IsDir );
    if( removal == NonRecursiveRemoval::File )
      return RemovalDecisionStatus( EvaluateNonRecursiveRemoval(
        removal, isDirectory, 0 ) );

    if( !isDirectory )
      return RemovalDecisionStatus( EvaluateNonRecursiveRemoval(
        removal, false, 0 ) );

    DirectoryList *rawList = nullptr;
    status = fs->DirList( path, DirListFlags::None, rawList );
    std::unique_ptr<DirectoryList> list( rawList );
    if( !status.IsOK() ) return status;
    if( !IsCompleteSuccess( status ) )
      return XRootDStatus( stError, errInvalidResponse, 0,
                           "Directory listing was incomplete." );
    if( !list )
      return XRootDStatus( stError, errInvalidResponse, 0,
                           "Directory listing succeeded without a response." );

    return RemovalDecisionStatus( EvaluateNonRecursiveRemoval(
      removal, true, list->GetSize() ) );
  }
}

//------------------------------------------------------------------------------
// Build a path
//------------------------------------------------------------------------------
XRootDStatus BuildPath( std::string &newPath, Env *env,
                        const std::string &path,
                        const char *op = nullptr )
{
  Log *log = DefaultEnv::GetLog();

  if( path.empty() )
  {
    const std::string msg = "A path is required.";
    log->Error( AppMsg, "%s", msg.c_str() );
    return XRootDStatus( stError, errInvalidArgs, 0, msg );
  }

  int noCwd = 0;
  env->GetInt( "NoCWD", noCwd );

  if( path[0] == '/' )
  {
    newPath = path;
    return XRootDStatus();
  }
  else if( noCwd )
  {
    std::string msg;
    if( op )
      msg = std::string( op ) + " relative path '" + path + "' is disallowed.";
    else
      msg = "Relative path '" + path + "' is disallowed.";
    log->Error( AppMsg, "%s", msg.c_str() );
    return XRootDStatus( stError, errInvalidArgs, 0, msg );
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
  XrdCl::Utils::splitString( pathComponents, newPath, "/" );
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
      {
        const std::string msg = "Path '" + path + "' escapes above root.";
        log->Error( AppMsg, "%s", msg.c_str() );
        return XRootDStatus( stError, errInvalidArgs, 0, msg );
      }
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
  if( newPath.length() > 1 )
    newPath.erase( newPath.length()-1, 1 );

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Perform a cache operation
//------------------------------------------------------------------------------
XRootDStatus DoCache( FileSystem                      *fs,
                      Env                             *env,
                      const FSExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log         *log     = DefaultEnv::GetLog();
  uint32_t     argc    = args.size();

  if( argc != 3 )
  {
    log->Error( AppMsg, "Wrong number of arguments." );
    return XRootDStatus( stError, errInvalidArgs, 0,
                                  "Wrong number of arguments." );
  }

  if( args[1] != "evict" && args[1] != "fevict")
  {
    log->Error( AppMsg, "Invalid cache operation." );
    return XRootDStatus( stError, errInvalidArgs, 0, "Invalid cache operation." );
  }

  std::string fullPath;
  XRootDStatus pathSt = BuildPath( fullPath, env, args[2], "Caching" );
  if( !pathSt.IsOK() )
    return pathSt;

  //----------------------------------------------------------------------------
  // Create the command 
  //----------------------------------------------------------------------------
  std::string cmd = args[1];
  cmd.append(" ");
  cmd.append(fullPath);

  //----------------------------------------------------------------------------
  // Run the operation
  //----------------------------------------------------------------------------
  Buffer *response = 0;
  XRootDStatus st = fs->SendCache( cmd, response );
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable set cache %s: %s",
                        fullPath.c_str(),
                        st.ToStr().c_str() );
    return st;
  }

  if( response )
  {
    std::cout << response->ToString() << '\n';
  }

  delete response;

  return XRootDStatus();
}
//------------------------------------------------------------------------------
// Change current working directory
//------------------------------------------------------------------------------
XRootDStatus DoCD( FileSystem                      *fs,
                   Env                             *env,
                   const FSExecutor::CommandParams &args )
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

  //----------------------------------------------------------------------------
  // cd excludes NoCWD
  //----------------------------------------------------------------------------
  env->PutInt( "NoCWD", 0 );

  std::string newPath;
  XRootDStatus pathSt = BuildPath( newPath, env, args[1], "Changing" );
  if( !pathSt.IsOK() )
    return pathSt;

  //----------------------------------------------------------------------------
  // Check if the path exist and is not a directory
  //----------------------------------------------------------------------------
  StatInfo *info;
  XRootDStatus st = fs->Stat( newPath, info );
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
  delete info;
  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Helper function to calculate number of digits in a number
//------------------------------------------------------------------------------
uint32_t nbDigits( uint64_t nb )
{
  if( nb == 0 ) return 1;
  return uint32_t( log10( double(nb) ) + 1);
}

std::string getSizeStr(uint64_t size, bool human, uint64_t base) {
  std::ostringstream oss;
  if (!human) {
    oss << size;
  } else {
    oss << XrdOucUtils::genHumanSize(size,base);
  }
  return oss.str();
}

bool IsGFALVirtualXAttr( const std::string &attribute );
XRootDStatus GetGFALVirtualXAttr( FileSystem        *fs,
                                 Env               *env,
                                 const std::string &path,
                                 const std::string &attribute,
                                 std::string       &value );
XRootDStatus GetNativeXAttrValue( FileSystem        *fs,
                                 const std::string &path,
                                 const std::string &attribute,
                                 std::string       &value );

void PrintDirListStatInfo( StatInfo *info, bool hascks = false, uint32_t ownerwidth = 0, uint32_t groupwidth = 0, uint32_t sizewidth = 0, bool human = false, uint64_t base = 1000 )
{
  if( info->ExtendedFormat() )
  {
    if( info->TestFlags( StatInfo::IsDir ) )
      std::cout << "d";
    else
      std::cout << "-";
    std::cout << info->GetModeAsOctString();

    std::cout << " " << std::setw( ownerwidth ) << info->GetOwner();
    std::cout << " " << std::setw( groupwidth ) << info->GetGroup();
    std::cout << " " << std::setw( sizewidth ) << getSizeStr(info->GetSize(),human, base);
    if( hascks && info->HasChecksum() )
      std::cout << " " << std::setw( sizewidth ) << info->GetChecksum();
    std::cout << " " << info->GetModTimeAsString() << " ";
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

    std::cout << " " << info->GetModTimeAsString();

    uint64_t size = info->GetSize();
    std::string displaySize = getSizeStr(size,human,base);
    int width = displaySize.size() + 2;
    if (width < 12) {
      width = 12;
    }
    std::cout << std::setw( width ) << displaySize << " ";
  }
}

//------------------------------------------------------------------------------
// List a directory
//------------------------------------------------------------------------------
XRootDStatus DoLS( FileSystem                      *fs,
                   Env                             *env,
                   const FSExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log *log = DefaultEnv::GetLog();
  bool        stats    = false;
  bool        showUrls = false;
  bool        hascks   = false;
  bool        human    = false;
  bool        directory = false;
  uint64_t base        = 1024;
  std::string path;
  std::vector<std::string> xattrs;
  DirListFlags::Flags flags = DirListFlags::Locate | DirListFlags::Merge;

  auto applyOption = [&]( char option )
  {
    switch( option )
    {
      case 'l':
        stats = true;
        flags |= DirListFlags::Stat;
        return true;
      case 'u':
        showUrls = true;
        return true;
      case 'R':
        flags |= DirListFlags::Recursive;
        return true;
      case 'D':
        flags &= ~DirListFlags::Merge;
        return true;
      case 'Z':
        flags |= DirListFlags::Zip;
        return true;
      case 'C':
        hascks = true;
        stats = true;
        flags |= DirListFlags::Cksm;
        return true;
      case 'h':
      case 'H':
        human = true;
        return true;
      case 'd':
        directory = true;
        return true;
      case 'a':
        // xrdfs already includes entries whose names begin with a dot.
        return true;
      case '1':
        // Compatibility no-op: xrdfs already emits one entry per line without -l.
        return true;
      default:
        return false;
    }
  };

  bool parseOptions = true;
  for( uint32_t i = 1; i < args.size(); ++i )
  {
    if( parseOptions && args[i] == "--" )
    {
      parseOptions = false;
    }
    else if( parseOptions && args[i] == "--long" )
    {
      stats = true;
      flags |= DirListFlags::Stat;
    }
    else if( parseOptions && args[i] == "--human-readable" )
      human = true;
    else if( parseOptions && args[i] == "--directory" )
      directory = true;
    else if( parseOptions && args[i] == "--all" )
    {
      // Compatibility no-op: unlike gfal-ls, xrdfs does not hide dotfiles.
      continue;
    }
    else if( parseOptions && args[i] == "--color=never" )
    {
      // Compatibility no-op: xrdfs output is already uncolored.
      continue;
    }
    else if( parseOptions && args[i] == "--color" )
    {
      if( i + 1 == args.size() )
      {
        log->Error( AppMsg, "Parameter '--color' requires an argument." );
        return XRootDStatus( stError, errInvalidArgs );
      }
      if( args[i + 1] != "never" )
      {
        log->Error( AppMsg, "Unsupported --color value: %s.",
                    args[i + 1].c_str() );
        return XRootDStatus( stError, errInvalidArgs );
      }
      ++i;
    }
    else if( parseOptions && args[i] == "--xattr" )
    {
      if( i + 1 == args.size() )
      {
        log->Error( AppMsg, "Parameter '--xattr' requires an argument." );
        return XRootDStatus( stError, errInvalidArgs );
      }
      if( args[i + 1].empty() )
      {
        log->Error( AppMsg, "Parameter '--xattr' requires an argument." );
        return XRootDStatus( stError, errInvalidArgs );
      }
      xattrs.emplace_back( args[++i] );
    }
    else if( parseOptions && args[i].compare( 0, 8, "--xattr=" ) == 0 )
    {
      if( args[i].size() == 8 )
      {
        log->Error( AppMsg, "Parameter '--xattr' requires an argument." );
        return XRootDStatus( stError, errInvalidArgs );
      }
      xattrs.emplace_back( args[i].substr( 8 ) );
    }
    else if( parseOptions && args[i].size() > 2 && args[i][0] == '-' &&
             args[i][1] == '-' )
    {
      log->Error( AppMsg, "Unsupported option: %s.", args[i].c_str() );
      return XRootDStatus( stError, errInvalidArgs );
    }
    else if( parseOptions && args[i].size() > 1 && args[i][0] == '-' &&
             args[i][1] != '-' )
    {
      for( std::size_t j = 1; j < args[i].size(); ++j )
      {
        if( !applyOption( args[i][j] ) )
        {
          log->Error( AppMsg, "Invalid option: %s.", args[i].c_str() );
          return XRootDStatus( stError, errInvalidArgs );
        }
      }
    }
    else
      path = args[i];
  }

  if( showUrls )
    // we don't merge the duplicate entries
    // in case we print the full URL
    flags &= ~DirListFlags::Merge;

  auto getXAttrValues = [&]( const std::string       &xattrPath,
                             std::vector<std::string> &values )
  {
    if( !stats ) return XRootDStatus();

    values.reserve( xattrs.size() );
    for( const std::string &attribute : xattrs )
    {
      std::string value;
      XRootDStatus status = IsGFALVirtualXAttr( attribute ) ?
        GetGFALVirtualXAttr( fs, env, xattrPath, attribute, value ) :
        GetNativeXAttrValue( fs, xattrPath, attribute, value );
      if( !status.IsOK() )
      {
        log->Error( AppMsg, "Unable to get attribute %s for %s: %s",
                    attribute.c_str(), xattrPath.c_str(),
                    status.ToStr().c_str() );
        return status;
      }
      values.emplace_back( std::move( value ) );
    }
    return XRootDStatus();
  };

  std::string newPath = "/";
  if( path.empty() )
    env->GetString( "CWD", newPath );
  else
  {
    XRootDStatus pathSt = BuildPath( newPath, env, path, "Listing" );
    if( !pathSt.IsOK() )
      return pathSt;
  }

  //----------------------------------------------------------------------------
  // Stat the entry so we know if it is a file or a directory
  //----------------------------------------------------------------------------
  log->Debug( AppMsg, "Attempting to stat: %s", newPath.c_str() );

  StatInfo *info = 0;
  XRootDStatus st = fs->Stat( newPath, info );
  std::unique_ptr<StatInfo> ptr( info );
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable to stat the path: %s", st.ToStr().c_str() );
    return st;
  }

  if( directory ||
      (!info->TestFlags( StatInfo::IsDir ) &&
       !( flags & DirListFlags::Zip )) )
  {
    std::vector<std::string> values;
    st = getXAttrValues( newPath, values );
    if( !st.IsOK() ) return st;

    if( stats )
      PrintDirListStatInfo( info, false, 0, 0, 0, human, base );

    if( showUrls )
    {
      std::string url;
      fs->GetProperty( "LastURL", url );
      std::cout << url;
    }
    std::cout << newPath;
    for( const std::string &value : values )
      std::cout << '\t' << value;
    std::cout << std::endl;
    return XRootDStatus();
  }


  //----------------------------------------------------------------------------
  // Ask for the list
  //----------------------------------------------------------------------------
  log->Debug( AppMsg, "Attempting to list: %s", newPath.c_str() );

  DirectoryList *list;
  st = fs->DirList( newPath, flags, list );
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable to list the path: %s", st.ToStr().c_str() );
    return st;
  }
  std::unique_ptr<DirectoryList> listPtr( list );

  if( st.code == suPartial )
  {
    std::cerr << "[!] Some of the requests failed. The result may be ";
    std::cerr << "incomplete." << std::endl;
  }

  uint32_t ownerwidth = 0, groupwidth = 0, sizewidth = 0, ckswidth = 0;
  DirectoryList::Iterator it;
  for( it = list->Begin(); it != list->End() && stats; ++it )
  {
    StatInfo *info = (*it)->GetStatInfo();
    if( !info ) continue;

    std::string size = getSizeStr(info->GetSize(),human,base);
    uint32_t sizeWidthComp;
    if (human) {
      sizeWidthComp = size.size();
    } else {
      sizeWidthComp = nbDigits( info->GetSize());
    }

    if( ownerwidth < info->GetOwner().size() )
      ownerwidth = info->GetOwner().size();
    if( groupwidth < info->GetGroup().size() )
      groupwidth = info->GetGroup().size();
    if( sizewidth < sizeWidthComp  )
      sizewidth = sizeWidthComp;
    if( ckswidth < info->GetChecksum().size() )
      ckswidth = info->GetChecksum().size();
  }

  //----------------------------------------------------------------------------
  // Print the results
  //----------------------------------------------------------------------------
  for( it = list->Begin(); it != list->End(); ++it )
  {
    StatInfo *entryInfo = stats ? (*it)->GetStatInfo() : 0;
    if( stats && !xattrs.empty() && !entryInfo )
    {
      // gfal-ls omits dangling entries before requesting their attributes.
      continue;
    }

    const std::string entryPath =
      list->GetParentName() + (*it)->GetName();
    std::vector<std::string> values;
    st = getXAttrValues( entryPath, values );
    if( !st.IsOK() ) return st;

    if( stats )
    {
      if( !entryInfo )
        std::cout << "---- 0000-00-00 00:00:00            ? ";
      else
        PrintDirListStatInfo( entryInfo, hascks, ownerwidth, groupwidth,
                              sizewidth, human, base );
    }
    if( showUrls )
      std::cout << "root://" << (*it)->GetHostAddress() << "/";
    std::cout << entryPath;
    for( const std::string &value : values )
      std::cout << '\t' << value;
    std::cout << std::endl;
  }
  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Create a directory
//------------------------------------------------------------------------------
XRootDStatus DoMkDir( FileSystem                      *fs,
                      Env                             *env,
                      const FSExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log *log = DefaultEnv::GetLog();
  MkDirFlags::Flags flags = MkDirFlags::None;
  std::vector<std::string> modeStrings( 1, "rwxr-x---" );
  std::vector<std::string> paths;
  bool parseOptions = true;

  for( std::size_t i = 1; i < args.size(); ++i )
  {
    if( parseOptions && args[i] == "--" )
    {
      parseOptions = false;
    }
    else if( parseOptions && (args[i] == "-p" ||
                              args[i] == "--parents") )
    {
      flags |= MkDirFlags::MakePath;
    }
    else if( parseOptions && (args[i] == "-m" ||
                              args[i] == "--mode") )
    {
      if( i + 1 == args.size() )
      {
        log->Error( AppMsg, "Parameter '%s' requires an argument.",
                    args[i].c_str() );
        return XRootDStatus( stError, errInvalidArgs );
      }
      modeStrings.emplace_back( args[++i] );
    }
    else if( parseOptions && args[i].compare( 0, 7, "--mode=" ) == 0 )
    {
      modeStrings.emplace_back( args[i].substr( 7 ) );
    }
    else if( parseOptions && args[i].compare( 0, 2, "-m" ) == 0 )
    {
      modeStrings.emplace_back( args[i].substr( 2 ) );
    }
    else if( parseOptions && args[i].size() > 1 && args[i][0] == '-' )
    {
      log->Error( AppMsg, "Unsupported option: %s.", args[i].c_str() );
      return XRootDStatus( stError, errInvalidArgs );
    }
    else
    {
      paths.emplace_back( args[i] );
    }
  }

  if( paths.empty() )
  {
    log->Error( AppMsg, "No directory path specified." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  Access::Mode mode = Access::None;
  for( const std::string &modeString : modeStrings )
  {
    if( ParseAccessMode( mode, modeString ) == AccessModeFormat::Invalid )
    {
      log->Error( AppMsg, "Invalid mode string: %s.", modeString.c_str() );
      return XRootDStatus( stError, errInvalidArgs );
    }
  }

  std::vector<std::string> newPaths;
  newPaths.reserve( paths.size() );
  for( const std::string &path : paths )
  {
    std::string newPath;
    if( !BuildPath( newPath, env, path ).IsOK() )
    {
      log->Error( AppMsg, "Invalid path: %s.", path.c_str() );
      return XRootDStatus( stError, errInvalidArgs );
    }
    newPaths.emplace_back( std::move( newPath ) );
  }

  //----------------------------------------------------------------------------
  // Run the queries sequentially, matching gfal-mkdir's ordering.
  //----------------------------------------------------------------------------
  for( const std::string &newPath : newPaths )
  {
    XRootDStatus st = fs->MkDir( newPath, flags, mode );
    if( !st.IsOK() )
    {
      log->Error( AppMsg, "Unable create directory %s: %s",
                          newPath.c_str(),
                          st.ToStr().c_str() );
      return st;
    }
  }

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Remove a directory
//------------------------------------------------------------------------------
XRootDStatus DoRmDir( FileSystem                      *query,
                      Env                             *env,
                      const FSExecutor::CommandParams &args )
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
  XRootDStatus pathSt = BuildPath( fullPath, env, args[1], "Removing" );
  if( !pathSt.IsOK() )
    return pathSt;

  if( NeedsWebDAVRemovalGuard( env ) )
  {
    XRootDStatus st = GuardWebDAVRemoval(
      query, fullPath, NonRecursiveRemoval::Directory );
    if( !st.IsOK() )
    {
      log->Error( AppMsg, "Unable to safely remove directory %s: %s",
                          fullPath.c_str(), st.ToStr().c_str() );
      return st;
    }
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
XRootDStatus DoMv( FileSystem                      *fs,
                   Env                             *env,
                   const FSExecutor::CommandParams &args )
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
  XRootDStatus pathSt = BuildPath( fullPath1, env, args[1], "Renaming" );
  if( !pathSt.IsOK() )
    return pathSt;

  std::string fullPath2;
  pathSt = BuildPath( fullPath2, env, args[2], "Renaming to" );
  if( !pathSt.IsOK() )
    return pathSt;

  if( is_subdirectory(fullPath1, fullPath2) )
    return XRootDStatus( stError, errInvalidArgs, 0,
      "cannot move directory to a subdirectory of itself." );

  //----------------------------------------------------------------------------
  // Run the query
  //----------------------------------------------------------------------------
  XRootDStatus st = fs->Mv( fullPath1, fullPath2 );
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
// Remove a file
//------------------------------------------------------------------------------
XRootDStatus DoRm( FileSystem                      *fs,
                   Env                             *env,
                   const FSExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log         *log     = DefaultEnv::GetLog();
  RemoveCommand command;
  std::string parseError;
  if( !ParseRemoveCommand( args, command, parseError ) )
  {
    log->Error( AppMsg, "%s", parseError.c_str() );
    return XRootDStatus( stError, errInvalidArgs );
  }

  std::vector<std::string> fullPaths;
  fullPaths.reserve( command.paths.size() );
  for( const std::string &path : command.paths )
  {
    if( command.recursive &&
        !ValidateRecursiveRemovePath( path, parseError ) )
    {
      log->Error( AppMsg, "Invalid recursive removal path %s: %s",
                          RemovalDisplayPath( path ).c_str(),
                          parseError.c_str() );
      return XRootDStatus( stError, errInvalidArgs, 0, parseError );
    }

    std::string fullPath;
    if( !BuildPath( fullPath, env, path ).IsOK() )
    {
      log->Error( AppMsg, "Invalid path: %s",
                          RemovalDisplayPath( path ).c_str() );
      return XRootDStatus( stError, errInvalidArgs );
    }
    fullPaths.emplace_back( std::move( fullPath ) );
  }

  if( command.recursive )
  {
    // Validate every resolved operand before the first mutation. This also
    // catches relative paths which resolve to the namespace root.
    for( const std::string &fullPath : fullPaths )
    {
      if( !ValidateRecursiveRemovePath( fullPath, parseError ) )
      {
        log->Error( AppMsg, "Invalid recursive removal path %s: %s",
                            RemovalDisplayPath( fullPath ).c_str(),
                            parseError.c_str() );
        return XRootDStatus( stError, errInvalidArgs, 0, parseError );
      }
    }
  }

  if( command.dryRun )
  {
    DryRunRemoveOperations operations;
    operations.force = command.force;
    operations.stat = [fs]( const std::string &path, bool &isDirectory )
    {
      StatInfo *rawStat = nullptr;
      XRootDStatus status = fs->Stat( path, rawStat );
      std::unique_ptr<StatInfo> stat( rawStat );
      if( !status.IsOK() || status.code != suDone ) return status;
      if( !stat )
        return XRootDStatus( stError, errInvalidResponse, 0,
                             "Stat succeeded without target metadata." );
      isDirectory = stat->TestFlags( StatInfo::IsDir );
      return status;
    };
    operations.list = [fs]( const std::string &path,
                            std::vector<std::string> &children )
    {
      DirectoryList *rawList = nullptr;
      XRootDStatus status = fs->DirList( path, DirListFlags::None, rawList );
      std::unique_ptr<DirectoryList> list( rawList );
      if( !status.IsOK() || status.code != suDone ) return status;
      if( !list )
        return XRootDStatus( stError, errInvalidResponse, 0,
                             "Directory listing succeeded without a response." );

      children.reserve( list->GetSize() );
      for( DirectoryList::ConstIterator entry = list->Begin();
           entry != list->End(); ++entry )
      {
        if( !*entry )
          return XRootDStatus( stError, errInvalidResponse, 0,
                               "Directory listing contained a null entry." );
        children.push_back( (*entry)->GetName() );
      }
      return status;
    };
    operations.report = []( const std::string &path, bool isDirectory )
    {
      std::cout << RemovalDisplayPath( path ) << '\t'
                << (isDirectory ? "SKIP DIR" : "SKIP") << '\n';
    };
    operations.reportFailure = []( const std::string &path,
                                   const XRootDStatus &status )
    {
      if( status.errNo == kXR_NotFound )
        std::cout << RemovalDisplayPath( path ) << "\tMISSING\n";
      else if( status.errNo != kXR_isDirectory )
        std::cout << RemovalDisplayPath( path ) << "\tFAILED\n";
    };

    std::string failedPath;
    XRootDStatus status = PlanRemoval(
      fullPaths, command.recursive, operations, failedPath );
    if( !status.IsOK() )
    {
      // ExecuteCommand prints a returned failure again, so sanitize the
      // status itself rather than only the diagnostic emitted here.
      const XRootDStatus sanitized = SanitizeRemovalStatus( status );
      log->Error( AppMsg, "Unable to plan removal of %s: %s",
                          RemovalDisplayPath( failedPath ).c_str(),
                          sanitized.ToStr().c_str() );
      return sanitized;
    }
    return XRootDStatus();
  }

  if( command.recursive )
  {
    RecursiveRemoveOperations operations;
    operations.nativeXRootD = IsXRootDProtocol( env );
    operations.force = command.force;
    operations.remove = [fs]( const std::string &path )
    {
      // Trying Rm first is important: a native directory symlink is unlinked
      // here instead of being followed by a metadata operation.
      return fs->Rm( path );
    };
    operations.list = [fs]( const std::string &path,
                            std::vector<std::string> &children )
    {
      DirectoryList *rawList = nullptr;
      XRootDStatus status = fs->DirList( path, DirListFlags::None, rawList );
      std::unique_ptr<DirectoryList> list( rawList );
      if( !status.IsOK() || status.code != suDone ) return status;
      if( !list )
        return XRootDStatus( stError, errInvalidResponse, 0,
                             "Directory listing succeeded without a response." );

      children.reserve( list->GetSize() );
      for( DirectoryList::ConstIterator entry = list->Begin();
           entry != list->End(); ++entry )
      {
        if( !*entry )
          return XRootDStatus( stError, errInvalidResponse, 0,
                               "Directory listing contained a null entry." );
        children.push_back( (*entry)->GetName() );
      }
      return status;
    };
    operations.removeDirectory = [fs]( const std::string &path )
    {
      return fs->RmDir( path );
    };

    bool haveFailure = false;
    XRootDStatus firstFailure;
    // Process operands in command-line order. A failed tree is abandoned, but
    // later top-level operands are still attempted, matching multi-file rm.
    for( const std::string &fullPath : fullPaths )
    {
      std::string failedPath;
      XRootDStatus status = RemoveRecursively(
        {fullPath}, operations, failedPath );
      if( !status.IsOK() )
      {
        if( command.force && status.errNo == kXR_NotFound )
          continue;
        log->Error( AppMsg, "Unable to recursively remove %s: %s",
                            failedPath.c_str(), status.ToStr().c_str() );
        if( !haveFailure )
        {
          firstFailure = status;
          haveFailure = true;
        }
        continue;
      }
      std::cout << "rm " << fullPath << " : " << status.ToString() << '\n';
    }
    return haveFailure ? firstFailure : XRootDStatus();
  }

  if( NeedsWebDAVRemovalGuard( env ) )
  {
    for( const std::string &fullPath : fullPaths )
    {
      XRootDStatus st = GuardWebDAVRemoval(
        fs, fullPath, NonRecursiveRemoval::File );
      if( !st.IsOK() )
      {
        if( command.force && st.errNo == kXR_NotFound )
          continue;
        log->Error( AppMsg, "Unable to safely remove %s: %s",
                            fullPath.c_str(), st.ToStr().c_str() );
        return st;
      }
    }
  }

  struct print_t
  {
    void print( const std::string &msg )
    {
      std::unique_lock<std::mutex> lck( mtx );
      std::cout << msg << '\n';
    }
    std::mutex mtx;
  };
  std::shared_ptr<print_t> print;
  if( !fullPaths.empty() )
    print = std::make_shared<print_t>();

  std::vector<Pipeline> rms;
  rms.reserve( fullPaths.size() );
  std::mutex failureMtx;
  bool haveNonForceFailure = false;
  XRootDStatus nonForceFailure;

  for( const std::string &fullPath : fullPaths )
  {
    const bool force = command.force;
    rms.emplace_back( Rm( fs, fullPath ) >>
                      [log, fullPath, print, force, &failureMtx,
                       &haveNonForceFailure, &nonForceFailure]( XRootDStatus &st )
                      {
                        if( !st.IsOK() )
                        {
                          if( force && st.errNo == kXR_NotFound )
                            return;

                          {
                            std::unique_lock<std::mutex> lck( failureMtx );
                            if( !haveNonForceFailure )
                            {
                              haveNonForceFailure = true;
                              nonForceFailure = st;
                            }
                          }
                          log->Error( AppMsg, "Unable remove %s: %s",
                                              fullPath.c_str(),
                                              st.ToStr().c_str() );
                        }
                        else if( print )
                        {
                          print->print( "rm " + fullPath + " : " + st.ToString() );
                        }
                      } );
  }

  //----------------------------------------------------------------------------
  // Run the query:
  // Parallel() will take the vector of Pipeline by reference and empty the
  // vector, so rms.size() will change after the call.
  //----------------------------------------------------------------------------
  const size_t rs = rms.size();
  XRootDStatus st = WaitFor( Parallel( rms ).AtLeast( rs ) );
  if( command.force )
  {
    std::unique_lock<std::mutex> lck( failureMtx );
    if( haveNonForceFailure )
      return nonForceFailure;
    return XRootDStatus();
  }
  if( !st.IsOK() )
    return st;

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Truncate a file
//------------------------------------------------------------------------------
XRootDStatus DoTruncate( FileSystem                      *fs,
                         Env                             *env,
                         const FSExecutor::CommandParams &args )
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

  std::string fullPath;
  XRootDStatus pathSt = BuildPath( fullPath, env, args[1], "Truncating" );
  if( !pathSt.IsOK() )
    return pathSt;

  char *result;
  uint64_t size = ::strtoll( args[2].c_str(), &result, 0 );
  if( *result != 0 )
  {
    log->Error( AppMsg, "Size parameter needs to be an integer" );
    return XRootDStatus( stError, errInvalidArgs );
  }

  //----------------------------------------------------------------------------
  // Run the query
  //----------------------------------------------------------------------------
  XRootDStatus st = fs->Truncate( fullPath, size );
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable truncate %s: %s",
                        fullPath.c_str(),
                        st.ToStr().c_str() );
    return st;
  }

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Change the access rights to a file
//------------------------------------------------------------------------------
XRootDStatus DoChMod( FileSystem                      *fs,
                      Env                             *env,
                      const FSExecutor::CommandParams &args )
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

  Access::Mode mode = Access::None;
  std::string path;
  const AccessModeFormat secondFormat = ParseAccessMode( mode, args[2] );
  if( secondFormat != AccessModeFormat::Invalid )
  {
    // Preserve the historical path-first form, including a path whose name
    // itself looks like an octal mode.
    path = args[1];
  }
  else
  {
    const AccessModeFormat firstFormat = ParseAccessMode( mode, args[1] );
    if( firstFormat != AccessModeFormat::Octal )
    {
      log->Error( AppMsg, "Invalid mode string." );
      return XRootDStatus( stError, errInvalidArgs );
    }
    path = args[2];
  }

  std::string fullPath;
  XRootDStatus pathSt = BuildPath( fullPath, env, path, "Modifying" );
  if( !pathSt.IsOK() )
    return pathSt;

  //----------------------------------------------------------------------------
  // Run the query
  //----------------------------------------------------------------------------
  XRootDStatus st = fs->ChMod( fullPath, mode );
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable change mode of %s: %s",
                        fullPath.c_str(),
                        st.ToStr().c_str() );
    return st;
  }

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Locate a path
//------------------------------------------------------------------------------
XRootDStatus DoLocate( FileSystem                      *fs,
                       Env                             *env,
                       const FSExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log         *log     = DefaultEnv::GetLog();
  uint32_t     argc    = args.size();

  if( argc > 4 )
  {
    log->Error( AppMsg, "Wrong number of arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  OpenFlags::Flags flags = OpenFlags::None;
  std::string path;
  bool        hasPath      = false;
  bool        doDeepLocate = false;
  for( uint32_t i = 1; i < argc; ++i )
  {
    if( args[i] == "-n" )
      flags |= OpenFlags::NoWait;
    else if( args[i] == "-r" )
      flags |= OpenFlags::Refresh;
    else if( args[i] == "-m" || args[i] == "-h" )
      flags |= OpenFlags::PrefName;
    else if( args[i] == "-i" )
      flags |= OpenFlags::Force;
    else if( args[i] == "-d" )
      doDeepLocate = true;
    else if( args[i] == "-p" )
    {
      Env *env = DefaultEnv::GetEnv();
      env->PutInt( "PreserveLocateTried", 0 );
    }
    else if( !hasPath )
    {
      path = args[i];
      hasPath = true;
    }
    else
    {
      log->Error( AppMsg, "Invalid argument: %s.", args[i].c_str() );
      return XRootDStatus( stError, errInvalidArgs );
    }
  }

  std::string fullPath;
  if( path[0] == '*' )
    fullPath = path;
  else
  {
    XRootDStatus pathSt = BuildPath( fullPath, env, path, "Locating" );
    if( !pathSt.IsOK() )
      return pathSt;
  }

  //----------------------------------------------------------------------------
  // Run the query
  //----------------------------------------------------------------------------
  LocationInfo *info = 0;
  XRootDStatus  st;
  if( doDeepLocate )
    st = fs->DeepLocate( fullPath, flags, info );
  else
    st = fs->Locate( fullPath, flags, info );

  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable locate %s: %s",
                        fullPath.c_str(),
                        st.ToStr().c_str() );
    return st;
  }

  //----------------------------------------------------------------------------
  // Print the result
  //----------------------------------------------------------------------------
  if( st.code == suPartial )
  {
    std::cerr << "[!] Some of the requests failed. The result may be ";
    std::cerr << "incomplete." << std::endl;
  }

  LocationInfo::Iterator it;
  for( it = info->Begin(); it != info->End(); ++it )
  {
    std::cout << it->GetAddress() << " ";
    switch( it->GetType() )
    {
      case LocationInfo::ManagerOnline:
        std::cout << "Manager ";
        break;
      case LocationInfo::ManagerPending:
        std::cout << "ManagerPending ";
        break;
      case LocationInfo::ServerOnline:
        std::cout << "Server ";
        break;
      case LocationInfo::ServerPending:
        std::cout << "ServerPending ";
        break;
      default:
        std::cout << "Unknown ";
    };

    switch( it->GetAccessType() )
    {
      case LocationInfo::Read:
        std::cout << "Read";
        break;
      case LocationInfo::ReadWrite:
        std::cout << "ReadWrite ";
        break;
      default:
        std::cout << "Unknown ";
    };
    std::cout << std::endl;
  }

  delete info;
  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Process stat query
//------------------------------------------------------------------------------
XRootDStatus ProcessStatQuery( StatInfo &info, const std::string &query )
{
  Log *log = DefaultEnv::GetLog();

  //----------------------------------------------------------------------------
  // Process the query
  //----------------------------------------------------------------------------
  bool isOrQuery = false;
  bool status    = true;
  if( query.find( '|' ) != std::string::npos )
  {
    isOrQuery = true;
    status    = false;
  }
  std::vector<std::string> queryFlags;
  if( isOrQuery )
    Utils::splitString( queryFlags, query, "|" );
  else
    Utils::splitString( queryFlags, query, "&" );

  //----------------------------------------------------------------------------
  // Initialize flag translation map and check the input flags
  //----------------------------------------------------------------------------
  std::map<std::string, StatInfo::Flags> flagMap;
  flagMap["XBitSet"]      = StatInfo::XBitSet;
  flagMap["IsDir"]        = StatInfo::IsDir;
  flagMap["Other"]        = StatInfo::Other;
  flagMap["Offline"]      = StatInfo::Offline;
  flagMap["POSCPending"]  = StatInfo::POSCPending;
  flagMap["IsReadable"]   = StatInfo::IsReadable;
  flagMap["IsWritable"]   = StatInfo::IsWritable;
  flagMap["BackUpExists"] = StatInfo::BackUpExists;

  std::vector<std::string>::iterator it;
  for( it = queryFlags.begin(); it != queryFlags.end(); ++it )
    if( flagMap.find( *it ) == flagMap.end() )
    {
      log->Error( AppMsg, "Flag '%s' is not recognized.", it->c_str() );
      return XRootDStatus( stError, errInvalidArgs );
    }

  //----------------------------------------------------------------------------
  // Process the query
  //----------------------------------------------------------------------------
  if( isOrQuery )
  {
    for( it = queryFlags.begin(); it != queryFlags.end(); ++it )
      if( info.TestFlags( flagMap[*it] ) )
        return XRootDStatus();
  }
  else
  {
    for( it = queryFlags.begin(); it != queryFlags.end(); ++it )
      if( !info.TestFlags( flagMap[*it] ) )
        return XRootDStatus( stError, errResponseNegative );
  }

  if( status )
    return XRootDStatus();
  return XRootDStatus( stError, errResponseNegative );
}

//------------------------------------------------------------------------------
// Stat a path
//------------------------------------------------------------------------------
XRootDStatus DoStat( FileSystem                      *fs,
                     Env                             *env,
                     const FSExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log         *log     = DefaultEnv::GetLog();
  uint32_t     argc    = args.size();

  if( argc < 2 )
  {
    log->Error( AppMsg, "Wrong number of arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  std::vector<std::string> paths;
  std::string query;

  for( uint32_t i = 1; i < args.size(); ++i )
  {
    if( args[i] == "-q" )
    {
      if( i < args.size()-1 )
      {
        query = args[i+1];
        ++i;
      }
      else
      {
        log->Error( AppMsg, "Parameter '-q' requires an argument." );
        return XRootDStatus( stError, errInvalidArgs );
      }
    }
    else
      paths.emplace_back( args[i] );
  }

  std::vector<XrdCl::Pipeline> stats;
  std::vector<std::tuple<std::future<StatInfo>, std::string>> results;
  for( auto &path : paths )
  {
    std::string fullPath;
    XRootDStatus pathSt = BuildPath( fullPath, env, path, "Stating" );
    if( !pathSt.IsOK() )
      return pathSt;
    std::future<XrdCl::StatInfo> ftr;
    stats.emplace_back( XrdCl::Stat( fs, fullPath ) >> ftr );
    results.emplace_back( std::move( ftr ), std::move( fullPath ) );
  }

  //----------------------------------------------------------------------------
  // Run the query
  //----------------------------------------------------------------------------
  XrdCl::Async( XrdCl::Parallel( stats ) );

  //----------------------------------------------------------------------------
  // Print the result
  //----------------------------------------------------------------------------
  XrdCl::XRootDStatus st;
  for( auto &tpl : results )
  {
    auto &ftr      = std::get<0>( tpl );
    auto &fullPath = std::get<1>( tpl );
    std::cout << std::endl;
    try
    {
      XrdCl::StatInfo info( ftr.get() );
      std::string flags;

      if( info.TestFlags( StatInfo::XBitSet ) )
        flags += "XBitSet|";
      if( info.TestFlags( StatInfo::IsDir ) )
        flags += "IsDir|";
      if( info.TestFlags( StatInfo::Other ) )
        flags += "Other|";
      if( info.TestFlags( StatInfo::Offline ) )
        flags += "Offline|";
      if( info.TestFlags( StatInfo::POSCPending ) )
        flags += "POSCPending|";
      if( info.TestFlags( StatInfo::IsReadable ) )
        flags += "IsReadable|";
      if( info.TestFlags( StatInfo::IsWritable ) )
        flags += "IsWritable|";
      if( info.TestFlags( StatInfo::BackUpExists ) )
        flags += "BackUpExists|";

      if( !flags.empty() )
        flags.erase( flags.length()-1, 1 );

      std::cout <<   "Path:   " << fullPath << std::endl;
      std::cout <<   "Id:     " << info.GetId() << std::endl;
      std::cout <<   "Size:   " << info.GetSize() << std::endl;
      std::cout <<   "MTime:  " << info.GetModTimeAsString() << std::endl;
      // if extended stat information is available we can print also
      // change time and access time
      if( info.ExtendedFormat() )
      {
        std::cout << "CTime:  " << info.GetChangeTimeAsString() << std::endl;
        std::cout << "ATime:  " << info.GetAccessTimeAsString() << std::endl;
      }
      std::cout << "Flags:  " << info.GetFlags() << " (" << flags << ")";

      // check if extended stat information is available
      if( info.ExtendedFormat() )
      {
        std::cout << "\nMode:   " << info.GetModeAsString() << std::endl;
        std::cout << "Owner:  " << info.GetOwner() << std::endl;
        std::cout << "Group:  " << info.GetGroup();
      }

      std::cout << std::endl;

      if( query.length() != 0 )
      {
        XRootDStatus s = ProcessStatQuery( info, query );
        if( !s.IsOK() )
          st = s;
        std::cout << "Query:  " << query << " " << std::endl;
      }
    }
    catch( XrdCl::PipelineException &ex )
    {
      st = ex.GetError();
      log->Error( AppMsg, "Unable stat %s: %s", fullPath.c_str(), st.ToStr().c_str() );
    }
  }

  return st;
}

//------------------------------------------------------------------------------
// Stat a VFS
//------------------------------------------------------------------------------
XRootDStatus DoStatVFS( FileSystem                      *fs,
                        Env                            *env,
                        const FSExecutor::CommandParams &args )
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
  XRootDStatus pathSt = BuildPath( fullPath, env, args[1], "Stating" );
  if( !pathSt.IsOK() )
    return pathSt;

  //----------------------------------------------------------------------------
  // Run the query
  //----------------------------------------------------------------------------
  StatInfoVFS *info = 0;
  XRootDStatus st = fs->StatVFS( fullPath, info );

  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable stat VFS at %s: %s",
                        fullPath.c_str(),
                        st.ToStr().c_str() );
    return st;
  }

  //----------------------------------------------------------------------------
  // Print the result
  //----------------------------------------------------------------------------
  std::cout << "Path:                               ";
  std::cout << fullPath << std::endl;
  std::cout << "Nodes with RW space:                ";
  std::cout << info->GetNodesRW() << std::endl;
  std::cout << "Size of largest RW space (MB):      ";
  std::cout << info->GetFreeRW() << std::endl;
  std::cout << "Utilization of RW space (%):        ";
  std::cout << (uint16_t)info->GetUtilizationRW() << std::endl;
  std::cout << "Nodes with staging space:           ";
  std::cout << info->GetNodesStaging() << std::endl;
  std::cout << "Size of largest staging space (MB): ";
  std::cout << info->GetFreeStaging() << std::endl;
  std::cout << "Utilization of staging space (%):   ";
  std::cout << (uint16_t)info->GetUtilizationStaging() << std::endl;

  delete info;
  return XRootDStatus();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Query the server
//------------------------------------------------------------------------------
XRootDStatus DoQuery( FileSystem                      *fs,
                      Env                             *env,
                      const FSExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log         *log     = DefaultEnv::GetLog();
  uint32_t     argc    = args.size();

  if( !( argc >= 2 ) )
  {
    log->Error( AppMsg, "Wrong number of arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  if( args[1] == "tape" || args[1] == "archiveinfo" )
  {
    bool jsonOutput = false;
    std::string tapeCmd = (args[1] == "archiveinfo") ? "archiveinfo" : "discover";
    size_t startIdx = 2;
    if( args[1] == "tape" && args.size() > 2 )
    {
      if( args[2] == "--json" )
      {
        jsonOutput = true;
        if( args.size() > 3 ) tapeCmd = args[3];
        startIdx = 4;
      }
      else
      {
        tapeCmd = args[2];
        startIdx = 3;
      }
    }

    std::string strArg;
    if( tapeCmd == "discover" || tapeCmd == "discovery" )
    {
      strArg = "tape.discover";
    }
    else if( tapeCmd == "archiveinfo" || tapeCmd == "archivepoll" )
    {
      strArg = "tape.archiveinfo";
      for( size_t i = startIdx; i < args.size(); ++i )
      {
        if( args[i] == "--json" ) { jsonOutput = true; continue; }
        std::string path;
        if( !BuildPath( path, env, args[i] ).IsOK() )
        {
          log->Error( AppMsg, "Invalid path: %s", args[i].c_str() );
          return XRootDStatus( stError, errInvalidArgs );
        }
        strArg += '\n' + path;
      }
    }
    else if( tapeCmd == "delete" || tapeCmd == "stage_delete" )
    {
      if( startIdx >= args.size() )
      {
        log->Error( AppMsg, "Missing request ID for tape delete." );
        return XRootDStatus( stError, errInvalidArgs );
      }
      strArg = "tape.stage_delete\n" + args[startIdx];
    }
    else
    {
      log->Error( AppMsg, "Unknown tape query command: %s", tapeCmd.c_str() );
      return XRootDStatus( stError, errInvalidArgs );
    }

    Buffer arg( strArg.size() );
    arg.FromString( strArg );
    Buffer *response = nullptr;
    XRootDStatus st = fs->Query( QueryCode::Opaque, arg, response );
    std::unique_ptr<Buffer> respPtr( response );
    if( !st.IsOK() )
    {
      log->Error( AppMsg, "Tape query failed: %s", st.ToStr().c_str() );
      return st;
    }
    if( !response ) return XRootDStatus();

    std::string respStr = response->ToString();
    while( !respStr.empty() && (respStr.back() == '\0' || respStr.back() == '\n' || respStr.back() == '\r') )
      respStr.pop_back();

    if( jsonOutput )
    {
      std::cout << respStr << '\n';
      return XRootDStatus();
    }

    if( tapeCmd == "discover" || tapeCmd == "discovery" )
    {
      auto json = nlohmann::json::parse( respStr, nullptr, false );
      if( json.is_object() )
      {
        std::cout << "Tape REST Discovery:\n";
        if( json.contains( "sitename" ) && !json["sitename"].is_null() )
          std::cout << "  Site Name:     " << json["sitename"].get<std::string>() << '\n';
        if( json.contains( "version" ) && !json["version"].is_null() )
          std::cout << "  API Version:   " << json["version"].get<std::string>() << '\n';
        if( json.contains( "uri" ) && !json["uri"].is_null() )
          std::cout << "  Endpoint URI:  " << json["uri"].get<std::string>() << '\n';
        return XRootDStatus();
      }
    }
    else if( tapeCmd == "archiveinfo" || tapeCmd == "archivepoll" )
    {
      auto json = nlohmann::json::parse( respStr, nullptr, false );
      if( json.is_array() )
      {
        for( const auto &entry : json )
        {
          if( entry.is_object() )
          {
            std::string p = entry.value( "path", entry.value( "url", "" ) );
            std::string loc = entry.value( "locality", "UNKNOWN" );
            std::cout << p << '\t' << loc;
            if( entry.contains( "error" ) && !entry["error"].is_null() )
              std::cout << "\tERROR: " << entry["error"].dump();
            std::cout << '\n';
          }
        }
        return XRootDStatus();
      }
    }

    std::cout << respStr << '\n';
    return XRootDStatus();
  }

  if( !( argc >= 3 ) )
  {
    log->Error( AppMsg, "Wrong number of arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  if( args[1] == "prepare" && args.size() > 2 && (args[2] == "-d" || args[2] == "--delete") )
  {
    if( args.size() < 4 )
    {
      log->Error( AppMsg, "Missing request ID for stage delete." );
      return XRootDStatus( stError, errInvalidArgs );
    }
    std::string strArg = "tape.stage_delete\n" + args[3];
    Buffer arg( strArg.size() );
    arg.FromString( strArg );
    Buffer *response = nullptr;
    XRootDStatus st = fs->Query( QueryCode::Opaque, arg, response );
    delete response;
    if( !st.IsOK() )
    {
      log->Error( AppMsg, "Stage delete failed: %s", st.ToStr().c_str() );
    }
    return st;
  }

  QueryCode::Code qCode;
  if( args[1] == "config" )
    qCode = QueryCode::Config;
  else if( args[1] == "checksumcancel" )
    qCode = QueryCode::ChecksumCancel;
  else if( args[1] == "checksum" )
    qCode = QueryCode::Checksum;
  else if( args[1] == "opaque" )
    qCode = QueryCode::Opaque;
  else if( args[1] == "opaquefile" )
    qCode = QueryCode::OpaqueFile;
  else if( args[1] == "prepare" )
    qCode = QueryCode::Prepare;
  else if( args[1] == "space" )
    qCode = QueryCode::Space;
  else if( args[1] == "stats" )
    qCode = QueryCode::Stats;
  else if( args[1] == "xattr" )
    qCode = QueryCode::XAttr;
  else
  {
    log->Error( AppMsg, "Invalid query code." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  if( !( qCode & QueryCode::Prepare ) && argc != 3  )
  {
    log->Error( AppMsg, "Wrong number of arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  std::string strArg = args[2];
  if( qCode & QueryCode::Prepare )
  {
    // strArg is supposed to contain already the request ID

    for( size_t i = 3; i < args.size(); ++i )
    {
      std::string path = args[i];
      XRootDStatus pathSt = BuildPath( path, env, path, "Preparing" );
      if( !pathSt.IsOK() )
        return pathSt;
      // we use new line character as delimiter
      strArg += '\n';
      strArg += path;
    }
  }
  else
  {
    std::string strArg = args[2];
    if( qCode == QueryCode::ChecksumCancel ||
        qCode == QueryCode::Checksum       ||
        qCode == QueryCode::XAttr )
    {
      XRootDStatus pathSt = BuildPath( strArg, env, args[2], "Querying" );
      if( !pathSt.IsOK() )
        return pathSt;
    }
  }

  //----------------------------------------------------------------------------
  // Run the query
  //----------------------------------------------------------------------------
  Buffer arg( strArg.size() );
  arg.FromString( strArg );
  Buffer *response = 0;
  XRootDStatus st = fs->Query( qCode, arg, response );

  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable run query %s: %s",
                        args[1].c_str(),
                        st.ToStr().c_str() );
    return st;
  }

  //----------------------------------------------------------------------------
  // Print the result
  //----------------------------------------------------------------------------
  std::cout << response->ToString() << std::endl;
  delete response;
  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Normalize a checksum algorithm before using it in a query parameter
//------------------------------------------------------------------------------
XRootDStatus NormalizeChecksumType( const std::string &input,
                                    std::string       &normalized )
{
  if( input.empty() )
    return XRootDStatus( stError, errInvalidArgs, 0,
                         "Checksum type cannot be empty." );

  normalized = input;
  for( char &character : normalized )
  {
    const unsigned char value = static_cast<unsigned char>( character );
    if( std::isalnum( value ) == 0 && character != '-' && character != '_' )
      return XRootDStatus( stError, errInvalidArgs, 0,
                           "Invalid checksum type: " + input );
    character = static_cast<char>( std::tolower( value ) );
  }
  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Query a checksum through the existing XrdCl filesystem operation
//------------------------------------------------------------------------------
XRootDStatus QueryChecksum( FileSystem        *fs,
                            const std::string &path,
                            const std::string &requested,
                            std::string       &algorithm,
                            std::string       &digest )
{
  std::string queryPath = path;
  std::string normalized;
  if( !requested.empty() )
  {
    XRootDStatus status = NormalizeChecksumType( requested, normalized );
    if( !status.IsOK() ) return status;

    queryPath += queryPath.find( '?' ) == std::string::npos ? '?' : '&';
    queryPath += "cks.type=";
    queryPath += normalized;
  }

  Buffer request( queryPath.size() );
  request.FromString( queryPath );
  Buffer *rawResponse = 0;
  XRootDStatus status = fs->Query(
    QueryCode::Checksum, request, rawResponse );
  std::unique_ptr<Buffer> response( rawResponse );
  if( !status.IsOK() ) return status;
  if( !response )
    return XRootDStatus( stError, errInvalidResponse, 0,
                         "Checksum query returned no response." );

  std::vector<std::string> fields;
  Utils::splitString( fields, response->ToString(), " " );
  if( fields.size() != 2 )
    return XRootDStatus( stError, errInvalidResponse, 0,
                         "Invalid checksum response: " + response->ToString() );

  std::transform( fields[0].begin(), fields[0].end(), fields[0].begin(),
                  []( unsigned char character )
  {
    return static_cast<char>( std::tolower( character ) );
  } );
  if( !normalized.empty() && fields[0] != normalized )
  {
    return XRootDStatus(
      stError, errCheckSumError, 0,
      "Checksum response used " + fields[0] + " instead of " + normalized );
  }

  algorithm = std::move( fields[0] );
  digest = std::move( fields[1] );
  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Query a file checksum using gfal-sum's positional layout
//------------------------------------------------------------------------------
XRootDStatus DoSum( FileSystem                      *fs,
                    Env                             *env,
                    const FSExecutor::CommandParams &args )
{
  Log *log = DefaultEnv::GetLog();
  if( args.size() != 3 )
  {
    log->Error( AppMsg, "Wrong number of arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  std::string path;
  if( !BuildPath( path, env, args[1] ).IsOK() )
  {
    log->Error( AppMsg, "Invalid path." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  std::string algorithm;
  std::string digest;
  XRootDStatus status = QueryChecksum(
    fs, path, args[2], algorithm, digest );
  if( !status.IsOK() )
  {
    log->Error( AppMsg, "Unable to query %s checksum: %s", args[2].c_str(),
                status.ToStr().c_str() );
    return status;
  }

  std::cout << algorithm << " " << digest << std::endl;
  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Request a storage-issued token
//------------------------------------------------------------------------------
XRootDStatus DoToken( FileSystem                      *fs,
                      Env                             *env,
                      const FSExecutor::CommandParams &args )
{
  Log *log = DefaultEnv::GetLog();

  enum
  {
    TokenValidityOption = 256,
    TokenIssuerOption
  };
  static const option options[] = {
    { "write",    no_argument,       nullptr, 'w' },
    { "validity", required_argument, nullptr, TokenValidityOption },
    { "issuer",   required_argument, nullptr, TokenIssuerOption },
    { nullptr, 0, nullptr, 0 }
  };

  bool writeAccess = false;
  std::uint64_t validity = 60;
  std::string issuer;
  std::string parseError;
  int firstOperand = 0;

  // getopt_long requires a mutable argv even though it does not modify the
  // argument strings. Keep this command's parser state isolated from main()
  // and from subsequent commands in interactive mode.
  std::vector<std::string> commandArguments( args );
  std::vector<char *> argv;
  argv.reserve( commandArguments.size() + 1 );
  for( std::string &argument : commandArguments )
    argv.push_back( argument.data() );
  argv.push_back( nullptr );

  // getopt_long accepts unique long-option abbreviations.  Reject them so the
  // command-first URL scanner and the command parser use the same grammar.
  for( std::size_t i = 1; i < commandArguments.size(); ++i )
  {
    const std::string &argument = commandArguments[i];
    if( argument == "--" || argument.empty() || argument[0] != '-' )
      break;
    if( argument.compare( 0, 2, "--" ) != 0 )
      continue;
    if( argument == "--write" )
      continue;
    if( argument == "--validity" || argument == "--issuer" )
    {
      if( i + 1 < commandArguments.size() ) ++i;
      continue;
    }
    if( argument.compare( 0, 11, "--validity=" ) == 0 ||
        argument.compare( 0, 9, "--issuer=" ) == 0 )
      continue;

    parseError = "Invalid token option: " + argument;
    break;
  }

  if( parseError.empty() )
  {
    ScopedGetoptState getoptState;
    int parsedOption = 0;
    while( (parsedOption = getopt_long(
              static_cast<int>( commandArguments.size() ), argv.data(),
              "+:w", options, nullptr )) != -1 )
    {
      switch( parsedOption )
      {
        case 'w':
          writeAccess = true;
          break;
        case TokenValidityOption:
          if( !ParseTokenValidity( optarg, validity ) )
            parseError = "Validity must be a number >= 0.";
          break;
        case TokenIssuerOption:
          issuer = optarg ? optarg : "";
          if( issuer.empty() )
            parseError = "Issuer URL cannot be empty.";
          break;
        case ':':
          if( optopt == TokenValidityOption )
            parseError = "Parameter '--validity' requires an argument.";
          else if( optopt == TokenIssuerOption )
            parseError = "Parameter '--issuer' requires an argument.";
          else
            parseError = "A token option requires an argument.";
          break;
        default:
        {
          const int invalidIndex = optind > 0 ? optind - 1 : 0;
          const std::string invalid =
            invalidIndex < static_cast<int>( commandArguments.size() ) ?
              commandArguments[invalidIndex] : std::string();
          parseError = invalid.empty() ? "Invalid token option." :
                       "Invalid token option: " + invalid;
          break;
        }
      }

      if( !parseError.empty() ) break;
    }
    firstOperand = optind;
  }

  if( !parseError.empty() )
  {
    log->Error( AppMsg, "%s", parseError.c_str() );
    return XRootDStatus( stError, errInvalidArgs, 0, parseError );
  }

  if( firstOperand >= static_cast<int>( commandArguments.size() ) )
  {
    const std::string error = "Missing path for token request.";
    log->Error( AppMsg, "%s", error.c_str() );
    return XRootDStatus( stError, errInvalidArgs, 0, error );
  }

  std::string path;
  if( !BuildPath( path, env, commandArguments[firstOperand] ).IsOK() )
  {
    const std::string error = "Invalid path for token request.";
    log->Error( AppMsg, "%s", error.c_str() );
    return XRootDStatus( stError, errInvalidArgs, 0, error );
  }

  std::string server;
  env->GetString( "ServerURL", server );
  URL endpoint( server );
  std::string protocol = endpoint.GetProtocol();
  std::transform( protocol.begin(), protocol.end(), protocol.begin(),
                  []( unsigned char character )
  {
    return static_cast<char>( std::tolower( character ) );
  } );
  if( protocol != "https" && protocol != "davs" )
  {
    const std::string error =
      "Token requests require an HTTPS or DAVS storage endpoint.";
    log->Error( AppMsg, "%s", error.c_str() );
    return XRootDStatus( stError, errNotSupported, 0, error );
  }

  std::vector<std::string> activities;
  for( std::size_t i = static_cast<std::size_t>( firstOperand + 1 );
       i < commandArguments.size(); ++i )
  {
    if( commandArguments[i].empty() )
    {
      const std::string error = "Token activities cannot be empty.";
      log->Error( AppMsg, "%s", error.c_str() );
      return XRootDStatus( stError, errInvalidArgs, 0, error );
    }
    activities.emplace_back( commandArguments[i] );
  }

  nlohmann::json request = {
    { "path", path },
    { "validity", validity },
    { "write", writeAccess },
    { "activities", activities }
  };
  if( !issuer.empty() ) request["issuer"] = issuer;

  Buffer argument;
  argument.FromString( request.dump() );

  Buffer *rawResponse = nullptr;
  XRootDStatus status = fs->Query(
    QueryCode::Visa, argument, rawResponse );
  std::unique_ptr<Buffer> response( rawResponse );
  if( !status.IsOK() )
  {
    // The path may contain an authz query parameter; do not copy it into the
    // client log on failure.
    log->Error( AppMsg, "Unable to request token: %s",
                status.ToStr().c_str() );
    return status;
  }
  if( !response )
  {
    const std::string error = "Token query returned no response.";
    log->Error( AppMsg, "%s", error.c_str() );
    return XRootDStatus( stError, errInvalidResponse, 0, error );
  }

  std::cout << response->ToString() << '\n';
  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Prepare files
//------------------------------------------------------------------------------
XRootDStatus DoPrepare( FileSystem                      *fs,
                        Env                             *env,
                        const FSExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log         *log     = DefaultEnv::GetLog();
  uint32_t     argc    = args.size();

  if( argc < 2 )
  {
    log->Error( AppMsg, "Wrong number of arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  PrepareFlags::Flags      flags       = PrepareFlags::None;
  std::vector<std::string> rawFiles;
  std::string              requestId;
  uint8_t                  priority    = 0;
  std::string              pinLifetime;
  std::string              targetedMetadata;
  bool                     waitPolling = false;
  uint32_t                 timeoutSecs = 0;
  bool                     parseOptions = true;

  for( uint32_t i = 1; i < args.size(); ++i )
  {
    const std::string &arg = args[i];
    if( parseOptions && arg == "--" )
    {
      parseOptions = false;
      continue;
    }
    if( parseOptions && (arg == "-p" || arg == "--priority") )
    {
      if( i + 1 < args.size() )
      {
        char *result = nullptr;
        int32_t param = ::strtol( args[i+1].c_str(), &result, 0 );
        if( *result != 0 || param > 3 || param < 0 )
        {
          log->Error( AppMsg, "Priority needs to be an integer between 0 and 3." );
          return XRootDStatus( stError, errInvalidArgs );
        }
        priority = (uint8_t)param;
        ++i;
      }
      else
      {
        log->Error( AppMsg, "Parameter '%s' requires an argument.", arg.c_str() );
        return XRootDStatus( stError, errInvalidArgs );
      }
    }
    else if( parseOptions && (arg == "-c" || arg == "--colocate") )
      flags |= PrepareFlags::Colocate;
    else if( parseOptions && (arg == "-f" || arg == "--fresh") )
      flags |= PrepareFlags::Fresh;
    else if( parseOptions && (arg == "-s" || arg == "--stage") )
      flags |= PrepareFlags::Stage;
    else if( parseOptions && (arg == "-w" || arg == "--write") )
      flags |= PrepareFlags::WriteMode;
    else if( parseOptions && (arg == "-e" || arg == "--evict" || arg == "--release") )
      flags |= PrepareFlags::Evict;
    else if( parseOptions && (arg == "-a" || arg == "--abort" || arg == "--cancel") )
    {
      flags |= PrepareFlags::Cancel;
      if( i + 1 < args.size() )
      {
        requestId = args[i+1];
        ++i;
      }
      else
      {
        log->Error( AppMsg, "Parameter '%s' requires a request ID.", arg.c_str() );
        return XRootDStatus( stError, errInvalidArgs );
      }
    }
    else if( parseOptions && (arg == "--pin-lifetime" || arg == "--disk-lifetime") )
    {
      if( i + 1 < args.size() )
      {
        pinLifetime = args[i+1];
        ++i;
      }
      else
      {
        log->Error( AppMsg, "Parameter '%s' requires a duration argument.", arg.c_str() );
        return XRootDStatus( stError, errInvalidArgs );
      }
    }
    else if( parseOptions && (arg == "--metadata" || arg == "--staging-metadata") )
    {
      if( i + 1 < args.size() )
      {
        targetedMetadata = args[i+1];
        ++i;
      }
      else
      {
        log->Error( AppMsg, "Parameter '%s' requires a JSON object argument.", arg.c_str() );
        return XRootDStatus( stError, errInvalidArgs );
      }
    }
    else if( parseOptions && arg == "--wait" )
    {
      waitPolling = true;
    }
    else if( parseOptions && arg == "--timeout" )
    {
      if( i + 1 < args.size() )
      {
        char *result = nullptr;
        long param = ::strtol( args[i+1].c_str(), &result, 0 );
        if( *result != 0 || param < 0 )
        {
          log->Error( AppMsg, "Timeout must be a non-negative integer." );
          return XRootDStatus( stError, errInvalidArgs );
        }
        timeoutSecs = static_cast<uint32_t>( param );
        waitPolling = true;
        ++i;
      }
      else
      {
        log->Error( AppMsg, "Parameter '--timeout' requires an integer argument." );
        return XRootDStatus( stError, errInvalidArgs );
      }
    }
    else
    {
      rawFiles.push_back( arg );
    }
  }

  if( (flags & PrepareFlags::Cancel) && !requestId.empty() )
  {
    rawFiles.insert( rawFiles.begin(), requestId );
  }

  if( rawFiles.empty() )
  {
    log->Error( AppMsg, "Filename or request ID missing." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  if( !pinLifetime.empty() && std::all_of( pinLifetime.begin(), pinLifetime.end(), ::isdigit ) )
  {
    pinLifetime = "PT" + pinLifetime + "S";
  }

  std::string server;
  env->GetString( "ServerURL", server );
  URL endpointUrl( server );
  std::string protocol = endpointUrl.GetProtocol();
  std::transform( protocol.begin(), protocol.end(), protocol.begin(),
                  []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
  const bool isHttp = (protocol == "http" || protocol == "https" ||
                       protocol == "dav" || protocol == "davs");

  std::vector<std::string> files;
  files.reserve( rawFiles.size() );
  for( size_t idx = 0; idx < rawFiles.size(); ++idx )
  {
    const std::string &path = rawFiles[idx];
    if( (flags & PrepareFlags::Cancel) && idx == 0 )
    {
      files.push_back( path );
      continue;
    }
    if( (flags & PrepareFlags::Evict) && rawFiles.size() > 1 && idx == 0 && path.find('/') == std::string::npos )
    {
      files.push_back( path );
      continue;
    }

    if( isHttp && (flags & PrepareFlags::Stage) && (!pinLifetime.empty() || !targetedMetadata.empty()) )
    {
      nlohmann::json obj;
      obj["path"] = path;
      if( !pinLifetime.empty() ) obj["diskLifetime"] = pinLifetime;
      if( !targetedMetadata.empty() )
      {
        auto metaJson = nlohmann::json::parse( targetedMetadata, nullptr, false );
        if( metaJson.is_object() ) obj["targetedMetadata"] = metaJson;
      }
      files.push_back( "xrdclhttp.tape.stage:" + obj.dump() );
    }
    else
    {
      files.push_back( path );
    }
  }

  //----------------------------------------------------------------------------
  // Run the command
  //----------------------------------------------------------------------------
  Buffer *response = nullptr;
  XRootDStatus st = fs->Prepare( files, flags, priority, response );
  std::unique_ptr<Buffer> respPtr( response );
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Prepare request failed: %s", st.ToStr().c_str() );
    return st;
  }

  std::string respStr;
  if( response )
  {
    respStr = response->ToString();
    while( !respStr.empty() && (respStr.back() == '\0' || respStr.back() == '\n' || respStr.back() == '\r') )
      respStr.pop_back();
  }

  if( ( flags & PrepareFlags::Stage ) && !respStr.empty() )
  {
    std::cout << respStr << '\n';
  }

  if( ( flags & PrepareFlags::Stage ) && waitPolling && !respStr.empty() )
  {
    std::string pollReqId = respStr;
    auto json = nlohmann::json::parse( respStr, nullptr, false );
    if( json.is_object() && json.contains( "requestId" ) )
      pollReqId = json["requestId"].get<std::string>();

    uint32_t sleepSeconds = 1;
    uint32_t elapsed = 0;
    while( true )
    {
      if( timeoutSecs > 0 && elapsed >= timeoutSecs )
      {
        std::cout << "Stage request " << pollReqId << " polling timed out after " << timeoutSecs << " seconds.\n";
        break;
      }
      sleep( sleepSeconds );
      elapsed += sleepSeconds;
      sleepSeconds = std::min( sleepSeconds * 2, 30u );

      Buffer qArg;
      qArg.FromString( pollReqId );
      Buffer *qResp = nullptr;
      XRootDStatus qSt = fs->Query( QueryCode::Prepare, qArg, qResp );
      std::unique_ptr<Buffer> qRespPtr( qResp );
      if( !qSt.IsOK() || !qResp ) continue;

      std::string qStr = qResp->ToString();
      auto qJson = nlohmann::json::parse( qStr, nullptr, false );
      if( qJson.is_object() && qJson.contains( "files" ) && qJson["files"].is_array() )
      {
        size_t total = qJson["files"].size();
        size_t completed = 0;
        size_t failed = 0;
        for( const auto &f : qJson["files"])
        {
          if( !f.is_object() ) continue;

          if( f.contains( "state" ) && f["state"].is_string() )
          {
            std::string stStr = f["state"].get<std::string>();
            if( stStr == "COMPLETED" ) ++completed;
            else if( stStr == "FAILED" || stStr == "CANCELLED" ) ++failed;
          }
          else if( f.value( "onDisk", false ) )
          {
            ++completed;
          }
          else if( f.contains( "error" ) && f["error"].is_string() &&
                   !f["error"].get_ref<const std::string &>().empty() )
          {
            ++failed;
          }
        }
        if( completed + failed == total )
        {
          if( failed > 0 )
            std::cout << "Stage request " << pollReqId << " completed with " << failed << " failures.\n";
          else
            std::cout << "Stage request " << pollReqId << " completed successfully.\n";
          break;
        }
      }
    }
  }

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Copy progress handler
//------------------------------------------------------------------------------
class ProgressDisplay: public XrdCl::CopyProgressHandler
{
  public:
    //--------------------------------------------------------------------------
    // Constructor
    //--------------------------------------------------------------------------
    ProgressDisplay(): pBytesProcessed(0), pBytesTotal(0), pPrevious(0)
    {}

    //--------------------------------------------------------------------------
    // End job
    //--------------------------------------------------------------------------
    virtual void EndJob( uint32_t jobNum, const XrdCl::PropertyList *results )
    {
      JobProgress( jobNum, pBytesProcessed, pBytesTotal );
      std::cerr << std::endl;
    }

    //--------------------------------------------------------------------------
    // Job progress
    //--------------------------------------------------------------------------
    virtual void JobProgress( uint32_t jobNum,
                              uint64_t bytesProcessed,
                              uint64_t bytesTotal )
    {
      pBytesProcessed = bytesProcessed;
      pBytesTotal     = bytesTotal;

      time_t now = time(0);
      if( (now - pPrevious < 1) && (bytesProcessed != bytesTotal) )
        return;
      pPrevious = now;

      std::cerr << "\r";
      std::cerr << "Progress: ";
      std::cerr << XrdCl::Utils::BytesToString(bytesProcessed) << "B ";

      if( bytesTotal )
        std::cerr << "(" << bytesProcessed*100/bytesTotal << "%)";

      std::cerr << std::flush;
     }

  private:
    uint64_t          pBytesProcessed;
    uint64_t          pBytesTotal;
    time_t            pPrevious;
};

//------------------------------------------------------------------------------
// Cat a file
//------------------------------------------------------------------------------
XRootDStatus DoCat( FileSystem                      *fs,
                    Env                             *env,
                    const FSExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log         *log     = DefaultEnv::GetLog();
  uint32_t     argc    = args.size();

  if( argc < 2 )
  {
    log->Error( AppMsg, "Wrong number of arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  std::string server;
  env->GetString( "ServerURL", server );
  if( server.empty() )
  {
    log->Error( AppMsg, "Invalid address: \"%s\".", server.c_str() );
    return XRootDStatus( stError, errInvalidAddr );
  }

  std::vector<std::string> remotes;
  std::string local;
  bool parseOptions = true;

  for( uint32_t i = 1; i < args.size(); ++i )
  {
    if( parseOptions && args[i] == "--" )
    {
      parseOptions = false;
    }
    else if( parseOptions &&
             ( args[i] == "-b" || args[i] == "--bytes" ) )
    {
      // gfal-cat exposes this Python 3 compatibility flag. xrdfs always
      // streams byte-preserving data, so no mode change is required.
      continue;
    }
    else if( parseOptions && args[i] == "-o" )
    {
      if( i < args.size()-1 )
      {
        local = args[i+1];
        ++i;
      }
      else
      {
        log->Error( AppMsg, "Parameter '-o' requires an argument." );
        return XRootDStatus( stError, errInvalidArgs );
      }
    }
    else
      remotes.emplace_back( args[i] );
  }

  if( !local.empty() && remotes.size() > 1 )
  {
    log->Error( AppMsg, "If '-o' is used only can be used with only one remote file." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  if( remotes.empty() )
  {
    log->Error( AppMsg, "Missing remote file." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  std::vector<URL> remoteUrls;
  remoteUrls.reserve( remotes.size() );
  for( auto &remote : remotes )
  {
    std::string remoteFile;
    XRootDStatus pathSt = BuildPath( remoteFile, env, remote, "Opening" );
    if( !pathSt.IsOK() )
      return pathSt;

    remoteUrls.emplace_back( server );
    remoteUrls.back().SetPath( remoteFile );
  }

  //----------------------------------------------------------------------------
  // Fetch the data
  //----------------------------------------------------------------------------
  CopyProgressHandler *handler = 0; ProgressDisplay d;
  CopyProcess process;
  std::vector<PropertyList> props( remoteUrls.size() ), results( remoteUrls.size() );

  for( size_t i = 0; i < remoteUrls.size(); ++i )
  {
    props[i].Set( "source", remoteUrls[i].GetURL() );
    if( !local.empty() )
    {
      props[i].Set( "target", std::string( "file://" ) + local );
      handler = &d;
    }
    else
      props[i].Set( "target", "stdio://-" );

    props[i].Set( "dynamicSource", true );

    XRootDStatus st = process.AddJob( props[i], &results[i] );
    if( !st.IsOK() )
    {
      log->Error( AppMsg, "Job adding failed: %s.", st.ToStr().c_str() );
      return st;
    }
  }

  XRootDStatus st = process.Prepare();
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Copy preparation failed: %s.", st.ToStr().c_str() );
    return st;
  }

  st = process.Run(handler);
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Cope process failed: %s.", st.ToStr().c_str() );
    return st;
  }

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Tail a file
//------------------------------------------------------------------------------
XRootDStatus DoTail( FileSystem                      *fs,
                     Env                             *env,
                     const FSExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log         *log     = DefaultEnv::GetLog();
  uint32_t     argc    = args.size();

  if( argc < 2 || argc > 5 )
  {
    log->Error( AppMsg, "Wrong number of arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  std::string server;
  env->GetString( "ServerURL", server );
  if( server.empty() )
  {
    log->Error( AppMsg, "Invalid address: \"%s\".", server.c_str() );
    return XRootDStatus( stError, errInvalidAddr );
  }

  std::string remote;
  bool        followMode = false;
  uint32_t    offset     = 512;

  for( uint32_t i = 1; i < args.size(); ++i )
  {
    if( args[i] == "-f" )
      followMode = true;
    else if( args[i] == "-c" )
    {
      if( i < args.size()-1 )
      {
        char *result;
        offset = ::strtol( args[i+1].c_str(), &result, 0 );
        if( *result != 0 )
        {
          log->Error( AppMsg, "Offset from the end needs to be a number: %s",
                      args[i+1].c_str() );
          return XRootDStatus( stError, errInvalidArgs );
        }
        ++i;
      }
      else
      {
        log->Error( AppMsg, "Parameter '-n' requires an argument." );
        return XRootDStatus( stError, errInvalidArgs );
      }
    }
    else
      remote = args[i];
  }

  std::string remoteFile;
  XRootDStatus pathSt = BuildPath( remoteFile, env, remote, "Opening" );
  if( !pathSt.IsOK() )
    return pathSt;

  URL remoteUrl( server );
  remoteUrl.SetPath( remoteFile );

  //----------------------------------------------------------------------------
  // Fetch the data
  //----------------------------------------------------------------------------
  File file;
  XRootDStatus st = file.Open( remoteUrl.GetURL(), OpenFlags::Read );
  if( !st.IsOK() )
  {
    log->Error( AppMsg, "Unable to open file %s: %s",
                remoteUrl.GetObfuscatedURL().c_str(), st.ToStr().c_str() );
    return st;
  }

  StatInfo *info = 0;
  uint64_t size = 0;
  st = file.Stat( false, info );
  if (st.IsOK()) size = info->GetSize();

  if( size < offset )
    offset = 0;
  else
    offset = size - offset;

  uint32_t  chunkSize = 1*1024*1024;
  char     *buffer    = new char[chunkSize];
  uint32_t  bytesRead = 0;
  while(1)
  {
    st = file.Read( offset, chunkSize, buffer, bytesRead );
    if( !st.IsOK() )
    {
      log->Error( AppMsg, "Unable to read from %s: %s",
                  remoteUrl.GetObfuscatedURL().c_str(), st.ToStr().c_str() );
      delete [] buffer;
      return st;
    }

    offset += bytesRead;
    int ret = write( 1, buffer, bytesRead );
    if( ret == -1 )
    {
      log->Error( AppMsg, "Unable to write to stdout: %s",
                  XrdSysE2T(errno) );
      delete [] buffer;
      return st;
    }

    if( bytesRead < chunkSize )
    {
      if( !followMode )
        break;
      sleep(1);
    }
  }
  delete [] buffer;

  XRootDStatus stC = file.Close();

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Print statistics concerning given space
//------------------------------------------------------------------------------
XRootDStatus DoSpaceInfo( FileSystem                      *fs,
                          Env                             *env,
                          const FSExecutor::CommandParams &args )
{
  using namespace XrdCl;

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

  FileSystemUtils::SpaceInfo *i = 0;

  XRootDStatus st = FileSystemUtils::GetSpaceInfo( i, fs, args[1] );
  if( !st.IsOK() )
    return st;

  if( st.code == suPartial )
  {
    std::cerr << "[!] Some of the requests failed. The result may be ";
    std::cerr << "incomplete." << std::endl;
  }

  std::cout << "Path:               " << args[1]                  << std::endl;
  std::cout << "Total:              " << i->GetTotal()            << std::endl;
  std::cout << "Free:               " << i->GetFree()             << std::endl;
  std::cout << "Used:               " << i->GetUsed()             << std::endl;
  std::cout << "Largest free chunk: " << i->GetLargestFreeChunk() << std::endl;

  delete i;
  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Return whether the active filesystem uses the native XRootD protocol
//------------------------------------------------------------------------------
bool IsXRootDProtocol( Env *env )
{
  std::string server;
  env->GetString( "ServerURL", server );
  URL url( server );
  std::string protocol = url.GetProtocol();
  std::transform( protocol.begin(), protocol.end(), protocol.begin(),
                  []( unsigned char character )
  {
    return static_cast<char>( std::tolower( character ) );
  } );
  return protocol == "root" || protocol == "roots" ||
         protocol == "xroot" || protocol == "xroots";
}

//------------------------------------------------------------------------------
// Query a text response through an existing XrdCl query code
//------------------------------------------------------------------------------
XRootDStatus QueryText( FileSystem             *fs,
                        QueryCode::Code         code,
                        const std::string      &path,
                        std::string            &value )
{
  Buffer request( path.size() );
  request.FromString( path );
  Buffer *rawResponse = 0;
  XRootDStatus status = fs->Query( code, request, rawResponse );
  std::unique_ptr<Buffer> response( rawResponse );
  if( !status.IsOK() ) return status;
  if( !response )
    return XRootDStatus( stError, errInvalidResponse, 0,
                         "Query returned no response." );
  value = response->ToString();
  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Return whether an attribute name belongs to gfal2's virtual namespace
//------------------------------------------------------------------------------
bool IsGFALVirtualXAttr( const std::string &attribute )
{
  static const std::string checksumPrefix = "user.checksum.";
  return attribute.compare( 0, checksumPrefix.size(), checksumPrefix ) == 0 ||
         attribute == "xroot.cksum" || attribute == "xroot.space" ||
         attribute == "xroot.xattr" || attribute == "spacetoken" ||
         attribute == "user.status" ||
         attribute == "taperestapi.version" ||
         attribute == "taperestapi.uri" ||
         attribute == "taperestapi.sitename";
}

namespace
{
  bool UsesWebDAVProtocol( Env *env )
  {
    std::string server;
    env->GetString( "ServerURL", server );
    return IsWebDAVProtocol( URL( server ).GetProtocol() );
  }

  XRootDStatus QueryTapeJson( FileSystem             *fs,
                              const std::string      &request,
                              nlohmann::json         &response )
  {
    std::string rawResponse;
    XRootDStatus status = QueryText(
      fs, QueryCode::Opaque, request, rawResponse );
    if( !status.IsOK() ) return status;

    response = nlohmann::json::parse( rawResponse, nullptr, false );
    if( response.is_discarded() )
      return XRootDStatus( stError, errInvalidResponse, 0,
                           "Tape REST query returned malformed JSON." );
    return XRootDStatus();
  }

  XRootDStatus ReadTapeDiscoveryAttribute( const nlohmann::json &discovery,
                                           const std::string    &attribute,
                                           std::string          &value )
  {
    static const std::string prefix = "taperestapi.";
    const std::string field = attribute.substr( prefix.size() );
    if( !discovery.is_object() || !discovery.contains( field ) ||
        !discovery[field].is_string() )
      return XRootDStatus( stError, errInvalidResponse, 0,
                           "Tape REST discovery attribute is missing: " +
                           field );
    value = discovery[field].get<std::string>();
    return XRootDStatus();
  }

  XRootDStatus GetTapeDiscoveryAttribute( FileSystem        *fs,
                                          const std::string &attribute,
                                          std::string       &value )
  {
    nlohmann::json discovery;
    XRootDStatus status = QueryTapeJson(
      fs, "tape.discover", discovery );
    if( !status.IsOK() ) return status;
    return ReadTapeDiscoveryAttribute( discovery, attribute, value );
  }

  XRootDStatus GetTapeFileStatus( FileSystem        *fs,
                                  Env               *env,
                                  const std::string &path,
                                  std::string       &value )
  {
    std::string server;
    env->GetString( "ServerURL", server );
    URL fileUrl( server );
    SetEndpointPath( fileUrl, path );

    nlohmann::json archiveInfo;
    XRootDStatus status = QueryTapeJson(
      fs, "tape.archiveinfo\n" + fileUrl.GetURL(), archiveInfo );
    if( !status.IsOK() ) return status;
    if( !archiveInfo.is_array() )
      return XRootDStatus( stError, errInvalidResponse, 0,
                           "Tape REST archive info is not an array." );

    for( const auto &entry : archiveInfo )
    {
      if( !entry.is_object() ||
          (entry.value( "url", "" ) != fileUrl.GetURL() &&
           entry.value( "path", "" ) != path ) )
        continue;
      if( entry.contains( "error" ) && entry["error"].is_string() )
        return XRootDStatus( stError, errErrorResponse, kXR_NotFound,
                             entry["error"].get<std::string>() );
      if( !entry.contains( "locality" ) || !entry["locality"].is_string() )
        return XRootDStatus( stError, errInvalidResponse, 0,
                             "Tape REST locality is missing." );

      const std::string locality = entry["locality"].get<std::string>();
      const char *fileStatus = GetGFALTapeFileStatus( locality );
      if( !fileStatus )
        return XRootDStatus( stError, errInvalidResponse, 0,
                             "Unsupported Tape REST locality: " + locality );
      value = fileStatus;
      return XRootDStatus();
    }

    return XRootDStatus( stError, errInvalidResponse, 0,
                         "Tape REST archive info omitted the requested path." );
  }
}

//------------------------------------------------------------------------------
// Read one native XRootD file attribute without changing its value formatting
//------------------------------------------------------------------------------
XRootDStatus GetNativeXAttrValue( FileSystem        *fs,
                                 const std::string &path,
                                 const std::string &attribute,
                                 std::string       &value )
{
  std::vector<std::string> attributes( 1, attribute );
  std::vector<XAttr> result;
  XRootDStatus status = fs->GetXAttr( path, attributes, result );
  if( !status.IsOK() ) return status;
  if( result.empty() )
    return XRootDStatus( stError, errInvalidResponse, 0,
                         "Attribute query returned no response." );
  if( !result.front().status.IsOK() ) return result.front().status;
  value = result.front().value;
  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Resolve gfal2's virtual XRootD attributes through native XrdCl operations
//------------------------------------------------------------------------------
XRootDStatus GetGFALVirtualXAttr( FileSystem        *fs,
                                 Env               *env,
                                 const std::string &path,
                                 const std::string &attribute,
                                 std::string       &value )
{
  static const std::string checksumPrefix = "user.checksum.";
  static const std::string tapePrefix = "taperestapi.";
  const bool isXRootD = IsXRootDProtocol( env );

  if( attribute.compare( 0, checksumPrefix.size(), checksumPrefix ) == 0 )
  {
    const std::string requested = attribute.substr( checksumPrefix.size() );
    if( requested.empty() )
      return XRootDStatus( stError, errInvalidArgs, 0,
                           "Checksum type cannot be empty." );
    std::string algorithm;
    std::string digest;
    XRootDStatus status = QueryChecksum(
      fs, path, requested, algorithm, digest );
    if( status.IsOK() ) value = std::move( digest );
    return status;
  }

  if( !isXRootD )
  {
    if( UsesWebDAVProtocol( env ) )
    {
      if( attribute.compare( 0, tapePrefix.size(), tapePrefix ) == 0 )
        return GetTapeDiscoveryAttribute( fs, attribute, value );
      if( attribute == "user.status" )
        return GetTapeFileStatus( fs, env, path, value );
    }
    return XRootDStatus(
      stError, errNotSupported, 0,
      "GFAL virtual attribute is not available for this protocol: " +
      attribute );
  }

  if( attribute == "xroot.cksum" )
  {
    std::string algorithm;
    std::string digest;
    XRootDStatus status = QueryChecksum(
      fs, path, "", algorithm, digest );
    if( status.IsOK() ) value = algorithm + " " + digest;
    return status;
  }

  if( attribute == "xroot.space" )
    return QueryText( fs, QueryCode::Space, path, value );

  if( attribute == "xroot.xattr" )
    return QueryText( fs, QueryCode::XAttr, path, value );

  if( attribute == "spacetoken" )
  {
    FileSystemUtils::SpaceInfo *rawInfo = 0;
    XRootDStatus status = FileSystemUtils::GetSpaceInfo( rawInfo, fs, path );
    std::unique_ptr<FileSystemUtils::SpaceInfo> info( rawInfo );
    if( !status.IsOK() ) return status;
    if( !info )
      return XRootDStatus( stError, errInvalidResponse, 0,
                           "Space query returned no response." );

    std::ostringstream output;
    output << "{ \"totalsize\": " << info->GetTotal()
           << ", \"unusedsize\": " << info->GetFree()
           << ", \"usedsize\": " << info->GetUsed()
           << ", \"guaranteedsize\": " << info->GetLargestFreeChunk()
           << " }";
    value = output.str();
    return XRootDStatus();
  }

  if( attribute == "user.status" )
  {
    StatInfo *rawInfo = 0;
    XRootDStatus status = fs->Stat( path, rawInfo );
    std::unique_ptr<StatInfo> info( rawInfo );
    if( !status.IsOK() ) return status;
    if( !info )
      return XRootDStatus( stError, errInvalidResponse, 0,
                           "Stat returned no response." );
    value = GetGFALFileStatus(
      info->TestFlags( StatInfo::Offline ),
      info->TestFlags( StatInfo::BackUpExists ) );
    return XRootDStatus();
  }

  return XRootDStatus( stError, errNotFound, 0,
                        "Unknown GFAL virtual attribute: " + attribute );
}

//------------------------------------------------------------------------------
// Carry out xattr operation
//------------------------------------------------------------------------------
XRootDStatus DoXAttr( FileSystem                      *fs,
                      Env                             *env,
                      const FSExecutor::CommandParams &args )
{
  //----------------------------------------------------------------------------
  // Check up the args
  //----------------------------------------------------------------------------
  Log         *log     = DefaultEnv::GetLog();
  uint32_t     argc    = args.size();

  if( argc < 2 )
  {
    log->Error( AppMsg, "Wrong number of arguments." );
    return XRootDStatus( stError, errInvalidArgs );
  }
  if( argc == 3 && args[2] == "--" )
  {
    log->Error( AppMsg, "Missing attribute name after '--'." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  const bool implicitList = argc == 2;
  const bool delimitedGet = argc == 4 && args[2] == "--";
  const bool implicitGet = delimitedGet ||
                           (argc == 3 && args[2] != "set" &&
                            args[2] != "get" && args[2] != "del" &&
                            args[2] != "list");

  kXR_char code = 0;
  if( !implicitList && !implicitGet && args[2] == "set")
    code = kXR_fattrSet;
  else if( !implicitList && !implicitGet && args[2] == "get" )
    code = kXR_fattrGet;
  else if( !implicitList && !implicitGet && args[2] == "del" )
    code = kXR_fattrDel;
  else if( !implicitList && !implicitGet && args[2] == "list" )
    code = kXR_fattrList;
  else if( !implicitList && !implicitGet )
  {
    log->Error( AppMsg, "Invalid xattr code." );
    return XRootDStatus( stError, errInvalidArgs );
  }

  std::string path;
  XRootDStatus pathSt = BuildPath( path, env, args[1], "Accessing" );
  if( !pathSt.IsOK() )
    return pathSt;

  if( implicitGet )
  {
    const std::string &attribute = delimitedGet ? args[3] : args[2];
    std::string value;
    XRootDStatus status = IsGFALVirtualXAttr( attribute ) ?
      GetGFALVirtualXAttr( fs, env, path, attribute, value ) :
      GetNativeXAttrValue( fs, path, attribute, value );
    if( !status.IsOK() )
      log->Error( AppMsg, "Unable to get attribute %s: %s",
                  attribute.c_str(), status.ToStr().c_str() );
    else
      std::cout << value << '\n';
    return status;
  }

  if( implicitList )
  {
    const bool isXRootD = IsXRootDProtocol( env );
    const bool isWebDAV = UsesWebDAVProtocol( env );
    if( !isXRootD && !isWebDAV )
    {
      log->Error( AppMsg,
                  "Virtual attribute listing is not available for this protocol." );
      return XRootDStatus( stError, errNotSupported );
    }

    static const char *xrootdAttributes[] = {
      "xroot.cksum", "xroot.space", "xroot.xattr", "spacetoken"
    };
    static const char *tapeAttributes[] = {
      "taperestapi.version", "taperestapi.uri", "taperestapi.sitename"
    };

    if( isWebDAV )
    {
      nlohmann::json discovery;
      XRootDStatus status = QueryTapeJson(
        fs, "tape.discover", discovery );
      if( !status.IsOK() ) return status;
      for( const char *attribute : tapeAttributes )
      {
        std::string value;
        status = ReadTapeDiscoveryAttribute( discovery, attribute, value );
        if( !status.IsOK() ) return status;
        std::cout << attribute << " = " << value << '\n';
      }
      return XRootDStatus();
    }

    for( const char *attribute : xrootdAttributes )
    {
      std::string value;
      XRootDStatus status = GetGFALVirtualXAttr(
        fs, env, path, attribute, value );
      if( status.IsOK() )
        std::cout << attribute << " = " << value << '\n';
      else
        std::cout << attribute << " FAILED: " << status.ToStr() << '\n';
    }
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Issue the xattr operation
  //----------------------------------------------------------------------------
  XRootDStatus status;
  switch( code )
  {
    case kXR_fattrSet:
    {
      if( argc != 4 )
      {
        log->Error( AppMsg, "Wrong number of arguments." );
        return XRootDStatus( stError, errInvalidArgs );
      }

      std::string key_value = args[3];
      size_t pos = key_value.find( '=' );
      std::string key   = key_value.substr( 0, pos );
      std::string value = key_value.substr( pos + 1 );
      std::vector<xattr_t> attrs;
      attrs.push_back( std::make_tuple( key, value ) );

      std::vector<XAttrStatus> result;
      XRootDStatus status = fs->SetXAttr( path, attrs, result );
      XAttrStatus xst = status.IsOK() ? result.front() : XAttrStatus( key, status );

      if( !xst.status.IsOK() )
        status = xst.status;

      if( !status.IsOK() )
        log->Error( AppMsg, "Unable to xattr set %s %s: %s",
                            key.c_str(), value.c_str(),
                            status.ToStr().c_str() );
      return status;
    }

    case kXR_fattrGet:
    {
      if( argc != 4 )
      {
        log->Error( AppMsg, "Wrong number of arguments." );
        return XRootDStatus( stError, errInvalidArgs );
      }

      std::string key = args[3];
      std::vector<std::string> attrs;
      attrs.push_back( key );

      std::vector<XAttr> result;
      XRootDStatus status = fs->GetXAttr( path, attrs, result );
      XAttr xattr = status.IsOK() ? result.front() : XAttr( key, status );

      if( !xattr.status.IsOK() )
        status = xattr.status;

      if( !status.IsOK() )
        log->Error( AppMsg, "Unable to xattr get %s : %s",
                            key.c_str(),
                            status.ToStr().c_str() );
      else
      {
        std::cout << "# file: " << path << '\n';
        std::cout << xattr.name << "=\"" << xattr.value << "\"\n";
      }

      return status;
    }

    case kXR_fattrDel:
    {
      if( argc != 4 )
      {
        log->Error( AppMsg, "Wrong number of arguments." );
        return XRootDStatus( stError, errInvalidArgs );
      }

      std::string key = args[3];
      std::vector<std::string> attrs;
      attrs.push_back( key );

      std::vector<XAttrStatus> result ;
      XRootDStatus status = fs->DelXAttr( path, attrs, result );
      XAttrStatus xst = status.IsOK() ? result.front() : XAttrStatus( key, status );

      if( !xst.status.IsOK() )
        status = xst.status;

      if( !status.IsOK() )
        log->Error( AppMsg, "Unable to xattr del %s : %s",
                            key.c_str(),
                            status.ToStr().c_str() );
      return status;
    }

    case kXR_fattrList:
    {
      if( argc != 3 )
      {
        log->Error( AppMsg, "Wrong number of arguments." );
        return XRootDStatus( stError, errInvalidArgs );
      }

      std::vector<XAttr> result;
      XRootDStatus status = fs->ListXAttr( path, result );

      if( !status.IsOK() )
        log->Error( AppMsg, "Unable to xattr list : %s",
                            status.ToStr().c_str() );
      else
      {
        std::cout << "# file: " << path << '\n';
        auto itr = result.begin();
        for( ; itr != result.end(); ++itr )
          std::cout << itr->name << "=\"" << itr->value << "\"\n";
      }

      return status;
    }

    default:
      return XRootDStatus( stError, errInvalidAddr );
  }
}

//------------------------------------------------------------------------------
// Print help
//------------------------------------------------------------------------------
XRootDStatus PrintHelp( FileSystem *, Env *,
                        const FSExecutor::CommandParams & )
{
  printf( "Usage:\n"                                                          );
  printf( "   xrdfs [options] host[:port]              - interactive mode\n"  );
  printf( "   xrdfs [options] host[:port] command args - server-first batch\n" );
  printf( "   xrdfs [options] command args-with-URLs   - command-first batch\n\n" );

  printf( "The two batch forms cannot be mixed.\n\n"                          );

  printf( "Available options:\n\n"                                            );

  printf( "   -4, --ipv4          use only the IPv4 network stack\n"          );
  printf( "   -6, --ipv6          use only the IPv6 network stack\n"          );
  printf( "   -d, --debug <level> set debug level: 0 off, 1 low, 2 medium,\n" );
  printf( "                       3 high\n"                                  );
  printf( "   -h, --help show this help\n"                                    );
  printf( "   --no-cwd    do not preset a CWD in interactive mode\n\n"        );

  printf( "Available commands:\n\n"                                           );

  printf( "   exit\n"                                                         );
  printf( "     Exits from the program.\n\n"                                  );

  printf( "   help\n"                                                         );
  printf( "     This help screen.\n\n"                                        );

  printf( "   cache {evict | fevict} <path>\n"                                );
  printf( "     Evict a file from a cache if not in use; while fevict\n"      );
  printf( "     forcibly evicts the file causing any current uses of the\n"   );
  printf( "     file to get read failures on a subsequent read\n\n"           );

  printf( "   cd <path>\n"                                                    );
  printf( "     Change the current working directory\n\n"                     );

  printf( "   chmod <path> <mode>\n"                                           );
  printf( "   chmod <octal-mode> <path>\n"                                     );
  printf( "     Modify permissions. Modes may be symbolic (for example,\n"      );
  printf( "     rwxr-x--x) or octal (for example, 0751). The mode-first\n"      );
  printf( "     form accepts octal modes.\n\n"                                 );

  printf( "   ls [-l] [-u] [-R] [-D] [-Z] [-C] [-h|-H] [-d] [-a]\n"       );
  printf( "      [--color=never] [--xattr name] [dirname]\n"               );
  printf( "     Get directory listing.\n"                                     );
  printf( "     -l|--long stat every entry and print long listing\n"          );
  printf( "     -u print paths as URLs\n"                                     );
  printf( "     -R list subdirectories recursively\n"                         );
  printf( "     -D show duplicate entries\n"                                  );
  printf( "     -Z if a ZIP archive list its content\n"                       );
  printf( "     -C checksum every entry\n"                                    );
  printf( "     -h|-H|--human-readable print human-readable sizes\n"           );
  printf( "     -d|--directory list the entry instead of its contents\n"       );
  printf( "     -a|--all accepted for gfal-ls compatibility; xrdfs already\n" );
  printf( "        includes entries whose names begin with a dot\n"            );
  printf( "     --color=never accepted for uncolored gfal-ls compatibility\n" );
  printf( "     --xattr name append an attribute value to long output; may\n" );
  printf( "        be repeated and has no visible effect without -l\n"         );
  printf( "     -- stop option parsing, allowing a dash-prefixed path\n\n"     );

  printf( "   locate [-n] [-r] [-d] [-m] [-i] [-p] <path>\n"                  );
  printf( "     Get the locations of the path.\n"                             );
  printf( "     -r refresh, don't use cached locations\n"                     );
  printf( "     -n make the server return the response immediately even\n"    );
  printf( "        though it may be incomplete\n"                             );
  printf( "     -d do a recursive (deep) locate\n"                            );
  printf( "     -m|-h prefer host names to IP addresses\n"                    );
  printf( "     -i ignore network dependencies\n"                             );
  printf( "     -p be passive: ignore tried/triedrc cgi opaque info\n\n"      );

  printf( "   mkdir [-p|--parents] [-m mode|--mode mode] <dirname>...\n"       );
  printf( "     Creates one or more directories/trees of directories. Modes\n" );
  printf( "     may be symbolic or octal; the default remains 0750. Use --\n"  );
  printf( "     before a directory name beginning with a dash.\n\n"           );

  printf( "   mv <path1> <path2>\n"                                           );
  printf( "     Move path1 to path2 locally on the same server.\n\n"          );

  printf( "   stat [-q query] <path>\n"                                       );
  printf( "     Get info about the file or directory.\n"                      );
  printf( "     -q query optional flag query parameter that makes\n"          );
  printf( "              xrdfs return error code to the shell if the\n"       );
  printf( "              requested flag combination is not present;\n"        );
  printf( "              flags may be combined together using '|' or '&'\n"   );
  printf( "              Available flags:\n"                                  );
  printf( "              XBitSet, IsDir, Other, Offline, POSCPending,\n"      );
  printf( "              IsReadable, IsWritable\n\n"                          );

  printf( "   statvfs <path>\n"                                               );
  printf( "     Get info about a virtual file system.\n\n"                    );

  printf( "   query <code> <parameters>\n"                                    );
  printf( "     Obtain server information. Query codes:\n\n"                  );

  printf( "     config         <what>   Server configuration; <what> is\n"    );
  printf( "                             one of the following:\n"              );
  printf( "                               bind_max      - the maximum number of parallel streams\n"  );
  printf( "                               chksum        - the supported checksum\n"                  );
  printf( "                               cms           - the status of the cmsd\n"                  );
  printf( "                               pio_max       - maximum number of parallel I/O requests\n" );
  printf( "                               readv_ior_max - maximum size of a readv element\n"         );
  printf( "                               readv_iov_max - maximum number of readv entries\n"         );
  printf( "                               role          - the role in a cluster\n"                   );
  printf( "                               sitename      - the site name\n"                           );
  printf( "                               tpc           - support for third party copies\n"          );
  printf( "                               version       - the version of the server\n"               );
  printf( "                               wan_port      - the port to use for wan copies\n"          );
  printf( "                               wan_window    - the wan_port window size\n"                );
  printf( "                               window        - the tcp window size\n"                     );
  printf( "     checksumcancel <path>   File checksum cancellation\n"       );
  printf( "     checksum       <path>   File checksum\n"                    );
  printf( "     opaque         <arg>    Implementation dependent\n"         );
  printf( "     opaquefile     <arg>    Implementation dependent\n"         );
  printf( "     space          <space>  Logical space stats\n"              );
  printf( "     stats          <what>   Server stats; <what> is a list\n"   );
  printf( "                             of letters indicating information\n");
  printf( "                             to be returned:\n"                  );
  printf( "                               a - all statistics\n"             );
  printf( "                               p - protocol statistics\n"        );
  printf( "                               b - buffer usage statistics\n"    );
  printf( "                               s - scheduling statistics\n"      );
  printf( "                               d - device polling statistics\n"  );
  printf( "                               u - usage statistics\n"           );
  printf( "                               i - server identification\n"      );
  printf( "                               z - synchronized statistics\n"    );
  printf( "                               l - connection statistics\n"      );
  printf( "     xattr          <path>   Extended attributes\n"            );
  printf( "     prepare        [-d|--delete] <reqid> [filenames]  Prepare request status\n" );
  printf( "     tape           [discover | archiveinfo <paths...> | delete <reqid>]\n" );
  printf( "     archiveinfo    <paths...>\n\n" );

  printf( "   rm [-r|-R|--recursive] [--dry-run] [--] <path>...\n"          );
  printf( "     Remove one or more files or directory trees.\n"              );
  printf( "     -r, -R, --recursive remove directories and their contents\n" );
  printf( "                         without following directory symlinks\n"   );
  printf( "                         WebDAV uses collection DELETE directly\n"  );
  printf( "     --dry-run inspect and print the removal plan without changing\n" );
  printf( "               storage; files use SKIP and directories SKIP DIR\n"   );
  printf( "               Plans are bounded to 4096 directory levels.\n"        );
  printf( "               Directory-symlink plans are advisory (no lstat).\n"    );
  printf( "     Recursive root and traversal paths are rejected up front.\n"   );
  printf( "     Later operands continue after a failure; the first error is\n"   );
  printf( "     returned, so multi-path removal is not transactional.\n"        );
  printf( "     -- stop option parsing, allowing a dash-prefixed path\n\n"   );

  printf( "   rmdir <dirname>\n"                                            );
  printf( "     Remove an empty directory.\n\n"                              );

  printf( "   truncate <filename> <length>\n"                               );
  printf( "     Truncate a file.\n\n"                                       );

  printf( "   prepare [-c] [-f] [-s] [-w] [-e] [-p priority] [-a requestid]\n" );
  printf( "           [--pin-lifetime duration] [--metadata json] [--wait] [--timeout seconds] [--] filenames\n" );
  printf( "     Prepare one or more files for access.\n"                    );
  printf( "     -c, --colocate co-locate staged files if possible\n"        );
  printf( "     -f, --fresh refresh file access time even if known\n"       );
  printf( "     -s, --stage stage files from tape to disk\n"                );
  printf( "     -w, --write files will be accessed for modification\n"      );
  printf( "     -p, --priority priority of request (0-3)\n"                 );
  printf( "     -a, --abort, --cancel abort/cancel stage request\n"         );
  printf( "     -e, --evict, --release evict/release file from disk cache\n");
  printf( "     --pin-lifetime duration in seconds or ISO-8601 duration\n"  );
  printf( "     --metadata JSON storage metadata\n"                         );
  printf( "     --wait wait/poll until stage completion\n"                  );
  printf( "     --timeout maximum polling timeout in seconds\n"             );
  printf( "     -- stop option parsing, allowing a dash-prefixed path\n\n"  );

  printf( "   cat [-b|--bytes] [-o local file] [--] files\n"                );
  printf( "     Print contents of one or more files to stdout.\n"           );
  printf( "     -b, --bytes accepted for gfal-cat compatibility; output\n"  );
  printf( "                 is always byte-preserving\n"                    );
  printf( "     -o print to the specified local file\n"                     );
  printf( "     -- stop option parsing, allowing a dash-prefixed path\n\n"  );

  printf( "   tail [-c bytes] [-f] file\n"                                  );
  printf( "     Output last part of files to stdout.\n"                     );
  printf( "     -c num_bytes out last num_bytes\n"                          );
  printf( "     -f           output appended data as file grows\n\n"        );

  printf( "   spaceinfo path\n"                                             );
  printf( "     Get space statistics for given path.\n\n"                   );

  printf( "   sum <path> <checksum type>\n"                                 );
  printf( "     Query a file checksum using the requested algorithm.\n\n"   );

  printf( "   token [-w|--write] [--validity minutes] [--issuer URL]\n"      );
  printf( "         [--] <path> [activity ...]\n"                            );
  printf( "     Request a token for a storage path.\n"                       );
  printf( "     The storage endpoint must use HTTPS or DAVS.\n"              );
  printf( "     -w|--write request write access; otherwise request read\n"    );
  printf( "                access\n"                                        );
  printf( "     --validity token validity in minutes (default: 60)\n"         );
  printf( "     --issuer request through an explicit HTTPS or DAVS issuer;\n" );
  printf( "              omit it to request directly from the storage\n"      );
  printf( "              endpoint\n"                                          );
  printf( "     -- stop option parsing, allowing a dash-prefixed path\n"      );
  printf( "     Optional activities override the read/write defaults.\n\n"   );

  printf( "   xattr <path> [attribute]\n"                                   );
  printf( "     With no attribute, list attributes; with one attribute,\n"   );
  printf( "     get its value. Explicit native forms follow.\n\n"           );
  printf( "   xattr <path> <code> <params> \n"                              );
  printf( "     Operation on extended attributes. Codes:\n\n"               );
  printf( "     set   <attr>          Set extended attribute; <attr> is\n"  );
  printf( "                             string of form name=value\n"        );
  printf( "     get   <name>          Get extended attribute\n"             );
  printf( "     del   <name>          Delete extended attribute\n"          );
  printf( "     list                  List extended attributes\n\n"         );

  return XRootDStatus();
}

//------------------------------------------------------------------------------
// Create the executor object
//------------------------------------------------------------------------------
FSExecutor *CreateExecutor( const URL &url )
{
  Env *env = new Env();
  env->PutString( "CWD", "/" );
  FSExecutor *executor = new FSExecutor( url, env );
  executor->AddCommand( "cache",       DoCache      );
  executor->AddCommand( "cd",          DoCD         );
  executor->AddCommand( "chmod",       DoChMod      );
  executor->AddCommand( "ls",          DoLS         );
  executor->AddCommand( "help",        PrintHelp    );
  executor->AddCommand( "stat",        DoStat       );
  executor->AddCommand( "statvfs",     DoStatVFS    );
  executor->AddCommand( "locate",      DoLocate     );
  executor->AddCommand( "mv",          DoMv         );
  executor->AddCommand( "mkdir",       DoMkDir      );
  executor->AddCommand( "rm",          DoRm         );
  executor->AddCommand( "rmdir",       DoRmDir      );
  executor->AddCommand( "query",       DoQuery      );
  executor->AddCommand( "truncate",    DoTruncate   );
  executor->AddCommand( "prepare",     DoPrepare    );
  executor->AddCommand( "cat",         DoCat        );
  executor->AddCommand( "tail",        DoTail       );
  executor->AddCommand( "spaceinfo",   DoSpaceInfo  );
  executor->AddCommand( "sum",         DoSum        );
  executor->AddCommand( "token",       DoToken      );
  executor->AddCommand( "xattr",       DoXAttr      );
  return executor;
}

//------------------------------------------------------------------------------
// Execute command
//------------------------------------------------------------------------------
int ExecuteCommand( FSExecutor *ex, const FSExecutor::CommandParams &args )
{
  XRootDStatus st = ex->Execute( args );
  if( !st.IsOK() )
    std::cerr << st.ToStr() << std::endl;
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

void rl_bind_key( char, uint16_t )
{
}

uint16_t rl_insert = 0;

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

//------------------------------------------------------------------------
//! parse command line
//!
//! @ result : command parameters
//! @ input  : string containing the command line
//! @ return : true if the command has been completed, false otherwise
//------------------------------------------------------------------------
bool getArguments (std::vector<std::string> & result, const std::string &input)
{
  // the delimiter (space in the case of command line)
  static const char delimiter = ' ';
  // two types of quotes: single and double quotes
  const char singleQuote = '\'', doubleQuote = '\"';
  // if the current character of the command has been
  // quoted 'currentQuote' holds the type of quote,
  // otherwise it holds the null character
  char currentQuote = '\0';

  std::string tmp;
  for (std::string::const_iterator it = input.begin (); it != input.end (); ++it)
  {
    // if we encountered a quote character ...
    if (*it == singleQuote || *it == doubleQuote)
    {
      // if we are not within quoted text ...
      if (!currentQuote)
      {
        currentQuote = *it; // set the type of quote
        continue; // and continue, the quote character itself is not a part of the parameter
      }
      // otherwise if it is the closing quote character ...
      else if (currentQuote == *it)
      {
        currentQuote = '\0'; // reset the current quote type
        continue; // and continue, the quote character itself is not a part of the parameter
      }
    }
    // if we are within quoted text or the character is not a delimiter ...
    if (currentQuote || *it != delimiter)
      {
        // concatenate it
        tmp += *it;
      }
    else
      {
        // otherwise add a parameter and erase the tmp string
        if (!tmp.empty ())
        {
          result.push_back(tmp);
          tmp.erase ();
        }
      }
    }
  // if the there are some remainders of the command add them
  if (!tmp.empty())
  {
    result.push_back(tmp);
  }
  // return true if the quotation has been closed
  return currentQuote == '\0';
}

//------------------------------------------------------------------------------
// Execute interactive
//------------------------------------------------------------------------------
int ExecuteInteractive( const URL &url, bool noCwd = false )
{
  //----------------------------------------------------------------------------
  // Set up the environment
  //----------------------------------------------------------------------------
  std::string historyFile = getenv( "HOME" );
  historyFile += "/.xrdquery.history";
  rl_bind_key( '\t', rl_insert );
  read_history( historyFile.c_str() );
  FSExecutor *ex = CreateExecutor( url );

  if( noCwd )
    ex->GetEnv()->PutInt( "NoCWD", 1 );

  //----------------------------------------------------------------------------
  // Execute the commands
  //----------------------------------------------------------------------------
  std::string cmdline;
  while(1)
  {
    char *linebuf = 0;
    // print new prompt only if the previous line was complete
    // (a line is considered not to be complete if a quote has
    // been opened but it has not been closed)
    linebuf = readline( cmdline.empty() ? BuildPrompt( ex->GetEnv(), url ).c_str() : "> " );
    if( !linebuf || !strncmp( linebuf, "exit", 4 ) || !strncmp( linebuf, "quit", 4 ) )
    {
      std::cout << "Goodbye." << std::endl << std::endl;
      break;
    }
    if( !*linebuf)
    {
      free( linebuf );
      continue;
    }
    std::vector<std::string> args;
    cmdline += linebuf;
    free( linebuf );
    if (getArguments( args, cmdline ))
    {
      XRootDStatus st = ex->Execute( args );
      add_history( cmdline.c_str() );
      cmdline.erase();
      if( !st.IsOK() )
        std::cerr << st.ToStr() << std::endl;
    }
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
int ExecuteCommand( const URL &url, const FSExecutor::CommandParams &args )
{
  FSExecutor *ex = CreateExecutor( url );
  ex->GetEnv()->PutInt( "NoCWD", 1 );
  int st = ExecuteCommand( ex, args );
  delete ex;
  return st;
}

//------------------------------------------------------------------------------
// Start the show
//------------------------------------------------------------------------------
int main( int argc, char **argv )
{
  XrdCl::FSExecutor::CommandParams params;
  enum { NoCwdOption = 256 };
  static const option options[] = {
    { "ipv4",   no_argument,       0, '4' },
    { "ipv6",   no_argument,       0, '6' },
    { "debug",  required_argument, 0, 'd' },
    { "help",   no_argument, 0, 'h' },
    { "no-cwd", no_argument, 0, NoCwdOption },
    { 0, 0, 0, 0 }
  };

  bool noCwd = false;
  int debugLevel = 0;
  std::string networkStack;
  opterr = 0;
  int option = 0;
  while( (option = getopt_long( argc, argv, "+46d:h", options, 0 )) != -1 )
  {
    switch( option )
    {
      case '4':
        if( networkStack == "IPv6" )
        {
          std::cerr << "xrdfs: -4 and -6 are mutually exclusive" << std::endl;
          return 1;
        }
        networkStack = "IPv4";
        break;
      case '6':
        if( networkStack == "IPv4" )
        {
          std::cerr << "xrdfs: -4 and -6 are mutually exclusive" << std::endl;
          return 1;
        }
        networkStack = "IPv6";
        break;
      case 'd':
      {
        char *end = 0;
        long level = std::strtol( optarg, &end, 10 );
        if( !*optarg || *end || level < 0 || level > 3 )
        {
          std::cerr << "xrdfs: invalid debug level '" << optarg
                    << "' (expected 0-3)" << std::endl;
          return 1;
        }
        debugLevel = static_cast<int>( level );
        break;
      }
      case 'h':
        PrintHelp( 0, 0, params );
        return 0;
      case NoCwdOption:
        noCwd = true;
        break;
      default:
        PrintHelp( 0, 0, params );
        return 1;
    }
  }

  if( !networkStack.empty() )
    DefaultEnv::GetEnv()->PutString( "NetworkStack", networkStack );

  if( debugLevel )
  {
    Log *log = DefaultEnv::GetLog();
    if( debugLevel == 1 )
      log->SetLevel( Log::InfoMsg );
    else if( debugLevel == 2 )
      log->SetLevel( Log::DebugMsg );
    else
      log->SetLevel( Log::DumpMsg );
  }

  if( optind == argc )
  {
    PrintHelp( 0, 0, params );
    return 1;
  }

  FSExecutor::CommandParams arguments( argv + optind, argv + argc );
  URL url;
  std::string error;
  URLCommandResult result = NormalizeFSURLCommand( arguments, url, error );
  if( result == InvalidURLCommand )
  {
    std::cerr << "xrdfs: " << error << std::endl;
    return 1;
  }

  if( result == ValidURLCommand )
    return ExecuteCommand( url, arguments );

  url = URL( arguments[0] );
  if( !url.IsValid() )
  {
    PrintHelp( 0, 0, params );
    return 1;
  }

  if( arguments.size() == 1 )
    return ExecuteInteractive( url, noCwd );

  arguments.erase( arguments.begin() );

  // Raw query parameters are implementation-dependent and may themselves look
  // like URLs, so query intentionally remains server-first only.
  if( arguments[0] != "query" )
  {
    URL inferredEndpoint;
    result = NormalizeFSURLCommand( arguments, inferredEndpoint, error );
    if( result == InvalidURLCommand )
    {
      std::cerr << "xrdfs: " << error << std::endl;
      return 1;
    }

    if( result == ValidURLCommand )
    {
      std::cerr << "xrdfs: cannot mix a leading endpoint with full URL "
                   "operands; use either server-first or command-first syntax"
                << std::endl;
      return 1;
    }
  }

  return ExecuteCommand( url, arguments );
}
