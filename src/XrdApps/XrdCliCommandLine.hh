/******************************************************************************/
/*                                                                            */
/*                 X r d C l i C o m m a n d L i n e . h h                  */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/******************************************************************************/

#ifndef __XRD_APPS_CLI_COMMAND_LINE_HH__
#define __XRD_APPS_CLI_COMMAND_LINE_HH__

#include <string>
#include <utility>
#include <vector>

namespace XrdApps
{
  enum class CommandLineAction
  {
    Execute,
    Help,
    Version,
    Error
  };

  struct CommandInvocation
  {
    std::string executable;
    std::vector<std::string> arguments;
    std::vector<std::pair<std::string, std::string>> environment;
    std::vector<std::string> environmentToUnset;
    std::vector<std::string> warnings;
  };

  struct CommandLineResult
  {
    CommandLineAction action = CommandLineAction::Error;
    std::string command;
    CommandInvocation invocation;
    std::string error;
  };

  CommandLineResult ParseCommandLine( const std::vector<std::string> &arguments );
}

#endif // __XRD_APPS_CLI_COMMAND_LINE_HH__
