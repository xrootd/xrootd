//------------------------------------------------------------------------------
// Copyright (c) 2026 by European Organization for Nuclear Research (CERN)
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClFSRemove.hh"

#include "XProtocol/XProtocol.hh"
#include "XrdOuc/XrdOucPrivateUtils.hh"

#include <cctype>
#include <cstddef>
#include <utility>

namespace
{
  // Keep metadata planning finite when a remote endpoint repeatedly presents
  // a child as another directory. The destructive walker is deliberately not
  // affected by this dry-run-only safety limit.
  constexpr std::size_t kMaximumRemovalPlanDepth = 4096;

  int HexDigit( char character )
  {
    const unsigned char value = static_cast<unsigned char>( character );
    if( std::isdigit( value ) ) return character - '0';
    if( character >= 'a' && character <= 'f' ) return character - 'a' + 10;
    if( character >= 'A' && character <= 'F' ) return character - 'A' + 10;
    return -1;
  }

  std::string DecodePercentEscapes( const std::string &path )
  {
    std::string decoded;
    decoded.reserve( path.size() );
    for( std::size_t i = 0; i < path.size(); ++i )
    {
      if( path[i] == '%' && i + 2 < path.size() )
      {
        const int high = HexDigit( path[i + 1] );
        const int low = HexDigit( path[i + 2] );
        if( high >= 0 && low >= 0 )
        {
          decoded.push_back( static_cast<char>( high * 16 + low ) );
          i += 2;
          continue;
        }
      }
      decoded.push_back( path[i] );
    }
    return decoded;
  }

  bool HasDotComponent( const std::string &path )
  {
    std::size_t begin = 0;
    while( begin <= path.size() )
    {
      const std::size_t end = path.find( '/', begin );
      const std::string component = path.substr(
        begin, end == std::string::npos ? std::string::npos : end - begin );
      if( component == "." || component == ".." ) return true;
      if( end == std::string::npos ) break;
      begin = end + 1;
    }
    return false;
  }

  bool IsRootPath( const std::string &path )
  {
    if( path.empty() ) return true;
    for( const char character : path )
      if( character != '/' ) return false;
    return true;
  }

  bool ValidateChildName( const std::string &child, std::string &error )
  {
    const std::string decoded = DecodePercentEscapes( child );
    if( child.empty() || child == "." || child == ".." ||
        child.find( '/' ) != std::string::npos ||
        child.find( '?' ) != std::string::npos ||
        decoded.empty() || decoded == "." || decoded == ".." ||
        decoded.find( '/' ) != std::string::npos ||
        decoded.find( '\0' ) != std::string::npos )
    {
      error = "directory listing returned an unsafe child name";
      return false;
    }
    return true;
  }

  XrdCl::XRootDStatus InvalidRemovePathStatus( const std::string &message )
  {
    return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInvalidArgs, 0,
                                message );
  }

  XrdCl::XRootDStatus InvalidListingStatus( const std::string &message )
  {
    return XrdCl::XRootDStatus( XrdCl::stError,
                                XrdCl::errInvalidResponse, 0, message );
  }

  XrdCl::XRootDStatus IncompleteOperationStatus( const char *operation )
  {
    return InvalidListingStatus(
      std::string( operation ) + " did not return a complete response" );
  }

  XrdCl::XRootDStatus RemovalPlanDepthStatus()
  {
    return InvalidListingStatus(
      "dry-run removal plan exceeded the maximum directory depth of 4096" );
  }
}

namespace XrdCl
{
  bool ParseRemoveCommand( const std::vector<std::string> &arguments,
                           RemoveCommand                  &command,
                           std::string                    &error )
  {
    command = RemoveCommand();
    error.clear();

    bool parseOptions = true;
    for( std::size_t i = 1; i < arguments.size(); ++i )
    {
      const std::string &argument = arguments[i];
      if( parseOptions && argument == "--" )
      {
        parseOptions = false;
        continue;
      }
      if( parseOptions && argument == "--recursive" )
      {
        command.recursive = true;
        continue;
      }
      if( parseOptions && argument == "--force" )
      {
        command.force = true;
        continue;
      }
      if( parseOptions && argument == "--dry-run" )
      {
        command.dryRun = true;
        continue;
      }
      if( parseOptions && argument.size() > 1 && argument[0] == '-' &&
          argument[1] != '-' )
      {
        for( std::size_t j = 1; j < argument.size(); ++j )
        {
          const char opt = argument[j];
          if( opt == 'r' || opt == 'R' )
            command.recursive = true;
          else if( opt == 'f' )
            command.force = true;
          else
          {
            error = "unknown rm option: -" + std::string( 1, opt );
            return false;
          }
        }
        continue;
      }
      if( parseOptions && argument.size() > 1 && argument[0] == '-' )
      {
        error = "unknown rm option: " + argument;
        return false;
      }
      command.paths.push_back( argument );
    }

    if( command.paths.empty() )
    {
      error = "rm requires at least one path";
      return false;
    }
    return true;
  }

  bool ValidateRecursiveRemovePath( const std::string &path,
                                    std::string       &error )
  {
    error.clear();
    const std::size_t query = path.find( '?' );
    const std::string pathPart = path.substr( 0, query );
    const std::string decoded = DecodePercentEscapes( pathPart );

    if( IsRootPath( pathPart ) || IsRootPath( decoded ) )
    {
      error = "recursive removal of the namespace root is not allowed";
      return false;
    }
    if( HasDotComponent( pathPart ) || HasDotComponent( decoded ) )
    {
      error = "recursive removal paths must not contain '.' or '..' components";
      return false;
    }
    if( decoded.find( '\0' ) != std::string::npos )
    {
      error = "recursive removal paths must not contain an encoded NUL byte";
      return false;
    }
    return true;
  }

  bool IsRecursiveRemovalDirectoryStatus( const XRootDStatus &status,
                                          bool nativeXRootD )
  {
    return nativeXRootD && !status.IsOK() &&
           status.code == errErrorResponse &&
           (status.errNo == kXR_isDirectory ||
            status.errNo == kXR_ItExists);
  }

  std::string RecursiveRemovalChildPath( const std::string &parent,
                                         const std::string &child )
  {
    const std::size_t query = parent.find( '?' );
    std::string path = parent.substr( 0, query );
    const std::string parameters = query == std::string::npos ?
      std::string() : parent.substr( query );

    while( path.size() > 1 && path.back() == '/' ) path.pop_back();
    path += '/';
    path += child;
    path += parameters;
    return path;
  }

  std::string RemovalDisplayPath( const std::string &path )
  {
    return obfuscateAuth( path );
  }

  XRootDStatus SanitizeRemovalStatus( const XRootDStatus &status )
  {
    XRootDStatus sanitized( status.status, status.code, status.errNo,
                            status.GetErrorMessage() );
    sanitized.SetErrorMessage(
      RemovalDisplayPath( sanitized.GetErrorMessage() ) );
    return sanitized;
  }

  XRootDStatus PlanRemoval(
    const std::vector<std::string> &paths,
    bool recursive,
    const DryRunRemoveOperations &operations,
    std::string &failedPath )
  {
    failedPath.clear();
    std::string error;
    if( recursive )
    {
      for( const std::string &path : paths )
      {
        if( !ValidateRecursiveRemovePath( path, error ) )
        {
          failedPath = path;
          return InvalidRemovePathStatus( error );
        }
      }
    }

    struct StackEntry
    {
      std::string path;
      bool postorder;
      std::size_t depth;
    };

    bool haveFailure = false;
    XRootDStatus firstFailure;
    for( const std::string &root : paths )
    {
      bool rootFailed = false;
      std::vector<StackEntry> stack;
      stack.push_back( {root, false, 0} );
      while( !stack.empty() )
      {
        StackEntry &entry = stack.back();
        if( entry.postorder )
        {
          operations.report( entry.path, true );
          stack.pop_back();
          continue;
        }

        if( entry.depth > kMaximumRemovalPlanDepth )
        {
          const XRootDStatus failure = SanitizeRemovalStatus(
            RemovalPlanDepthStatus() );
          operations.reportFailure( entry.path, failure );
          if( !haveFailure )
          {
            failedPath = entry.path;
            firstFailure = failure;
            haveFailure = true;
          }
          rootFailed = true;
          break;
        }

        bool isDirectory = false;
        XRootDStatus status = operations.stat( entry.path, isDirectory );
        if( !status.IsOK() || status.code != suDone )
        {
          const XRootDStatus failure = SanitizeRemovalStatus(
            status.IsOK() ? IncompleteOperationStatus(
                              "metadata inspection" ) : status );
          operations.reportFailure( entry.path, failure );
          if( !(operations.force && status.errNo == kXR_NotFound) &&
              !haveFailure )
          {
            failedPath = entry.path;
            firstFailure = failure;
            haveFailure = true;
          }
          rootFailed = true;
          break;
        }

        if( !isDirectory )
        {
          operations.report( entry.path, false );
          stack.pop_back();
          continue;
        }

        if( !recursive )
        {
          const XRootDStatus failure = SanitizeRemovalStatus( XRootDStatus(
            stError, errErrorResponse, kXR_isDirectory,
            "Target is a directory; recursive removal was not requested." ) );
          operations.reportFailure( entry.path, failure );
          if( !haveFailure )
          {
            failedPath = entry.path;
            firstFailure = failure;
            haveFailure = true;
          }
          rootFailed = true;
          break;
        }

        std::vector<std::string> children;
        status = operations.list( entry.path, children );
        if( !status.IsOK() || status.code != suDone )
        {
          const XRootDStatus failure = SanitizeRemovalStatus(
            status.IsOK() ? IncompleteOperationStatus(
                              "directory listing" ) : status );
          operations.reportFailure( entry.path, failure );
          if( !haveFailure )
          {
            failedPath = entry.path;
            firstFailure = failure;
            haveFailure = true;
          }
          rootFailed = true;
          break;
        }

        std::vector<std::string> childPaths;
        childPaths.reserve( children.size() );
        for( const std::string &child : children )
        {
          if( child == "." || child == ".." ) continue;
          if( !ValidateChildName( child, error ) )
          {
            const XRootDStatus failure = SanitizeRemovalStatus(
              InvalidListingStatus( error ) );
            operations.reportFailure( entry.path, failure );
            if( !haveFailure )
            {
              failedPath = entry.path;
              firstFailure = failure;
              haveFailure = true;
            }
            rootFailed = true;
            break;
          }
          std::string childPath = RecursiveRemovalChildPath( entry.path, child );
          if( !ValidateRecursiveRemovePath( childPath, error ) )
          {
            const XRootDStatus failure = SanitizeRemovalStatus(
              InvalidListingStatus( error ) );
            operations.reportFailure( childPath, failure );
            if( !haveFailure )
            {
              failedPath = childPath;
              firstFailure = failure;
              haveFailure = true;
            }
            rootFailed = true;
            break;
          }
          childPaths.push_back( std::move( childPath ) );
        }

        if( rootFailed ) break;

        const std::size_t childDepth = entry.depth + 1;
        entry.postorder = true;
        for( auto child = childPaths.rbegin(); child != childPaths.rend();
             ++child )
          stack.push_back( {*child, false, childDepth} );
      }
    }
    return haveFailure ? firstFailure : XRootDStatus();
  }

  XRootDStatus RemoveRecursively(
    const std::vector<std::string> &paths,
    const RecursiveRemoveOperations &operations,
    std::string &failedPath )
  {
    failedPath.clear();
    std::string error;
    for( const std::string &path : paths )
    {
      if( !ValidateRecursiveRemovePath( path, error ) )
      {
        failedPath = path;
        return InvalidRemovePathStatus( error );
      }
    }

    struct StackEntry
    {
      std::string path;
      bool postorder;
    };

    bool haveFailure = false;
    XRootDStatus firstFailure;
    for( const std::string &root : paths )
    {
      bool rootFailed = false;
      std::vector<StackEntry> stack;
      stack.push_back( {root, false} );
      while( !stack.empty() )
      {
        StackEntry &entry = stack.back();
        if( entry.postorder )
        {
          XRootDStatus status = operations.removeDirectory( entry.path );
          if( !status.IsOK() || status.code != suDone )
          {
            if( operations.force && status.errNo == kXR_NotFound )
            {
              stack.pop_back();
              continue;
            }
            if( !haveFailure )
            {
              failedPath = entry.path;
              firstFailure = status.IsOK() ?
                IncompleteOperationStatus( "directory removal" ) : status;
              haveFailure = true;
            }
            rootFailed = true;
            break;
          }
          stack.pop_back();
          continue;
        }

        XRootDStatus status = operations.remove( entry.path );
        if( status.IsOK() && status.code == suDone )
        {
          stack.pop_back();
          continue;
        }
        if( status.IsOK() )
        {
          if( !haveFailure )
          {
            failedPath = entry.path;
            firstFailure = IncompleteOperationStatus( "removal" );
            haveFailure = true;
          }
          rootFailed = true;
          break;
        }
        if( operations.force && status.errNo == kXR_NotFound )
        {
          stack.pop_back();
          continue;
        }
        if( !IsRecursiveRemovalDirectoryStatus(
              status, operations.nativeXRootD ) )
        {
          if( !haveFailure )
          {
            failedPath = entry.path;
            firstFailure = status;
            haveFailure = true;
          }
          rootFailed = true;
          break;
        }

        if( status.errNo == kXR_isDirectory )
        {
          // On Linux an absolute symlink to a directory can make native Rm
          // report EISDIR. RmDir is a non-following type probe: it removes an
          // empty real directory, reports ENOTEMPTY for a nonempty real
          // directory, and fails for the symlink itself. Listing is safe only
          // after that explicit nonempty-directory response.
          status = operations.removeDirectory( entry.path );
          if( status.IsOK() && status.code == suDone )
          {
            stack.pop_back();
            continue;
          }
          if( operations.force && status.errNo == kXR_NotFound )
          {
            stack.pop_back();
            continue;
          }
          if( status.IsOK() || status.code != errErrorResponse ||
              status.errNo != kXR_ItExists )
          {
            if( !haveFailure )
            {
              failedPath = entry.path;
              firstFailure = status.IsOK() ?
                IncompleteOperationStatus( "directory removal probe" ) :
                status;
              haveFailure = true;
            }
            rootFailed = true;
            break;
          }
        }

        std::vector<std::string> children;
        status = operations.list( entry.path, children );
        if( !status.IsOK() )
        {
          if( !haveFailure )
          {
            failedPath = entry.path;
            firstFailure = status;
            haveFailure = true;
          }
          rootFailed = true;
          break;
        }
        if( status.code != suDone )
        {
          if( !haveFailure )
          {
            failedPath = entry.path;
            firstFailure = IncompleteOperationStatus( "directory listing" );
            haveFailure = true;
          }
          rootFailed = true;
          break;
        }

        std::vector<std::string> childPaths;
        childPaths.reserve( children.size() );
        for( const std::string &child : children )
        {
          // Some directory backends expose POSIX pseudo-entries. They are not
          // children and must never be traversed.
          if( child == "." || child == ".." ) continue;
          if( !ValidateChildName( child, error ) )
          {
            if( !haveFailure )
            {
              failedPath = entry.path;
              firstFailure = InvalidListingStatus( error );
              haveFailure = true;
            }
            rootFailed = true;
            break;
          }
          std::string childPath = RecursiveRemovalChildPath( entry.path, child );
          if( !ValidateRecursiveRemovePath( childPath, error ) )
          {
            if( !haveFailure )
            {
              failedPath = childPath;
              firstFailure = InvalidListingStatus( error );
              haveFailure = true;
            }
            rootFailed = true;
            break;
          }
          childPaths.push_back( std::move( childPath ) );
        }

        if( rootFailed ) break;

        entry.postorder = true;
        for( auto child = childPaths.rbegin(); child != childPaths.rend();
             ++child )
          stack.push_back( {*child, false} );
      }
    }
    return haveFailure ? firstFailure : XRootDStatus();
  }
}
