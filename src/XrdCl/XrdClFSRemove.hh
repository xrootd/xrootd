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

#ifndef __XRD_CL_FS_REMOVE_HH__
#define __XRD_CL_FS_REMOVE_HH__

#include "XrdCl/XrdClXRootDResponses.hh"

#include <functional>
#include <string>
#include <vector>

namespace XrdCl
{
  struct RemoveCommand
  {
    bool recursive = false;
    bool dryRun = false;
    bool force = false;
    std::vector<std::string> paths;
  };

  bool ParseRemoveCommand( const std::vector<std::string> &arguments,
                           RemoveCommand                  &command,
                           std::string                    &error );

  bool ValidateRecursiveRemovePath( const std::string &path,
                                    std::string       &error );

  bool IsRecursiveRemovalDirectoryStatus( const XRootDStatus &status,
                                          bool nativeXRootD );

  std::string RecursiveRemovalChildPath( const std::string &parent,
                                         const std::string &child );

  std::string RemovalDisplayPath( const std::string &path );

  XRootDStatus SanitizeRemovalStatus( const XRootDStatus &status );

  struct DryRunRemoveOperations
  {
    bool force = false;
    std::function<XRootDStatus( const std::string &, bool & )> stat;
    std::function<XRootDStatus( const std::string &,
                                std::vector<std::string> & )> list;
    std::function<void( const std::string &, bool )> report;
    std::function<void( const std::string &,
                        const XRootDStatus & )> reportFailure;
  };

  XRootDStatus PlanRemoval(
    const std::vector<std::string> &paths,
    bool recursive,
    const DryRunRemoveOperations &operations,
    std::string &failedPath );

  struct RecursiveRemoveOperations
  {
    bool nativeXRootD = false;
    bool force = false;
    std::function<XRootDStatus( const std::string & )> remove;
    std::function<XRootDStatus( const std::string &,
                                std::vector<std::string> & )> list;
    std::function<XRootDStatus( const std::string & )> removeDirectory;
  };

  XRootDStatus RemoveRecursively(
    const std::vector<std::string> &paths,
    const RecursiveRemoveOperations &operations,
    std::string &failedPath );
}

#endif // __XRD_CL_FS_REMOVE_HH__
