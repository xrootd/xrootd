/******************************************************************************/
/*                                                                            */
/*                         X r d C l i . c c                                  */
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

#include "XrdApps/XrdCliCommandLine.hh"
#include "XrdVersion.hh"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>

namespace
{
  void PrintTopLevelHelp()
  {
    std::cout <<
      "Usage: xrd <command> [options] arguments\n\n"
      "Thin migration frontend for native XRootD commands. The name 'xrd'\n"
      "is provisional while the command and packaging design are reviewed.\n\n"
      "Commands:\n"
      "  stat URL             Delegate metadata lookup to xrdfs\n"
      "  ls [options] URL     Delegate directory listing to xrdfs\n"
      "  cat [options] URL... Delegate same-endpoint file streaming to xrdfs\n"
      "  xattr URL [NAME]    Delegate read-only xattr list/get to xrdfs\n"
      "  copy SRC DST         Delegate a transfer to xrdcp\n"
      "  cp SRC DST           Alias for copy\n\n"
      "Run 'xrd <command> --help' for supported compatibility options.\n";
  }

  void PrintCommonOptions()
  {
    std::cout <<
      "Common options:\n"
      "  -h, --help              Show command help\n"
      "  -V, --version           Show the XRootD version\n"
      "  -v, --verbose           Increase client logs (Warning/Info/Debug)\n"
      "  -t, --timeout SECONDS   Set XRD_REQUESTTIMEOUT\n"
      "  -E, --cert FILE         Set the X.509 client certificate\n"
      "      --key FILE          Set the X.509 client key used with --cert\n"
      "  -4 / -6                 Select the IPv4 or IPv6 network stack\n"
      "      --log-file FILE     Set XRD_LOGFILE\n"
      "  -D, --definition VALUE  Accepted with a warning; not translated\n"
      "  -C, --client-info INFO  Accepted with a warning; not translated\n";
  }

  void PrintCommandHelp( const std::string &command )
  {
    if( command == "stat" )
      std::cout << "Usage: xrd stat [common-options] URL\n\n";
    else if( command == "cat" )
      std::cout <<
        "Usage: xrd cat [common-options] [-b|--bytes] URL...\n\n"
        "The bytes flag is accepted as a no-op because xrdfs cat already\n"
        "streams raw bytes. All URLs must use the same endpoint.\n\n";
    else if( command == "ls" )
      std::cout <<
        "Usage: xrd ls [common-options] [-a] [-l] [-H] URL\n\n"
        "  -a, --all             Accepted as a no-op; xrdfs does not filter\n"
        "                        dot-prefixed entries\n"
        "  -l, --long            Request the native xrdfs long listing\n"
        "  -H, --human-readable  Request human-readable sizes\n"
        "      --color auto|never Accepted as a no-op\n\n";
    else if( command == "xattr" )
      std::cout <<
        "Usage: xrd xattr [common-options] URL [ATTRIBUTE]\n\n"
        "Without an attribute, list attributes through xrdfs. With an\n"
        "attribute name, retrieve it. ATTRIBUTE=VALUE is rejected because\n"
        "this initial command is deliberately read-only.\n\n";
    else if( command == "copy" )
      std::cout <<
        "Usage: xrd copy [common-options] [copy-options] SRC DST\n"
        "       xrd copy [common-options] --from-file FILE DST\n\n"
        "  -f, --force            Replace an existing destination\n"
        "  -p, --parent           Create missing destination directories\n"
        "  -r, --recursive        Copy directories recursively\n"
        "  -K, --checksum VALUE   Translate to xrdcp --cksum\n"
        "  -n, --nbstreams N      Translate to xrdcp --streams\n"
        "  -T, --transfer-timeout SECONDS\n"
        "                         Set XRD_CPTIMEOUT\n"
        "      --from-file FILE   Translate to xrdcp --infiles\n\n";
    PrintCommonOptions();
  }

  int Execute( const XrdApps::CommandInvocation &invocation )
  {
    for( const std::string &name : invocation.environmentToUnset )
    {
      if( unsetenv( name.c_str() ) != 0 )
      {
        std::cerr << "xrd: unable to unset " << name << ": "
                  << std::strerror( errno ) << '\n';
        return 126;
      }
    }

    for( const auto &entry : invocation.environment )
    {
      if( setenv( entry.first.c_str(), entry.second.c_str(), 1 ) != 0 )
      {
        std::cerr << "xrd: unable to set " << entry.first << ": "
                  << std::strerror( errno ) << '\n';
        return 126;
      }
    }

    for( const std::string &warning : invocation.warnings )
      std::cerr << "xrd: warning: " << warning << '\n';

    std::vector<char *> arguments;
    arguments.reserve( invocation.arguments.size() + 2 );
    arguments.push_back( const_cast<char *>( invocation.executable.c_str() ) );
    for( const std::string &argument : invocation.arguments )
      arguments.push_back( const_cast<char *>( argument.c_str() ) );
    arguments.push_back( nullptr );

    execvp( invocation.executable.c_str(), arguments.data() );
    const int error = errno;
    std::cerr << "xrd: unable to execute " << invocation.executable << ": "
              << std::strerror( error ) << '\n';
    return error == ENOENT ? 127 : 126;
  }
}

int main( int argc, char **argv )
{
  const std::vector<std::string> arguments( argv + 1, argv + argc );
  const XrdApps::CommandLineResult result = XrdApps::ParseCommandLine( arguments );

  switch( result.action )
  {
    case XrdApps::CommandLineAction::Execute:
      return Execute( result.invocation );
    case XrdApps::CommandLineAction::Help:
      if( result.command.empty() )
        PrintTopLevelHelp();
      else
        PrintCommandHelp( result.command );
      return 0;
    case XrdApps::CommandLineAction::Version:
      std::cout << "xrd " << XrdVERSION << '\n';
      return 0;
    case XrdApps::CommandLineAction::Error:
      std::cerr << "xrd: " << result.error << '\n';
      if( !result.command.empty() )
        std::cerr << "Try 'xrd " << result.command << " --help' for more information.\n";
      else
        std::cerr << "Try 'xrd --help' for more information.\n";
      return 2;
  }
  return 2;
}
