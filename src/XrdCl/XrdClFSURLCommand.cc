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

#include "XrdCl/XrdClFSURLCommand.hh"

#include <algorithm>
#include <cctype>
#include <iterator>

namespace
{
  bool SupportsFullURLs( const std::string &command )
  {
    static const char *commands[] = {
      "cache", "cat", "chmod", "locate", "ls", "mkdir", "mv",
      "prepare", "rm", "rmdir", "spaceinfo", "stat", "statvfs", "sum",
      "tail", "truncate", "xattr"
    };

    return std::find( std::begin( commands ), std::end( commands ), command )
           != std::end( commands );
  }

  bool HasURLSeparator( const std::string &argument )
  {
    return argument.find( "://" ) != std::string::npos;
  }

  bool IsCompleteURL( const std::string &argument )
  {
    const std::size_t separator = argument.find( "://" );
    if( separator == std::string::npos || separator == 0 ||
        std::isalpha( static_cast<unsigned char>( argument[0] ) ) == 0 )
      return false;

    for( std::size_t i = 1; i < separator; ++i )
    {
      const unsigned char character = argument[i];
      if( std::isalnum( character ) == 0 && character != '+' &&
          character != '-' && character != '.' )
        return false;
    }
    return true;
  }

  bool IsOptionValue( const std::vector<std::string> &arguments,
                      std::size_t index, const char *option )
  {
    return index > 1 && arguments[index - 1] == option;
  }

  bool IsPathOperand( const std::vector<std::string> &arguments,
                      std::size_t index )
  {
    const std::string &command = arguments[0];

    if( command == "cache" )
      return index == 2;
    if( command == "chmod" || command == "rmdir" ||
        command == "spaceinfo" || command == "statvfs" || command == "sum" ||
        command == "truncate" || command == "xattr" )
      return index == 1;
    if( command == "mv" )
      return index == 1 || index == 2;
    if( command == "stat" )
      return !IsOptionValue( arguments, index, "-q" );
    if( command == "cat" )
      return !IsOptionValue( arguments, index, "-o" );
    if( command == "ls" )
      return !IsOptionValue( arguments, index, "--color" ) &&
             !IsOptionValue( arguments, index, "--xattr" );
    if( command == "tail" )
      return !IsOptionValue( arguments, index, "-c" );
    if( command == "prepare" )
      return !IsOptionValue( arguments, index, "-a" ) &&
             !IsOptionValue( arguments, index, "-p" );

    // locate, mkdir, and rm have no options with separate values.
    return true;
  }

  std::string LowerCase( std::string value )
  {
    std::transform( value.begin(), value.end(), value.begin(),
                    []( unsigned char character )
    {
      return static_cast<char>( std::tolower( character ) );
    } );
    return value;
  }

  bool SameEndpoint( const XrdCl::URL &left, const XrdCl::URL &right )
  {
    return LowerCase( left.GetProtocol() ) ==
             LowerCase( right.GetProtocol() ) &&
           left.GetUserName() == right.GetUserName() &&
           left.GetPassword() == right.GetPassword() &&
           LowerCase( left.GetHostName() ) ==
             LowerCase( right.GetHostName() ) &&
           left.GetPort() == right.GetPort();
  }

  XrdCl::URL EndpointURL( const XrdCl::URL &url )
  {
    XrdCl::URL endpoint( url );
    endpoint.SetProtocol( LowerCase( endpoint.GetProtocol() ) );
    endpoint.SetHostName( LowerCase( endpoint.GetHostName() ) );
    endpoint.SetPath( "" );
    endpoint.SetParams( XrdCl::URL::ParamsMap() );
    return endpoint;
  }

  std::string OperandPath( const XrdCl::URL &url )
  {
    std::string path = url.GetPathWithParams();
    if( path.empty() || path[0] != '/' )
      path.insert( path.begin(), '/' );
    return path;
  }
}

namespace XrdCl
{
  URLCommandResult NormalizeFSURLCommand(
    std::vector<std::string> &arguments,
    URL                      &endpoint,
    std::string              &error )
  {
    if( arguments.empty() )
      return NotURLCommand;

    if( arguments[0] == "query" )
    {
      for( std::size_t i = 1; i < arguments.size(); ++i )
      {
        if( IsCompleteURL( arguments[i] ) )
        {
          error = "command-first full URLs are not supported for 'query'";
          return InvalidURLCommand;
        }
      }
      return NotURLCommand;
    }

    if( !SupportsFullURLs( arguments[0] ) )
      return NotURLCommand;

    bool foundURL = false;
    URL normalizedEndpoint;
    std::vector<std::string> normalizedArguments( arguments );
    for( std::size_t i = 1; i < arguments.size(); ++i )
    {
      if( !IsPathOperand( arguments, i ) ||
          !HasURLSeparator( arguments[i] ) )
        continue;

      if( !IsCompleteURL( arguments[i] ) )
      {
        error = "invalid remote URL operand";
        return InvalidURLCommand;
      }

      URL operand( arguments[i] );
      const std::string protocol = LowerCase( operand.GetProtocol() );
      if( !operand.IsValid() || operand.GetProtocol().empty() ||
          operand.GetHostName().empty() || protocol == "file" ||
          protocol == "stdio" )
      {
        error = "invalid remote URL operand";
        return InvalidURLCommand;
      }

      const URL operandEndpoint = EndpointURL( operand );
      if( !foundURL )
      {
        normalizedEndpoint = operandEndpoint;
        foundURL = true;
      }
      else if( !SameEndpoint( normalizedEndpoint, operandEndpoint ) )
      {
        error = "all URL operands must use the same endpoint";
        return InvalidURLCommand;
      }

      normalizedArguments[i] = OperandPath( operand );
    }

    if( !foundURL )
      return NotURLCommand;

    endpoint = normalizedEndpoint;
    arguments.swap( normalizedArguments );
    return ValidURLCommand;
  }
}
