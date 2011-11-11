//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"

#include <map>

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Get default log
  //----------------------------------------------------------------------------
  Log *Utils::sDefaultLog = 0;

  Log *Utils::GetDefaultLog()
  {
    //--------------------------------------------------------------------------
    // This is actually thread safe because it is first called from
    // a static initializer in a thread safe context
    //--------------------------------------------------------------------------
    if( unlikely( !sDefaultLog ) )
      sDefaultLog = new Log();
    return sDefaultLog;
  }

  //----------------------------------------------------------------------------
  // Split string
  //----------------------------------------------------------------------------
  void Utils::splitString( std::vector<std::string> &result,
                           const std::string  &input,
                           const std::string  &delimiter )
  {
    size_t start  = 0;
    size_t end    = 0;
    size_t length = 0;

    do
    {
      end = input.find( delimiter, start );

      if( end != std::string::npos )
        length = end - start;
      else
        length = std::string::npos;

      result.push_back( input.substr( start, length ) );

      start = end + delimiter.size();
    }
    while( end != std::string::npos );
  }
}

//------------------------------------------------------------------------------
// Default log initialization
//------------------------------------------------------------------------------
namespace
{
  //----------------------------------------------------------------------------
  // Translate a string into a topic mask
  //----------------------------------------------------------------------------
  struct MaskTranslator
  {
    //--------------------------------------------------------------------------
    // Initialize the translation array
    //--------------------------------------------------------------------------
    MaskTranslator()
    {
      masks["AppMsg"]     = XrdClient::AppMsg;
      masks["UtilityMsg"] = XrdClient::UtilityMsg;
      masks["FileMsg"]    = XrdClient::FileMsg;
    }

    //--------------------------------------------------------------------------
    // Translate the mask
    //--------------------------------------------------------------------------
    uint64_t translateMask( const std::string mask )
    {
      if( mask == "" || mask == "All" )
        return 0xffffffffffffffff;

      if( mask == "None" )
        return 0;

      std::vector<std::string>           topics;
      std::vector<std::string>::iterator it;
      XrdClient::Utils::splitString( topics, mask, "|" );

      uint64_t resultMask = 0;
      std::map<std::string, uint64_t>::iterator maskIt;
      for( it = topics.begin(); it != topics.end(); ++it )
      {
        maskIt = masks.find( *it );
        if( maskIt != masks.end() )
          resultMask |= maskIt->second;
      }

      return resultMask;
    }

    std::map<std::string, uint64_t> masks;
  };

  //----------------------------------------------------------------------------
  // Initialize the log
  //----------------------------------------------------------------------------
  struct LogInitializer
  {
    //--------------------------------------------------------------------------
    // Initializer
    //--------------------------------------------------------------------------
    LogInitializer()
    {
      using namespace XrdClient;
      Log *log = Utils::GetDefaultLog();

      //------------------------------------------------------------------------
      // Check if the log level has been defined in the environment
      //------------------------------------------------------------------------
      char *level = getenv( "XRD_LOGLEVEL" );
      if( level )
        log->SetLevel( level );

      //------------------------------------------------------------------------
      // Check if we need to log to a file
      //------------------------------------------------------------------------
      char *file = getenv( "XRD_LOGFILE" );
      if( file )
      {
        LogOutFile *out = new LogOutFile();
        if( out->Open( file ) )
          log->SetOutput( out );
        else
          delete out;
      }

      //------------------------------------------------------------------------
      // Initialize the topic mask
      //------------------------------------------------------------------------
      char *logMask = getenv( "XRD_LOGMASK" );
      if( logMask )
      {
        MaskTranslator translator;
        log->SetMask( translator.translateMask( logMask ) );
      }
    }

    //--------------------------------------------------------------------------
    // Finalizer
    //--------------------------------------------------------------------------
    ~LogInitializer()
    {
      using namespace XrdClient;
      delete Utils::GetDefaultLog();
    }
  };
  static LogInitializer initialized;
}
