/******************************************************************************/
/*                                                                            */
/*                 X r d C l i C o m m a n d L i n e . c c                  */
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

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <limits>
#include <string_view>

namespace
{
  enum class OptionAction
  {
    Help,
    Version,
    Verbose,
    Timeout,
    Certificate,
    Key,
    IPv4,
    IPv6,
    LogFile,
    Forward,
    Ignore,
    Warn,
    Reject,
    CopyTimeout,
    CopyFromFile,
    CopyStreams,
    CopyChecksum,
    Color
  };

  struct OptionSpec
  {
    char shortName;
    const char *longName;
    bool takesValue;
    OptionAction action;
    const char *value;
  };

  constexpr OptionSpec CommonOptions[] = {
    {'h', "help",        false, OptionAction::Help,        nullptr},
    {'V', "version",     false, OptionAction::Version,     nullptr},
    {'v', "verbose",     false, OptionAction::Verbose,     nullptr},
    {'D', "definition",  true,  OptionAction::Warn,
      "--definition has no XRootD equivalent and was ignored"},
    {'t', "timeout",     true,  OptionAction::Timeout,     nullptr},
    {'E', "cert",        true,  OptionAction::Certificate, nullptr},
    {0,   "key",         true,  OptionAction::Key,         nullptr},
    {'4', nullptr,       false, OptionAction::IPv4,        nullptr},
    {'6', nullptr,       false, OptionAction::IPv6,        nullptr},
    {'C', "client-info", true,  OptionAction::Warn,
      "--client-info is not exposed by the delegated commands and was ignored"},
    {0,   "log-file",    true,  OptionAction::LogFile,     nullptr},
  };

  constexpr OptionSpec CatOptions[] = {
    {'b', "bytes", false, OptionAction::Ignore, nullptr},
  };

  constexpr OptionSpec LsOptions[] = {
    {'a', "all",            false, OptionAction::Ignore,  nullptr},
    {'l', "long",           false, OptionAction::Forward, "-l"},
    {'H', "human-readable", false, OptionAction::Forward, "-h"},
    {'d', "directory",      false, OptionAction::Reject,
      "--directory requires native xrdfs output support"},
    {0,   "xattr",          true,  OptionAction::Reject,
      "--xattr requires native xrdfs output support"},
    {0,   "time-style",     true,  OptionAction::Reject,
      "--time-style requires native xrdfs output support"},
    {0,   "full-time",      false, OptionAction::Reject,
      "--full-time requires native xrdfs output support"},
    {0,   "color",          true,  OptionAction::Color,   nullptr},
  };

  constexpr OptionSpec CopyOptions[] = {
    {'f', "force",           false, OptionAction::Forward,      "--force"},
    {'p', "parent",          false, OptionAction::Forward,      "--path"},
    {'n', "nbstreams",       true,  OptionAction::CopyStreams,  nullptr},
    {0,   "tcp-buffersize",  true,  OptionAction::Reject,
      "--tcp-buffersize has no xrdcp equivalent"},
    {'s', "src-spacetoken",  true,  OptionAction::Reject,
      "--src-spacetoken is not supported by XRootD protocols"},
    {'S', "dst-spacetoken",  true,  OptionAction::Reject,
      "--dst-spacetoken is not supported by XRootD protocols"},
    {'T', "transfer-timeout", true, OptionAction::CopyTimeout,  nullptr},
    {'K', "checksum",        true,  OptionAction::CopyChecksum, nullptr},
    {0,   "checksum-mode",   true,  OptionAction::Reject,
      "--checksum-mode requires a separate native compatibility design"},
    {0,   "from-file",       true,  OptionAction::CopyFromFile, nullptr},
    {0,   "copy-mode",       true,  OptionAction::Reject,
      "--copy-mode cannot be translated faithfully to xrdcp"},
    {0,   "just-copy",       false, OptionAction::Reject,
      "--just-copy cannot be translated faithfully to xrdcp"},
    {0,   "disable-cleanup", false, OptionAction::Reject,
      "--disable-cleanup has no xrdcp equivalent"},
    {0,   "no-delegation",   false, OptionAction::Reject,
      "--no-delegation requires an explicit third-party-copy mode"},
    {0,   "evict",           false, OptionAction::Reject,
      "--evict is not yet exposed through this compatibility command"},
    {0,   "scitag",          true,  OptionAction::Reject,
      "--scitag is not yet exposed through this compatibility command"},
    {'r', "recursive",       false, OptionAction::Forward,      "--recursive"},
    {0,   "abort-on-failure", false, OptionAction::Reject,
      "--abort-on-failure is not needed for a single xrdcp job"},
    {0,   "dry-run",         false, OptionAction::Reject,
      "--dry-run is not supported by xrdcp"},
  };

  struct ParseState
  {
    unsigned int verbosity = 0;
    bool fromFile = false;
    std::string networkStack;
    bool certificateSpecified = false;
    bool keySpecified = false;
    std::string certificate;
    std::string key;
  };

  struct TranslatedOptions
  {
    std::vector<std::string> arguments;
    std::vector<std::string> flags;
    std::vector<std::pair<std::string, std::size_t>> valueIndexes;
  };

  template<std::size_t Size>
  const OptionSpec *FindLongOption( std::string_view name,
                                    const OptionSpec (&options)[Size] )
  {
    for( const OptionSpec &option : options )
    {
      if( option.longName && name == option.longName )
        return &option;
    }
    return nullptr;
  }

  template<std::size_t Size>
  const OptionSpec *FindShortOption( char name,
                                     const OptionSpec (&options)[Size] )
  {
    for( const OptionSpec &option : options )
    {
      if( option.shortName == name )
        return &option;
    }
    return nullptr;
  }

  template<std::size_t Size>
  const OptionSpec *FindLongOption( std::string_view name,
                                    const OptionSpec (&commandOptions)[Size],
                                    bool &isCommon )
  {
    if( const OptionSpec *option = FindLongOption( name, CommonOptions ) )
    {
      isCommon = true;
      return option;
    }
    isCommon = false;
    return FindLongOption( name, commandOptions );
  }

  template<std::size_t Size>
  const OptionSpec *FindShortOption( char name,
                                     const OptionSpec (&commandOptions)[Size],
                                     bool &isCommon )
  {
    if( const OptionSpec *option = FindShortOption( name, CommonOptions ) )
    {
      isCommon = true;
      return option;
    }
    isCommon = false;
    return FindShortOption( name, commandOptions );
  }

  bool NormalizeInteger( const std::string &value, std::string &normalized,
                         unsigned int minimum = 1,
                         unsigned int maximum =
                           std::numeric_limits<int>::max() )
  {
    if( value.empty() )
      return false;

    unsigned int parsed = 0;
    const std::size_t firstDigit = value[0] == '+' ? 1 : 0;
    if( firstDigit == value.size() )
      return false;
    for( std::size_t index = firstDigit; index < value.size(); ++index )
    {
      const unsigned char character = value[index];
      if( std::isdigit( character ) == 0 )
        return false;
      const unsigned int digit = character - '0';
      if( parsed > (maximum - digit) / 10 )
        return false;
      parsed = parsed * 10 + digit;
    }

    if( parsed < minimum )
      return false;
    normalized = std::to_string( parsed );
    return true;
  }

  void SetEnvironment( XrdApps::CommandInvocation &invocation,
                       const std::string &name, const std::string &value )
  {
    for( auto &entry : invocation.environment )
    {
      if( entry.first == name )
      {
        entry.second = value;
        return;
      }
    }
    invocation.environment.emplace_back( name, value );
  }

  void RemoveEnvironment( XrdApps::CommandInvocation &invocation,
                          const std::string &name )
  {
    invocation.environment.erase(
      std::remove_if( invocation.environment.begin(),
                      invocation.environment.end(),
                      [&name]( const auto &entry ) { return entry.first == name; } ),
      invocation.environment.end() );
  }

  void UnsetEnvironment( XrdApps::CommandInvocation &invocation,
                         const std::string &name )
  {
    if( std::find( invocation.environmentToUnset.begin(),
                   invocation.environmentToUnset.end(), name ) ==
        invocation.environmentToUnset.end() )
      invocation.environmentToUnset.emplace_back( name );
  }

  void AddTranslatedFlag( TranslatedOptions &options,
                          const std::string &flag )
  {
    if( std::find( options.flags.begin(), options.flags.end(), flag ) ==
        options.flags.end() )
    {
      options.flags.emplace_back( flag );
      options.arguments.emplace_back( flag );
    }
  }

  void SetTranslatedOption( TranslatedOptions &options,
                            const std::string &name,
                            const std::string &value )
  {
    for( const auto &entry : options.valueIndexes )
    {
      if( entry.first == name )
      {
        options.arguments[entry.second] = value;
        return;
      }
    }

    options.arguments.emplace_back( name );
    options.arguments.emplace_back( value );
    options.valueIndexes.emplace_back( name, options.arguments.size() - 1 );
  }

  void RemoveTranslatedOption( TranslatedOptions &options,
                               const std::string &name )
  {
    for( auto entry = options.valueIndexes.begin();
         entry != options.valueIndexes.end(); ++entry )
    {
      if( entry->first != name )
        continue;

      const std::size_t valueIndex = entry->second;
      options.arguments.erase( options.arguments.begin() + valueIndex - 1,
                               options.arguments.begin() + valueIndex + 1 );
      options.valueIndexes.erase( entry );
      for( auto &remaining : options.valueIndexes )
      {
        if( remaining.second > valueIndex )
          remaining.second -= 2;
      }
      return;
    }
  }

  std::string NormalizeChecksum( std::string checksum )
  {
    const std::size_t separator = checksum.find( ':' );
    const std::size_t algorithmLength = separator == std::string::npos
                                      ? checksum.size() : separator;
    std::transform( checksum.begin(), checksum.begin() + algorithmLength,
                    checksum.begin(), []( unsigned char character )
    {
      return static_cast<char>( std::tolower( character ) );
    } );
    return checksum;
  }

  bool ApplyOption( const OptionSpec &option, const std::string &displayName,
                    const std::string &argument,
                    XrdApps::CommandLineResult &result, ParseState &state,
                    TranslatedOptions &translatedOptions )
  {
    switch( option.action )
    {
      case OptionAction::Help:
        result.action = XrdApps::CommandLineAction::Help;
        return false;
      case OptionAction::Version:
        result.action = XrdApps::CommandLineAction::Version;
        return false;
      case OptionAction::Verbose:
        ++state.verbosity;
        return true;
      case OptionAction::Timeout:
      {
        std::string normalized;
        if( !NormalizeInteger( argument, normalized, 0 ) )
        {
          result.error = displayName + " requires a non-negative integer";
          return false;
        }
        if( normalized == "0" )
          RemoveEnvironment( result.invocation, "XRD_REQUESTTIMEOUT" );
        else
          SetEnvironment( result.invocation, "XRD_REQUESTTIMEOUT", normalized );
        return true;
      }
      case OptionAction::Certificate:
        if( argument.empty() )
        {
          result.error = displayName + " requires a non-empty file name";
          return false;
        }
        state.certificateSpecified = true;
        state.certificate = argument;
        return true;
      case OptionAction::Key:
        if( argument.empty() )
        {
          result.error = displayName + " requires a non-empty file name";
          return false;
        }
        state.keySpecified = true;
        state.key = argument;
        return true;
      case OptionAction::IPv4:
      case OptionAction::IPv6:
      {
        const std::string value = option.action == OptionAction::IPv4
                                ? "IPv4" : "IPv6";
        if( !state.networkStack.empty() && state.networkStack != value )
        {
          result.error = "-4 and -6 are mutually exclusive";
          return false;
        }
        state.networkStack = value;
        SetEnvironment( result.invocation, "XRD_NETWORKSTACK", value );
        return true;
      }
      case OptionAction::LogFile:
        SetEnvironment( result.invocation, "XRD_LOGFILE", argument );
        return true;
      case OptionAction::Forward:
        if( option.takesValue )
          SetTranslatedOption( translatedOptions, option.value, argument );
        else
          AddTranslatedFlag( translatedOptions, option.value );
        return true;
      case OptionAction::Ignore:
        return true;
      case OptionAction::Warn:
        result.invocation.warnings.emplace_back( option.value );
        return true;
      case OptionAction::Reject:
        result.error = displayName + ": " + option.value;
        return false;
      case OptionAction::CopyTimeout:
      {
        std::string normalized;
        if( !NormalizeInteger( argument, normalized, 0 ) )
        {
          result.error = displayName + " requires a non-negative integer";
          return false;
        }
        SetEnvironment( result.invocation, "XRD_CPTIMEOUT", normalized );
        return true;
      }
      case OptionAction::CopyFromFile:
        if( argument.empty() )
        {
          result.error = displayName + " requires a non-empty file name";
          return false;
        }
        state.fromFile = true;
        SetTranslatedOption( translatedOptions, "--infiles", argument );
        return true;
      case OptionAction::CopyStreams:
      {
        std::string normalized;
        if( !NormalizeInteger( argument, normalized, 0, 15 ) )
        {
          result.error = displayName + " requires an integer from 0 to 15";
          return false;
        }
        if( normalized == "0" )
          RemoveTranslatedOption( translatedOptions, "--streams" );
        else
          SetTranslatedOption( translatedOptions, "--streams", normalized );
        return true;
      }
      case OptionAction::CopyChecksum:
        if( argument.empty() || argument[0] == ':' )
        {
          result.error = displayName + " requires a checksum algorithm";
          return false;
        }
        SetTranslatedOption( translatedOptions, "--cksum",
                             NormalizeChecksum( argument ) );
        return true;
      case OptionAction::Color:
        if( argument == "never" || argument == "auto" )
          return true;
        if( argument == "always" )
        {
          result.error = "--color=always requires native xrdfs output support";
          return false;
        }
        result.error = "--color must be one of always, never, or auto";
        return false;
    }
    return false;
  }

  bool IsOptionToken( const std::string &value )
  {
    return value.size() > 1 && value[0] == '-';
  }

  template<std::size_t Size>
  bool ParseOptions( const std::vector<std::string> &arguments,
                     const OptionSpec (&commandOptions)[Size],
                     XrdApps::CommandLineResult &result, ParseState &state,
                     TranslatedOptions &translatedOptions,
                     std::vector<std::string> &operands )
  {
    bool optionsEnabled = true;
    for( std::size_t index = 0; index < arguments.size(); ++index )
    {
      const std::string &token = arguments[index];
      if( optionsEnabled && token == "--" )
      {
        optionsEnabled = false;
        continue;
      }

      if( optionsEnabled && token.size() > 2 && token.compare( 0, 2, "--" ) == 0 )
      {
        const std::size_t separator = token.find( '=' );
        const std::string name = token.substr( 2, separator == std::string::npos
                                                 ? std::string::npos
                                                 : separator - 2 );
        bool isCommon = false;
        const OptionSpec *option = FindLongOption( name, commandOptions, isCommon );
        (void)isCommon;
        if( !option )
        {
          result.error = "unknown option '--" + name + "'";
          return false;
        }

        std::string value;
        if( option->takesValue )
        {
          if( separator != std::string::npos )
            value = token.substr( separator + 1 );
          else if( index + 1 < arguments.size() &&
                   !IsOptionToken( arguments[index + 1] ) )
            value = arguments[++index];
          else
          {
            result.error = "option '--" + name + "' requires an argument";
            return false;
          }
        }
        else if( separator != std::string::npos )
        {
          result.error = "option '--" + name + "' does not take an argument";
          return false;
        }

        if( !ApplyOption( *option, "--" + name, value, result, state,
                          translatedOptions ) )
          return false;
        continue;
      }

      if( optionsEnabled && token.size() > 1 && token[0] == '-' && token != "-" )
      {
        for( std::size_t position = 1; position < token.size(); ++position )
        {
          const char name = token[position];
          bool isCommon = false;
          const OptionSpec *option = FindShortOption( name, commandOptions, isCommon );
          (void)isCommon;
          if( !option )
          {
            result.error = std::string( "unknown option '-" ) + name + "'";
            return false;
          }

          std::string value;
          if( option->takesValue )
          {
            if( position + 1 < token.size() )
            {
              value = token.substr( position + 1 );
              position = token.size();
            }
            else if( index + 1 < arguments.size() &&
                     !IsOptionToken( arguments[index + 1] ) )
              value = arguments[++index];
            else
            {
              result.error = std::string( "option '-" ) + name +
                             "' requires an argument";
              return false;
            }
          }

          if( !ApplyOption( *option, std::string( "-" ) + name, value,
                            result, state, translatedOptions ) )
            return false;
          if( option->takesValue )
            break;
        }
        continue;
      }

      if( !optionsEnabled && result.command == "copy" && token.size() > 1 &&
          token[0] == '-' )
        operands.emplace_back( "./" + token );
      else
        operands.emplace_back( token );
    }
    return true;
  }

  bool IsCompleteRemoteURL( const std::string &value )
  {
    const std::size_t separator = value.find( "://" );
    if( separator == std::string::npos || separator == 0 ||
        std::isalpha( static_cast<unsigned char>( value[0] ) ) == 0 )
      return false;

    for( std::size_t index = 1; index < separator; ++index )
    {
      const unsigned char character = value[index];
      if( std::isalnum( character ) == 0 && character != '+' &&
          character != '-' && character != '.' )
        return false;
    }

    std::string protocol = value.substr( 0, separator );
    std::transform( protocol.begin(), protocol.end(), protocol.begin(),
                    []( unsigned char character )
    {
      return static_cast<char>( std::tolower( character ) );
    } );
    if( protocol == "file" || protocol == "stdio" )
      return false;

    const std::size_t authority = separator + 3;
    return authority < value.size() && value[authority] != '/' &&
           value[authority] != '?' && value[authority] != '#';
  }

  void ApplyCredentialOptions( XrdApps::CommandLineResult &result,
                               const ParseState &state )
  {
    if( !state.certificateSpecified )
    {
      if( state.keySpecified )
        result.invocation.warnings.emplace_back(
          "--key was ignored because --cert was not provided" );
      return;
    }

    const std::string &key = state.keySpecified ? state.key : state.certificate;
    SetEnvironment( result.invocation, "X509_USER_CERT", state.certificate );
    SetEnvironment( result.invocation, "X509_USER_KEY", key );
    SetEnvironment( result.invocation, "XrdSecGSIUSERCERT", state.certificate );
    SetEnvironment( result.invocation, "XrdSecGSIUSERKEY", key );
    SetEnvironment( result.invocation, "XrdSecGSIUSERPROXY", state.certificate );
    SetEnvironment( result.invocation, "XRD_HTTPCLIENTCERTFILE",
                    state.certificate );
    SetEnvironment( result.invocation, "XRD_HTTPCLIENTKEYFILE", key );
    UnsetEnvironment( result.invocation, "X509_USER_PROXY" );
    UnsetEnvironment( result.invocation, "XrdSecCREDS" );
  }

  bool ValidateOperands( XrdApps::CommandLineResult &result,
                         const ParseState &state,
                         const std::vector<std::string> &operands )
  {
    if( result.command == "stat" || result.command == "ls" )
    {
      if( operands.size() != 1 )
      {
        result.error = "xrd " + result.command + " requires exactly one URL";
        return false;
      }
      if( !IsCompleteRemoteURL( operands[0] ) )
      {
        result.error = "xrd " + result.command +
                       " requires a complete remote URL";
        return false;
      }
      return true;
    }

    if( result.command == "cat" )
    {
      if( operands.empty() )
      {
        result.error = "xrd cat requires at least one URL";
        return false;
      }
      if( !std::all_of( operands.begin(), operands.end(), IsCompleteRemoteURL ) )
      {
        result.error = "xrd cat requires complete remote URLs";
        return false;
      }
      return true;
    }

    if( result.command == "xattr" )
    {
      if( operands.empty() || operands.size() > 2 )
      {
        result.error = "xrd xattr requires a URL and optional attribute name";
        return false;
      }
      if( !IsCompleteRemoteURL( operands[0] ) )
      {
        result.error = "xrd xattr requires a complete remote URL";
        return false;
      }
      if( operands.size() == 2 && operands[1].find( '=' ) != std::string::npos )
      {
        result.error = "xrd xattr currently supports read-only list/get operations";
        return false;
      }
      return true;
    }

    if( result.command == "copy" )
    {
      const std::size_t expected = state.fromFile ? 1 : 2;
      if( operands.size() != expected )
      {
        if( state.fromFile )
          result.error = "xrd copy --from-file requires one destination";
        else if( operands.size() > 2 )
          result.error = "xrd copy does not yet support chained destinations";
        else
          result.error = "xrd copy requires one source and one destination";
        return false;
      }
      return true;
    }

    return false;
  }

  template<std::size_t Size>
  XrdApps::CommandLineResult ParseCommand(
    const std::string &command, const std::vector<std::string> &arguments,
    const OptionSpec (&commandOptions)[Size] )
  {
    XrdApps::CommandLineResult result;
    result.command = command;
    result.invocation.executable = command == "copy" ? "xrdcp" : "xrdfs";

    ParseState state;
    TranslatedOptions translatedOptions;
    std::vector<std::string> operands;
    if( !ParseOptions( arguments, commandOptions, result, state,
                       translatedOptions, operands ) )
    {
      if( result.action != XrdApps::CommandLineAction::Help &&
          result.action != XrdApps::CommandLineAction::Version )
        result.action = XrdApps::CommandLineAction::Error;
      return result;
    }

    if( !ValidateOperands( result, state, operands ) )
    {
      result.action = XrdApps::CommandLineAction::Error;
      return result;
    }

    ApplyCredentialOptions( result, state );

    if( state.verbosity != 0 )
    {
      const char *level = state.verbosity == 1 ? "Warning"
                        : state.verbosity == 2 ? "Info" : "Debug";
      SetEnvironment( result.invocation, "XRD_LOGLEVEL", level );
    }

    if( command == "xattr" )
    {
      result.invocation.arguments.emplace_back( command );
      result.invocation.arguments.emplace_back( operands[0] );
      result.invocation.arguments.emplace_back( operands.size() == 1
                                                ? "list" : "get" );
      if( operands.size() == 2 )
        result.invocation.arguments.emplace_back( operands[1] );
      result.action = XrdApps::CommandLineAction::Execute;
      return result;
    }

    if( command != "copy" )
      result.invocation.arguments.emplace_back( command );
    result.invocation.arguments.insert( result.invocation.arguments.end(),
                                        translatedOptions.arguments.begin(),
                                        translatedOptions.arguments.end() );
    result.invocation.arguments.insert( result.invocation.arguments.end(),
                                        operands.begin(), operands.end() );
    result.action = XrdApps::CommandLineAction::Execute;
    return result;
  }

  constexpr OptionSpec NoOptions[] = {
    {0, nullptr, false, OptionAction::Ignore, nullptr},
  };
}

namespace XrdApps
{
  CommandLineResult ParseCommandLine( const std::vector<std::string> &arguments )
  {
    if( arguments.empty() )
    {
      CommandLineResult result;
      result.action = CommandLineAction::Help;
      return result;
    }

    if( arguments[0] == "-h" || arguments[0] == "--help" )
    {
      CommandLineResult result;
      result.action = CommandLineAction::Help;
      return result;
    }
    if( arguments[0] == "-V" || arguments[0] == "--version" )
    {
      CommandLineResult result;
      result.action = CommandLineAction::Version;
      return result;
    }

    std::string command = arguments[0];
    if( command == "cp" )
      command = "copy";
    const std::vector<std::string> commandArguments( arguments.begin() + 1,
                                                     arguments.end() );

    if( command == "stat" )
      return ParseCommand( command, commandArguments, NoOptions );
    if( command == "ls" )
      return ParseCommand( command, commandArguments, LsOptions );
    if( command == "cat" )
      return ParseCommand( command, commandArguments, CatOptions );
    if( command == "xattr" )
      return ParseCommand( command, commandArguments, NoOptions );
    if( command == "copy" )
      return ParseCommand( command, commandArguments, CopyOptions );

    CommandLineResult result;
    result.command = command;
    result.error = "unknown command '" + command + "'";
    return result;
  }
}
