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

#ifndef __XRD_CL_FS_COMPATIBILITY_HH__
#define __XRD_CL_FS_COMPATIBILITY_HH__

#include "XrdCl/XrdClFileSystem.hh"

#include <cstddef>
#include <string>

namespace XrdCl
{
  enum class AccessModeFormat
  {
    Invalid,
    Symbolic,
    Octal
  };

  enum class NonRecursiveRemoval
  {
    File,
    Directory
  };

  enum class NonRecursiveRemovalDecision
  {
    Allow,
    IsDirectory,
    NotDirectory,
    NotEmpty
  };

  AccessModeFormat ParseAccessMode( Access::Mode       &mode,
                                    const std::string &modeString );

  bool IsWebDAVProtocol( const std::string &protocol );

  bool IsCompleteSuccess( const XRootDStatus &status );

  NonRecursiveRemovalDecision EvaluateNonRecursiveRemoval(
    NonRecursiveRemoval removal, bool isDirectory, std::size_t childCount );

  const char *GetGFALFileStatus( bool offline, bool backupExists );
}

#endif // __XRD_CL_FS_COMPATIBILITY_HH__
