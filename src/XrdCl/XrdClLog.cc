//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <ctime>

#include <XrdOuc/XrdOucTokenizer.hh>

#include "XrdCl/XrdClLog.hh"

namespace XrdCl
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
    return true;
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
  void Log::Say( LogLevel    level,
                 uint64_t    topic,
                 const char *format,
                 va_list     list )
  {
    //--------------------------------------------------------------------------
    // Build the user message
    //--------------------------------------------------------------------------
    int size     = 1024;
    int ret      = 0;
    char *buffer = 0;
    while(1)
    {
      va_list cp;
      va_copy( cp, list );
      buffer = new char[size];
      ret = vsnprintf( buffer, size, format, cp );
      va_end( cp );

      if( ret < 0 )
      {
        snprintf( buffer, size, "Error while processing a log message \"%s\" \n", format);
        pOutput->Write(buffer);
        delete [] buffer;
        return;
      }
      else if( ret < size )
        break;

      size *= 2;
      delete [] buffer;
    }

    //--------------------------------------------------------------------------
    // Add time and error level
    //--------------------------------------------------------------------------
    char    now[48];
    char    ts[32];
    char    tz[8];
    tm      tsNow;
    timeval ttNow;

    gettimeofday( &ttNow, 0 );
    localtime_r( &ttNow.tv_sec, &tsNow );

    strftime( ts, 32, "%Y-%m-%d %H:%M:%S", &tsNow );
    strftime( tz, 8, "%z", &tsNow );
    snprintf( now, 48, "%s.%06ld %s", ts, (long int)ttNow.tv_usec, tz );

    XrdOucTokenizer tok( buffer );
    char *line = 0;
    std::ostringstream out;
    while( (line = tok.GetLine()) )
    {
      out << "[" << now << "][" << LogLevelToString( level ) << "]";
      out << "[" << TopicToString( topic ) << "] " << line << std::endl;
    }

    pOutput->Write( out.str() );
    delete [] buffer;
  }

  //----------------------------------------------------------------------------
  // Map a topic number to a string
  //----------------------------------------------------------------------------
  void Log::SetTopicName( uint64_t topic, std::string name )
  {
    uint32_t len = name.length();
    if( len > pTopicMaxLength )
    {
      pTopicMaxLength = len;
      TopicMap::iterator it;
      for( it = pTopicMap.begin(); it != pTopicMap.end(); ++it )
        it->second.append( len-it->second.length(), ' ' );
    }
    else
      name.append( pTopicMaxLength-len, ' ' );
    pTopicMap[topic] = name;
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

  //----------------------------------------------------------------------------
  // Convert a topic number to a string
  //----------------------------------------------------------------------------
  std::string Log::TopicToString( uint64_t topic )
  {
    TopicMap::iterator it = pTopicMap.find( topic );
    if( it != pTopicMap.end() )
      return it->second;
    std::ostringstream o;
    o << "0x" << std::setw(pTopicMaxLength-2) << std::setfill( '0' );
    o << std::setbase(16) << topic;
    return o.str();
  }
}
