//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#ifndef __XRD_CL_LOG_HH__
#define __XRD_CL_LOG_HH__

#include <stdarg.h>
#include <string>
#include <stdint.h>

#include "XrdCl/XrdClOptimizers.hh"

#include "XrdSys/XrdSysPthread.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  //! Interface for logger outputs
  //----------------------------------------------------------------------------
  class LogOut
  {
    public:
      //------------------------------------------------------------------------
      //! Write a message to the destination
      //!
      //! @param message message to be written
      //------------------------------------------------------------------------
      virtual void Write( const std::string &message ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Write log messages to a file
  //----------------------------------------------------------------------------
  class LogOutFile: public LogOut
  {
    public:
      LogOutFile(): pFileDes(-1) {};
      ~LogOutFile() { Close(); };

      //------------------------------------------------------------------------
      //! Open the log file
      //------------------------------------------------------------------------
      bool Open( const std::string &fileName );

      //------------------------------------------------------------------------
      //! Close the log file
      //------------------------------------------------------------------------
      void Close();
      virtual void Write( const std::string &message );
    private:
      int pFileDes;
  };

  //----------------------------------------------------------------------------
  //! Write log messages to stderr
  //----------------------------------------------------------------------------
  class LogOutCerr: public LogOut
  {
    public:
      virtual void Write( const std::string &message );
    private:
      XrdSysMutex pMutex;
  };

  //----------------------------------------------------------------------------
  //! Handle diagnostics
  //----------------------------------------------------------------------------
  class Log
  {
    public:
      //------------------------------------------------------------------------
      //! Log levels
      //------------------------------------------------------------------------
      enum LogLevel
      {
        ErrorMsg    = 1,  //!< report errors
        WarningMsg  = 2,  //!< report warnings
        InfoMsg     = 3,  //!< print info
        DebugMsg    = 4,  //!< print debug info
        DumpMsg     = 5   //!< print details of the request and responses
      };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Log(): pLevel( WarningMsg ), pMask( 0xffffffffffffffff )
      {
        pOutput = new LogOutCerr();
      }

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      ~Log()
      {
        delete pOutput;
      }

      //------------------------------------------------------------------------
      //! Report an error
      //------------------------------------------------------------------------
      void Error( uint64_t type, const char *format, ... )
      {
        if( unlikely( pLevel < ErrorMsg ) )
          return;

        if( unlikely( (type & pMask) == 0 ) )
          return;

        va_list argList;
        va_start( argList, format );
        Say( ErrorMsg, type, format, argList );
        va_end( argList );
      }

      //------------------------------------------------------------------------
      //! Report a warning
      //------------------------------------------------------------------------
      void Warning( uint64_t type, const char *format, ... )
      {
        if( unlikely( pLevel < WarningMsg ) )
          return;

        if( unlikely( (type & pMask) == 0 ) )
          return;

        va_list argList;
        va_start( argList, format );
        Say( WarningMsg, type, format, argList );
        va_end( argList );
      }

      //------------------------------------------------------------------------
      //! Print an info
      //------------------------------------------------------------------------
      void Info( uint64_t type, const char *format, ... )
      {
        if( likely( pLevel < InfoMsg ) )
          return;

        if( unlikely( (type & pMask) == 0 ) )
          return;

        va_list argList;
        va_start( argList, format );
        Say( InfoMsg, type, format, argList );
        va_end( argList );
      }

      //------------------------------------------------------------------------
      //! Print a debug message
      //------------------------------------------------------------------------
      void Debug( uint64_t type, const char *format, ... )
      {
        if( likely( pLevel < DebugMsg ) )
          return;

        if( unlikely( (type & pMask) == 0 ) )
          return;

        va_list argList;
        va_start( argList, format );
        Say( DebugMsg, type, format, argList );
        va_end( argList );
      }

      //------------------------------------------------------------------------
      //! Print a dump message
      //------------------------------------------------------------------------
      void Dump( uint64_t type, const char *format, ... )
      {
        if( likely( pLevel < DumpMsg ) )
          return;

        if( unlikely( (type & pMask) == 0 ) )
          return;

        va_list argList;
        va_start( argList, format );
        Say( DumpMsg, type, format, argList );
        va_end( argList );
      }

      //------------------------------------------------------------------------
      //! Always print the message
      //!
      //! @param level  log level
      //! @param type   type of the message
      //! @param format format string - the same as in printf
      //! @param list   list of arguments
      //------------------------------------------------------------------------
      void Say( LogLevel level, uint64_t type, const char *format, va_list list );

      //------------------------------------------------------------------------
      //! Set the level of the messages that should be sent to the destination
      //------------------------------------------------------------------------
      void SetLevel( LogLevel level )
      {
        pLevel = level;
      }

      //------------------------------------------------------------------------
      //! Set the output that should be used.
      //------------------------------------------------------------------------
      void SetOutput( LogOut *output )
      {
        delete pOutput;
        pOutput = output;
      }

      //------------------------------------------------------------------------
      //! Sets the mask for the types of messages that should be printed
      //------------------------------------------------------------------------
      void SetMask( uint64_t mask )
      {
        pMask = mask;
      }

      //------------------------------------------------------------------------
      //! Get a default log.
      //! Currently no other log is supported so it's more of a singleton
      //! implementation than anything else.
      //------------------------------------------------------------------------
      static Log *GetDefaultLog();

    private:
      std::string LogLevelToString( LogLevel level );

      static Log *sDefaultLog;
      LogLevel    pLevel;
      uint64_t    pMask;
      LogOut     *pOutput;
  };
}

#endif // __XRD_CL_LOG_HH__
