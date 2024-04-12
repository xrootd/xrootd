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
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOucTUtils.hh"

#include <sys/time.h>

#ifdef __linux__
#include <sys/fsuid.h>
#endif

namespace XrdCl
{
  class Message;

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
          /*
           * This was done in order to not duplicate code as this method
           * is also used in XrdHttp
           * TODO: Maybe this method could be collapsed
           * to avoid this middle-man call here
           */
          XrdOucTUtils::splitString(result,input,delimiter);
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
                                             const URL         &url );

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
                                    time_t             timeout = 0 );

      //------------------------------------------------------------------------
      //! Check if peer supports tpc / tpc lite
      //!
      //! @return : suDone if TPC lite is supported, suPartial if plain TPC is
      //!           supported, stError otherwise
      //------------------------------------------------------------------------
      static XRootDStatus CheckTPCLite( const std::string &server,
                                        time_t             timeout = 0 );

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
      //! Process a config directory and return key-value pairs
      //------------------------------------------------------------------------
      static Status ProcessConfigDir( std::map<std::string, std::string> &config,
                                      const std::string                  &dir );

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

      //------------------------------------------------------------------------
      //! Get supported checksum types for given URL
      //------------------------------------------------------------------------
      static std::vector<std::string> GetSupportedCheckSums( const XrdCl::URL &url );

      //------------------------------------------------------------------------
      //! Automatically infer the right checksum type
      //!
      //! @param source      : source URL
      //! @param destination : destination URL
      //! @param zip         : true if the source file is being extracted from
      //!                      a ZIP archive, false otherwise
      //! @return            : checksum type
      //------------------------------------------------------------------------
      static std::string InferChecksumType( const XrdCl::URL &source,
                                            const XrdCl::URL &destination,
                                            bool              zip = false );

      //------------------------------------------------------------------------
      //! Check if this client can support given EC redirect
      //------------------------------------------------------------------------
      static bool CheckEC( const Message *req, const URL &url );

      //------------------------------------------------------------------------
      //! Get protocol version of the given server
      //! @param url     : URL pointing to the server
      //! @param protver : protocol version (output parameter)
      //! @return        : operation status
      //------------------------------------------------------------------------
      inline static XrdCl::XRootDStatus GetProtocolVersion( const XrdCl::URL url, int &protver )
      {
        XrdCl::AnyObject  qryResult;
        XrdCl::XRootDStatus st = XrdCl::DefaultEnv::GetPostMaster()->
            QueryTransport( url, XrdCl::XRootDQuery::ProtocolVersion, qryResult );
        if( !st.IsOK() ) return st;
        int *tmp = 0;
        qryResult.Get( tmp );
        if( !tmp )
          return st;
        protver = *tmp;
        delete tmp;
        return XrdCl::XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Check if given server supports extended file attributes
      //! @param url : URL pointing to the server
      //! @return    : true if yes, false otherwise
      //------------------------------------------------------------------------
      inline static bool HasXAttr( const XrdCl::URL &url )
      {
        if( url.IsLocalFile() ) return true;
        int protver = 0;
        auto st = GetProtocolVersion( url, protver );
        if( !st.IsOK() ) return false;
        return protver >= kXR_PROTXATTVERSION;
      }

      //------------------------------------------------------------------------
      //! Check if given server supports pgread/pgwrite
      //! @param url : URL pointing to the server
      //! @return    : true if yes, false otherwise
      //------------------------------------------------------------------------
      inline static bool HasPgRW( const XrdCl::URL &url )
      {
        if( url.IsLocalFile() ) return false;
        int protver = 0;
        auto st = GetProtocolVersion( url, protver );
        if( !st.IsOK() ) return false;
        return protver >= kXR_PROTPGRWVERSION;
      }

      //------------------------------------------------------------------------
      //! Split chunks in a ChunkList into one or more ChunkLists
      //! @param listsvec        : output vector of ChunkLists
      //! @param chunks          : input ChunkLisits
      //! @param maxcs           : maximum size of a ChunkInfo in output
      //! @param maxc            : maximum number of ChunkInfo in each ChunkList
      //------------------------------------------------------------------------
      static void SplitChunks( std::vector<ChunkList> &listsvec,
                               const ChunkList        &chunks,
                               const uint32_t          maxcs,
                               const size_t            maxc );
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

#ifdef __linux__
  //----------------------------------------------------------------------------
  //! Scoped fsuid and fsgid setter, restoring original values on destruction
  //----------------------------------------------------------------------------
  class ScopedFsUidSetter
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      ScopedFsUidSetter(uid_t fsuid, gid_t fsgid, const std::string &streamName)
      : pFsUid(fsuid), pFsGid(fsgid), pStreamName(streamName)
      {
        pOk = true;
        pPrevFsUid = -1;
        pPrevFsGid = -1;

        //----------------------------------------------------------------------
        //! Set fsuid
        //----------------------------------------------------------------------
        if(pFsUid >= 0) {
          pPrevFsUid = setfsuid(pFsUid);

          if(setfsuid(pFsUid) != pFsUid) {
            pOk = false;
            return;
          }
        }

        //----------------------------------------------------------------------
        //! Set fsgid
        //----------------------------------------------------------------------
        if(pFsGid >= 0) {
          pPrevFsGid = setfsgid(pFsGid);

          if(setfsgid(pFsGid) != pFsGid) {
            pOk = false;
            return;
          }
        }
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~ScopedFsUidSetter() {
        Log *log = DefaultEnv::GetLog();

        if(pPrevFsUid >= 0) {
          int retcode = setfsuid(pPrevFsUid);
          log->Dump(XRootDTransportMsg, "[%s] Restored fsuid from %d to %d", pStreamName.c_str(), retcode, pPrevFsUid);
        }

        if(pPrevFsGid >= 0) {
          int retcode = setfsgid(pPrevFsGid);
          log->Dump(XRootDTransportMsg, "[%s] Restored fsgid from %d to %d", pStreamName.c_str(), retcode, pPrevFsGid);
        }
      }

      bool IsOk() const {
        return pOk;
      }

    private:
      int pFsUid;
      int pFsGid;

      const std::string &pStreamName;

      int pPrevFsUid;
      int pPrevFsGid;

      bool pOk;
  };
#endif

}

#endif // __XRD_CL_UTILS_HH__
