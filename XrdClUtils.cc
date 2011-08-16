//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClLog.hh"

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
}

//------------------------------------------------------------------------------
// Default log initialization
//------------------------------------------------------------------------------
namespace
{
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
