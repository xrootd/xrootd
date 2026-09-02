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

#include "XrdCl/XrdClFSCompatibility.hh"
#include "XProtocol/XProtocol.hh"
#include "XrdSys/XrdSysE2T.hh"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace XrdCl
{
  AccessModeFormat ParseAccessMode( Access::Mode       &mode,
                                    const std::string &modeString )
  {
    mode = Access::None;

    bool octal = !modeString.empty();
    unsigned int octalMode = 0;
    for( const char character : modeString )
    {
      if( character < '0' || character > '7' )
      {
        octal = false;
        break;
      }

      const unsigned int digit = character - '0';
      if( octalMode > (0777U - digit) / 8U )
        return AccessModeFormat::Invalid;
      octalMode = octalMode * 8U + digit;
    }

    if( octal )
    {
      mode = static_cast<Access::Mode>( octalMode );
      return AccessModeFormat::Octal;
    }

    // Keep the historical xrdfs interpretation: within each permission
    // triplet, r, w, and x select the corresponding bit regardless of their
    // exact position.
    if( modeString.length() != 9 )
      return AccessModeFormat::Invalid;

    Access::Mode symbolicMode = Access::None;
    for( int i = 0; i < 3; ++i )
    {
      if( modeString[i] == 'r' )
        symbolicMode |= Access::UR;
      else if( modeString[i] == 'w' )
        symbolicMode |= Access::UW;
      else if( modeString[i] == 'x' )
        symbolicMode |= Access::UX;
      else if( modeString[i] != '-' )
        return AccessModeFormat::Invalid;
    }
    for( int i = 3; i < 6; ++i )
    {
      if( modeString[i] == 'r' )
        symbolicMode |= Access::GR;
      else if( modeString[i] == 'w' )
        symbolicMode |= Access::GW;
      else if( modeString[i] == 'x' )
        symbolicMode |= Access::GX;
      else if( modeString[i] != '-' )
        return AccessModeFormat::Invalid;
    }
    for( int i = 6; i < 9; ++i )
    {
      if( modeString[i] == 'r' )
        symbolicMode |= Access::OR;
      else if( modeString[i] == 'w' )
        symbolicMode |= Access::OW;
      else if( modeString[i] == 'x' )
        symbolicMode |= Access::OX;
      else if( modeString[i] != '-' )
        return AccessModeFormat::Invalid;
    }
    mode = symbolicMode;
    return AccessModeFormat::Symbolic;
  }

  bool IsWebDAVProtocol( const std::string &protocol )
  {
    std::string normalized( protocol );
    std::transform( normalized.begin(), normalized.end(), normalized.begin(),
                    []( unsigned char character )
    {
      return static_cast<char>( std::tolower( character ) );
    } );
    return normalized == "http" || normalized == "https" ||
           normalized == "dav" || normalized == "davs";
  }

  bool IsCompleteSuccess( const XRootDStatus &status )
  {
    return status.IsOK() && status.code == suDone;
  }

  NonRecursiveRemovalDecision EvaluateNonRecursiveRemoval(
    NonRecursiveRemoval removal, bool isDirectory, std::size_t childCount )
  {
    if( removal == NonRecursiveRemoval::File )
      return isDirectory ? NonRecursiveRemovalDecision::IsDirectory :
                           NonRecursiveRemovalDecision::Allow;

    if( !isDirectory ) return NonRecursiveRemovalDecision::NotDirectory;
    if( childCount != 0 ) return NonRecursiveRemovalDecision::NotEmpty;
    return NonRecursiveRemovalDecision::Allow;
  }

  std::string FormatGFALXAttrFailure( const std::string  &attribute,
                                      const XRootDStatus &status )
  {
    std::string errorText;
    if( status.errNo == kXR_AttrNotFound )
    {
      // Match the Linux/gfal2 wording on platforms such as macOS, where the
      // native ENOATTR text is "Attribute not found" instead.
      errorText = "No data available";
    }
    else if( status.errNo )
    {
      int errorNumber = status.errNo;
      if( errorNumber >= kXR_ArgInvalid )
        errorNumber = XProtocol::toErrno( errorNumber );
      errorText = XrdSysE2T( errorNumber );
    }
    else
    {
      errorText = status.ToString();
      const std::string::size_type prefixEnd = errorText.find( "] " );
      if( prefixEnd != std::string::npos )
        errorText.erase( 0, prefixEnd + 2 );
    }

    if( !errorText.empty() )
      errorText.front() = static_cast<char>(
        std::toupper( static_cast<unsigned char>( errorText.front() ) ) );

    std::ostringstream message;
    message << "Failed to get the xattr \"" << attribute << "\" ("
            << errorText << ')';
    return message.str();
  }

  const char *GetGFALFileStatus( bool offline, bool backupExists )
  {
    const bool onDisk = !offline;
    if( backupExists && onDisk ) return "ONLINE_AND_NEARLINE";
    if( backupExists ) return "NEARLINE";
    if( onDisk ) return "ONLINE";
    return "UNKNOWN";
  }

  const char *GetGFALTapeFileStatus( const std::string &locality )
  {
    if( locality == "DISK" ) return "ONLINE";
    if( locality == "TAPE" ) return "NEARLINE";
    if( locality == "DISK_AND_TAPE" ) return "ONLINE_AND_NEARLINE";
    return nullptr;
  }
}
