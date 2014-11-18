//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_LOG_HH__
#define __XRD_CL_LOG_HH__

#include <stdarg.h>
#include <string>
#include <map>
#include <stdint.h>

#include "XrdCl/XrdClOptimizers.hh"

#include "XrdSys/XrdSysPthread.hh"

//------------------------------------------------------------------------------
// C++11 atomics are used to avoid illegal behavior when setting/getting the
// log level. To minimize costs across all platforms, we use
// std::memory_order_relaxed; this means threads may reorder SetLogLevel writes
// and the visibility is relatively undefined. However, we know the stores are
// at least atomic.
//------------------------------------------------------------------------------
#if __cplusplus >= 201103L
#include <atomic>
#endif

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Interface for logger outputs
  //----------------------------------------------------------------------------
  class LogOut
  {
    public:
      virtual ~LogOut() {}

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
      virtual ~LogOutFile() { Close(); };

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
      virtual ~LogOutCerr() {}
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
        NoMsg       = 0,  //!< report nothing
        ErrorMsg    = 1,  //!< report errors
        WarningMsg  = 2,  //!< report warnings
        InfoMsg     = 3,  //!< print info
        DebugMsg    = 4,  //!< print debug info
        DumpMsg     = 5   //!< print details of the request and responses
      };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Log(): pLevel( NoMsg ), pTopicMaxLength( 18 )
      {
        pOutput = new LogOutCerr();
        int maxMask = (int)DumpMsg+1;
        for( int i = 0; i < maxMask; ++i )
          pMask[i] = 0xffffffffffffffffULL;
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
      void Error( uint64_t topic, const char *format, ... )
      {
        if( unlikely( GetLevel() < ErrorMsg ) )
          return;

        if( unlikely( (topic & pMask[ErrorMsg]) == 0 ) )
          return;

        va_list argList;
        va_start( argList, format );
        Say( ErrorMsg, topic, format, argList );
        va_end( argList );
      }

      //------------------------------------------------------------------------
      //! Report a warning
      //------------------------------------------------------------------------
      void Warning( uint64_t topic, const char *format, ... )
      {
        if( unlikely( GetLevel() < WarningMsg ) )
          return;

        if( unlikely( (topic & pMask[WarningMsg]) == 0 ) )
          return;

        va_list argList;
        va_start( argList, format );
        Say( WarningMsg, topic, format, argList );
        va_end( argList );
      }

      //------------------------------------------------------------------------
      //! Print an info
      //------------------------------------------------------------------------
      void Info( uint64_t topic, const char *format, ... )
      {
        if( likely( GetLevel() < InfoMsg ) )
          return;

        if( unlikely( (topic & pMask[InfoMsg]) == 0 ) )
          return;

        va_list argList;
        va_start( argList, format );
        Say( InfoMsg, topic, format, argList );
        va_end( argList );
      }

      //------------------------------------------------------------------------
      //! Print a debug message
      //------------------------------------------------------------------------
      void Debug( uint64_t topic, const char *format, ... )
      {
        if( likely( GetLevel() < DebugMsg ) )
          return;

        if( unlikely( (topic & pMask[DebugMsg]) == 0 ) )
          return;

        va_list argList;
        va_start( argList, format );
        Say( DebugMsg, topic, format, argList );
        va_end( argList );
      }

      //------------------------------------------------------------------------
      //! Print a dump message
      //------------------------------------------------------------------------
      void Dump( uint64_t topic, const char *format, ... )
      {
        if( likely( GetLevel() < DumpMsg ) )
          return;

        if( unlikely( (topic & pMask[DumpMsg]) == 0 ) )
          return;

        va_list argList;
        va_start( argList, format );
        Say( DumpMsg, topic, format, argList );
        va_end( argList );
      }

      //------------------------------------------------------------------------
      //! Always print the message
      //!
      //! @param level  log level
      //! @param type   topic of the message
      //! @param format format string - the same as in printf
      //! @param list   list of arguments
      //------------------------------------------------------------------------
      void Say( LogLevel level, uint64_t topic, const char *format, va_list list );

      //------------------------------------------------------------------------
      //! Set the level of the messages that should be sent to the destination
      //------------------------------------------------------------------------
      void SetLevel( LogLevel level )
      {
#if __cplusplus >= 201103L
        pLevel.store(level, std::memory_order_relaxed);
#else
        pLevel = level;
#endif
      }

      //------------------------------------------------------------------------
      //! Set the level of the messages that should be sent to the destination
      //------------------------------------------------------------------------
      void SetLevel( const std::string &level )
      {
        LogLevel lvl;
        if( StringToLogLevel( level, lvl ) )
          SetLevel( lvl );
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
      //! Sets the mask for the topics of messages that should be printed
      //------------------------------------------------------------------------
      void SetMask( LogLevel level, uint64_t mask )
      {
        pMask[level] = mask;
      }

      //------------------------------------------------------------------------
      //! Sets the mask for the topics of messages that should be printed
      //------------------------------------------------------------------------
      void SetMask( const std::string &level, uint64_t mask )
      {
        LogLevel lvl;
        if( StringToLogLevel( level, lvl ) )
          pMask[lvl] = mask;
      }

      //------------------------------------------------------------------------
      //! Map a topic number to a string
      //------------------------------------------------------------------------
      void SetTopicName( uint64_t topic, std::string name );

      //------------------------------------------------------------------------
      //! Get the log level
      //------------------------------------------------------------------------
      LogLevel GetLevel() const
      {
#if __cplusplus >= 201103L
        LogLevel lvl = pLevel.load(std::memory_order_relaxed);
        return lvl;
#else
        return pLevel;
#endif
      }

    private:
      typedef std::map<uint64_t, std::string> TopicMap;
      std::string LogLevelToString( LogLevel level );
      bool StringToLogLevel( const std::string &strLevel, LogLevel &level );
      std::string TopicToString( uint64_t topic );

#if __cplusplus >= 201103L
      std::atomic<LogLevel> pLevel;
#else
      LogLevel    pLevel;
#endif
      uint64_t    pMask[DumpMsg+1];
      LogOut     *pOutput;
      TopicMap    pTopicMap;
      uint32_t    pTopicMaxLength;
  };
}

#endif // __XRD_CL_LOG_HH__
