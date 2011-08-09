//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <XrdOuc/XrdOucTokenizer.hh>

#include "XrdCl/XrdClLog.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Open a file
  //----------------------------------------------------------------------------
  bool LogOutFile::Open( const std::string &filename )
  {
    int fd = open( filename.c_str(), O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR );
    if( fd < 0 )
    {
      std::cerr << "Unable to open " << filename << " " << strerror( errno );
      std::cerr << std::endl;
      return false;
    }
    pFileDes = fd;
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  void LogOutFile::Close()
  {
    if( pFileDes != -1 )
    {
      close( pFileDes );
      pFileDes = -1;
    }
  }

  //----------------------------------------------------------------------------
  // Message handler
  //----------------------------------------------------------------------------
  void LogOutFile::Write( const std::string &message )
  {
    if( unlikely( pFileDes == -1 ) )
    {
      std::cerr << "Log file not opened" << std::endl;
      return;
    }
    int ret = write( pFileDes, message.c_str(), message.length() );
    if( ret < 0 )
    {
      std::cerr << "Unable to write to the log file: " << strerror( errno );
      std::cerr << std::endl;
      return;
    }
  }

  //----------------------------------------------------------------------------
  // Message handler
  //----------------------------------------------------------------------------
  void LogOutCerr::Write( const std::string &message )
  {
    pMutex.Lock();
    std::cerr << message;
    pMutex.UnLock();
  }

  //----------------------------------------------------------------------------
  // Print an error message
  //----------------------------------------------------------------------------
  void Log::Say( LogLevel level, uint64_t type, const char *format, va_list list )
  {
    //--------------------------------------------------------------------------
    // Build the user message
    //--------------------------------------------------------------------------
    int size     = 1024;
    int ret      = 0;
    char *buffer = 0;
    while(1)
    {
      buffer = new char[size];
      ret = vsnprintf( buffer, size, format, list );

      if( ret < 0 )
      {
        pOutput->Write( "Error while processing a log message" );
        return;
      }
      else if( ret < size )
        break;

      size *= 2;
      delete buffer;
    }

    //--------------------------------------------------------------------------
    // Add time and error level
    //--------------------------------------------------------------------------
    char   now[30];
    tm     tsNow;
    time_t ttNow = time(0);
    localtime_r( &ttNow, &tsNow );
    strftime( now, 30, "%Y-%m-%d %H:%M:%S %z", &tsNow );

    XrdOucTokenizer tok( buffer );
    char *line = 0;
    std::ostringstream out;
    while( (line = tok.GetLine()) )
      out << "[" << now << "][" << LogLevelToString( level ) << "] " << line << std::endl;

    pOutput->Write( out.str() );
    delete buffer;
  }

  //----------------------------------------------------------------------------
  // Get a default logger
  //----------------------------------------------------------------------------
  Log *Log::sDefaultLog = 0;

  Log *Log::GetDefaultLog()
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
  // Convert log level to string
  //----------------------------------------------------------------------------
  std::string Log::LogLevelToString( LogLevel level )
  {
    switch( level )
    {
      case ErrorMsg:
        return "Error  ";
      case WarningMsg:
        return "Warning";
      case InfoMsg:
        return "Info   ";
      case DebugMsg:
        return "Debug  ";
      case DumpMsg:
        return "Dump   ";
      default:
        return "Unknown Level";
    }
    return "Unknown Level";
  }

  //----------------------------------------------------------------------------
  // Convert a string to LogLevel
  //----------------------------------------------------------------------------
  bool Log::StringToLogLevel( const std::string &strLevel, LogLevel &level )
  {
    if( strLevel == "Error" )        level = ErrorMsg;
    else if( strLevel == "Warning" ) level = WarningMsg;
    else if( strLevel == "Info" )    level = InfoMsg;
    else if( strLevel == "Debug" )   level = DebugMsg;
    else if( strLevel == "Dump" )    level = DumpMsg;
    else return false;
    return true;
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
      Log *log = Log::GetDefaultLog();

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
      delete Log::GetDefaultLog();
    }
  };
  static LogInitializer initialized;
}
