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

#ifndef __XRD_CL_UTILS_HH__
#define __XRD_CL_UTILS_HH__

#include <string>
#include <vector>
#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClPropertyList.hh"
#include "XrdNet/XrdNetUtils.hh"

#include <sys/time.h>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Random utilities
  //----------------------------------------------------------------------------
  class Utils
  {
    public:
      //------------------------------------------------------------------------
      //! Split a string
      //------------------------------------------------------------------------
      template<class Container>
      static void splitString( Container         &result,
                               const std::string &input,
                               const std::string &delimiter )
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
            length = input.length() - start;

          if( length )
            result.push_back( input.substr( start, length ) );

          start = end + delimiter.size();
        }
        while( end != std::string::npos );
      }

      //------------------------------------------------------------------------
      //! Get a parameter either from the environment or URL
      //------------------------------------------------------------------------
      static int GetIntParameter( const URL         &url,
                                  const std::string &name,
                                  int                defaultVal );

      //------------------------------------------------------------------------
      //! Get a parameter either from the environment or URL
      //------------------------------------------------------------------------
      static std::string GetStringParameter( const URL         &url,
                                             const std::string &name,
                                             const std::string &defaultVal );

      //------------------------------------------------------------------------
      //! Address type
      //------------------------------------------------------------------------
      enum AddressType
      {
        IPAuto        = 0,
        IPAll         = 1,
        IPv6          = 2,
        IPv4          = 3,
        IPv4Mapped6   = 4
      };

      //------------------------------------------------------------------------
      //! Interpret a string as address type, default to IPAll
      //------------------------------------------------------------------------
      static AddressType String2AddressType( const std::string &addressType );

      //------------------------------------------------------------------------
      //! Resolve IP addresses
      //------------------------------------------------------------------------
      static Status GetHostAddresses( std::vector<XrdNetAddr> &addresses,
                                      const URL               &url,
                                      AddressType              type );

      //------------------------------------------------------------------------
      //! Log all the addresses on the list
      //------------------------------------------------------------------------
      static void LogHostAddresses( Log                     *log,
                                    uint64_t                 type,
                                    const std::string       &hostId,
                                    std::vector<XrdNetAddr> &addresses );

      //------------------------------------------------------------------------
      //! Convert timestamp to a string
      //------------------------------------------------------------------------
      static std::string TimeToString( time_t timestamp );

      //------------------------------------------------------------------------
      //! Get the elapsed microseconds between two timevals
      //------------------------------------------------------------------------
      static uint64_t GetElapsedMicroSecs( timeval start, timeval end );

      //------------------------------------------------------------------------
      //! Get a checksum from a remote xrootd server
      //------------------------------------------------------------------------
      static XRootDStatus GetRemoteCheckSum( std::string       &checkSum,
                                             const std::string &checkSumType,
                                             const std::string &server,
                                             const std::string &path );

      //------------------------------------------------------------------------
      //! Get a checksum from local file
      //------------------------------------------------------------------------
      static XRootDStatus GetLocalCheckSum( std::string       &checkSum,
                                            const std::string &checkSumType,
                                            const std::string &path );

      //------------------------------------------------------------------------
      //! Convert bytes to a human readable string
      //------------------------------------------------------------------------
      static std::string BytesToString( uint64_t bytes );

      //------------------------------------------------------------------------
      //! Check if peer supports tpc
      //------------------------------------------------------------------------
      static XRootDStatus CheckTPC( const std::string &server,
                                    uint16_t           timeout = 0 );

      //------------------------------------------------------------------------
      //! Convert the fully qualified host name to country code
      //------------------------------------------------------------------------
      static std::string FQDNToCC( const std::string &fqdn );

      //------------------------------------------------------------------------
      //! Get directory entries
      //------------------------------------------------------------------------
      static Status GetDirectoryEntries( std::vector<std::string> &entries,
                                         const std::string        &path );

      //------------------------------------------------------------------------
      //! Process a config file and return key-value pairs
      //------------------------------------------------------------------------
      static Status ProcessConfig( std::map<std::string, std::string> &config,
                                   const std::string                  &file );

      //------------------------------------------------------------------------
      //! Trim a string
      //------------------------------------------------------------------------
      static void Trim( std::string &str );

      //------------------------------------------------------------------------
      //! Log property list
      //------------------------------------------------------------------------
      static void LogPropertyList( Log                *log,
                                   uint64_t            topic,
                                   const char         *format,
                                   const PropertyList &list );

      //------------------------------------------------------------------------
      //! Print a char array as hex
      //------------------------------------------------------------------------
      static std::string Char2Hex( uint8_t *array, uint16_t size );

      //------------------------------------------------------------------------
      //! Normalize checksum
      //------------------------------------------------------------------------
      static std::string NormalizeChecksum( const std::string &name,
                                            const std::string &checksum );
  };

  //----------------------------------------------------------------------------
  //! Smart descriptor - closes the descriptor on destruction
  //----------------------------------------------------------------------------
  class ScopedDescriptor
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      ScopedDescriptor( int descriptor ): pDescriptor( descriptor ) {}

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~ScopedDescriptor() { if( pDescriptor >= 0 ) close( pDescriptor ); }

      //------------------------------------------------------------------------
      //! Release the descriptor being held
      //------------------------------------------------------------------------
      int Release()
      {
        int desc = pDescriptor;
        pDescriptor = -1;
        return desc;
      }

      //------------------------------------------------------------------------
      //! Get the descriptor
      //------------------------------------------------------------------------
      int GetDescriptor()
      {
        return pDescriptor;
      }

    private:
      int pDescriptor;
  };
}

#endif // __XRD_CL_UTILS_HH__
