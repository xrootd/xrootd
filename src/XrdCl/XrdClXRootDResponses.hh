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

#ifndef __XRD_CL_XROOTD_RESPONSES_HH__
#define __XRD_CL_XROOTD_RESPONSES_HH__

#include "XrdCl/XrdClBuffer.hh"
#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClAnyObject.hh"
#include "XProtocol/XProtocol.hh"
#include <string>
#include <vector>
#include <list>
#include <ctime>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Path location info
  //----------------------------------------------------------------------------
  class LocationInfo
  {
    public:
      //------------------------------------------------------------------------
      //! Describes the node type and file status for a given location
      //------------------------------------------------------------------------
      enum LocationType
      {
        ManagerOnline,   //!< manager node where the file is online
        ManagerPending,  //!< manager node where the file is pending to be online
        ServerOnline,    //!< server node where the file is online
        ServerPending    //!< server node where the file is pending to be online
      };

      //------------------------------------------------------------------------
      //! Describes the allowed access type for the file at given location
      //------------------------------------------------------------------------
      enum AccessType
      {
        Read,            //!< read access is allowed
        ReadWrite        //!< write access is allowed
      };

      //------------------------------------------------------------------------
      //! Location
      //------------------------------------------------------------------------
      class Location
      {
        public:

          //--------------------------------------------------------------------
          //! Constructor
          //--------------------------------------------------------------------
          Location( const std::string  &address,
                    LocationType        type,
                    AccessType          access ):
            pAddress( address ),
            pType( type ),
            pAccess( access ) {}

          //--------------------------------------------------------------------
          //! Get address
          //--------------------------------------------------------------------
          const std::string &GetAddress() const
          {
            return pAddress;
          }

          //--------------------------------------------------------------------
          //! Get location type
          //--------------------------------------------------------------------
          LocationType GetType() const
          {
            return pType;
          }

          //--------------------------------------------------------------------
          //! Get access type
          //--------------------------------------------------------------------
          AccessType GetAccessType() const
          {
            return pAccess;
          }

          //--------------------------------------------------------------------
          //! Check whether the location is a server
          //--------------------------------------------------------------------
          bool IsServer() const
          {
            return pType == ServerOnline || pType == ServerPending;
          }

          //--------------------------------------------------------------------
          //! Check whether the location is a manager
          //--------------------------------------------------------------------
          bool IsManager() const
          {
            return pType == ManagerOnline || pType == ManagerPending;
          }

        private:
          std::string  pAddress;
          LocationType pType;
          AccessType   pAccess;
      };

      //------------------------------------------------------------------------
      //! List of locations
      //------------------------------------------------------------------------
      typedef std::vector<Location>        LocationList;

      //------------------------------------------------------------------------
      //! Iterator over locations
      //------------------------------------------------------------------------
      typedef LocationList::iterator       Iterator;

      //------------------------------------------------------------------------
      //! Iterator over locations
      //------------------------------------------------------------------------
      typedef LocationList::const_iterator ConstIterator;

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      LocationInfo();

      //------------------------------------------------------------------------
      //! Get number of locations
      //------------------------------------------------------------------------
      uint32_t GetSize() const
      {
        return pLocations.size();
      }

      //------------------------------------------------------------------------
      //! Get the location at index
      //------------------------------------------------------------------------
      Location &At( uint32_t index )
      {
        return pLocations[index];
      }

      //------------------------------------------------------------------------
      //! Get the location begin iterator
      //------------------------------------------------------------------------
      Iterator Begin()
      {
        return pLocations.begin();
      }

      //------------------------------------------------------------------------
      //! Get the location begin iterator
      //------------------------------------------------------------------------
      ConstIterator Begin() const
      {
        return pLocations.begin();
      }

      //------------------------------------------------------------------------
      //! Get the location end iterator
      //------------------------------------------------------------------------
      Iterator End()
      {
        return pLocations.end();
      }

      //------------------------------------------------------------------------
      //! Get the location end iterator
      //------------------------------------------------------------------------
      ConstIterator End() const
      {
        return pLocations.end();
      }

      //------------------------------------------------------------------------
      //! Add a location
      //------------------------------------------------------------------------
      void Add( const Location &location )
      {
        pLocations.push_back( location );
      }

      //------------------------------------------------------------------------
      //! Parse server response and fill up the object
      //------------------------------------------------------------------------
      bool ParseServerResponse( const char *data );

    private:
      bool ProcessLocation( std::string &location );
      LocationList pLocations;
  };

  //----------------------------------------------------------------------------
  //! Request status
  //----------------------------------------------------------------------------
  class XRootDStatus: public Status
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      XRootDStatus( uint16_t           st      = 0,
                    uint16_t           code    = 0,
                    uint32_t           errN    = 0,
                    const std::string &message = "" ):
        Status( st, code, errN ),
        pMessage( message ) {}

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      XRootDStatus( const Status      &st,
                    const std::string &message = "" ):
        Status( st ),
        pMessage( message ) {}

      //------------------------------------------------------------------------
      //! Get error message
      //------------------------------------------------------------------------
      const std::string &GetErrorMessage() const
      {
        return pMessage;
      }

      //------------------------------------------------------------------------
      //! Set the error message
      //------------------------------------------------------------------------
      void SetErrorMessage( const std::string &message )
      {
        pMessage = message;
      }

      //------------------------------------------------------------------------
      //! Convert to string
      //------------------------------------------------------------------------
      std::string ToStr() const
      {
        if( code == errErrorResponse )
        {
          std::ostringstream o;
          o << "[ERROR] Server responded with an error: [" << errNo << "] ";
          o << pMessage << std::endl;
          return o.str();
        }
        std::string str = ToString();
        if( !pMessage.empty() )
          str += ": " + pMessage;
        return str;
      }

    private:
      std::string pMessage;
  };

  //----------------------------------------------------------------------------
  //! Binary buffer
  //----------------------------------------------------------------------------
  typedef Buffer BinaryDataInfo;

  //----------------------------------------------------------------------------
  //! Protocol response
  //----------------------------------------------------------------------------
  class ProtocolInfo
  {
    public:
      //------------------------------------------------------------------------
      //! Types of XRootD servers
      //------------------------------------------------------------------------
      enum HostTypes
      {
        IsManager = kXR_isManager,   //!< Manager
        IsServer  = kXR_isServer,    //!< Data server
        AttrMeta  = kXR_attrMeta,    //!< Meta attribute
        AttrProxy = kXR_attrProxy,   //!< Proxy attribute
        AttrSuper = kXR_attrSuper    //!< Supervisor attribute
      };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      ProtocolInfo( uint32_t version, uint32_t hostInfo ):
        pVersion( version ), pHostInfo( hostInfo ) {}

      //------------------------------------------------------------------------
      //! Get version info
      //------------------------------------------------------------------------
      uint32_t GetVersion() const
      {
        return pVersion;
      }

      //------------------------------------------------------------------------
      //! Get host info
      //------------------------------------------------------------------------
      uint32_t GetHostInfo() const
      {
        return pHostInfo;
      }

      //------------------------------------------------------------------------
      //! Test host info flags
      //------------------------------------------------------------------------
      bool TestHostInfo( uint32_t flags )
      {
        return pHostInfo & flags;
      }

    private:
      uint32_t pVersion;
      uint32_t pHostInfo;
  };

  //----------------------------------------------------------------------------
  //! Object stat info
  //----------------------------------------------------------------------------
  class StatInfo
  {
    public:
      //------------------------------------------------------------------------
      //! Flags
      //------------------------------------------------------------------------
      enum Flags
      {
        XBitSet      = kXR_xset,      //!< Executable/searchable bit set
        IsDir        = kXR_isDir,     //!< This is a directory
        Other        = kXR_other,     //!< Neither a file nor a directory
        Offline      = kXR_offline,   //!< File is not online (ie. on disk)
        POSCPending  = kXR_poscpend,  //!< File opened with POST flag, not yet
                                      //!< successfully closed
        IsReadable   = kXR_readable,  //!< Read access is allowed
        IsWritable   = kXR_writable,  //!< Write access is allowed
        BackUpExists = kXR_bkpexist   //!< Back up copy exists
      };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      StatInfo();

      //------------------------------------------------------------------------
      //! Get id
      //------------------------------------------------------------------------
      const std::string GetId() const
      {
        return pId;
      }

      //------------------------------------------------------------------------
      //! Get size (in bytes)
      //------------------------------------------------------------------------
      uint64_t GetSize() const
      {
        return pSize;
      }

      //------------------------------------------------------------------------
      //! Get flags
      //------------------------------------------------------------------------
      uint32_t GetFlags() const
      {
        return pFlags;
      }

      //------------------------------------------------------------------------
      //! Test flags
      //------------------------------------------------------------------------
      bool TestFlags( uint32_t flags ) const
      {
        return pFlags & flags;
      }

      //------------------------------------------------------------------------
      //! Get modification time (in seconds since epoch)
      //------------------------------------------------------------------------
      uint64_t GetModTime() const
      {
        return pModTime;
      }

      //------------------------------------------------------------------------
      //! Get modification time
      //------------------------------------------------------------------------
      std::string GetModTimeAsString() const
      {
        char ts[256];
        time_t modTime = pModTime;
        tm *t = gmtime( &modTime );
        strftime( ts, 255, "%F %T", t );
        return ts;
      }

      //------------------------------------------------------------------------
      //! Parse server response and fill up the object
      //------------------------------------------------------------------------
      bool ParseServerResponse( const char *data );

    private:

      //------------------------------------------------------------------------
      // Normal stat
      //------------------------------------------------------------------------
      std::string pId;
      uint64_t    pSize;
      uint32_t    pFlags;
      uint64_t    pModTime;
  };

  //----------------------------------------------------------------------------
  //! VFS stat info
  //----------------------------------------------------------------------------
  class StatInfoVFS
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      StatInfoVFS();

      //------------------------------------------------------------------------
      //! Get number of nodes that can provide read/write space
      //------------------------------------------------------------------------
      uint64_t GetNodesRW() const
      {
        return pNodesRW;
      }

      //------------------------------------------------------------------------
      //! Get size of the largest contiguous area of free r/w space (in MB)
      //------------------------------------------------------------------------
      uint64_t GetFreeRW() const
      {
        return pFreeRW;
      }

      //------------------------------------------------------------------------
      //! Get percentage of the partition utilization represented by FreeRW
      //------------------------------------------------------------------------
      uint8_t GetUtilizationRW() const
      {
        return pUtilizationRW;
      }

      //------------------------------------------------------------------------
      //! Get number of nodes that can provide staging space
      //------------------------------------------------------------------------
      uint64_t GetNodesStaging() const
      {
        return pNodesStaging;
      }

      //------------------------------------------------------------------------
      //! Get size of the largest contiguous area of free staging space (in MB)
      //------------------------------------------------------------------------
      uint64_t GetFreeStaging() const
      {
        return pFreeStaging;
      }

      //------------------------------------------------------------------------
      //! Get percentage of the partition utilization represented by FreeStaging
      //------------------------------------------------------------------------
      uint8_t GetUtilizationStaging() const
      {
        return pUtilizationStaging;
      }

      //------------------------------------------------------------------------
      //! Parse server response and fill up the object
      //------------------------------------------------------------------------
      bool ParseServerResponse( const char *data );

    private:

      //------------------------------------------------------------------------
      // kXR_vfs stat
      //------------------------------------------------------------------------
      uint64_t    pNodesRW;
      uint64_t    pFreeRW;
      uint32_t    pUtilizationRW;
      uint64_t    pNodesStaging;
      uint64_t    pFreeStaging;
      uint32_t    pUtilizationStaging;
  };

  //----------------------------------------------------------------------------
  //! Directory list
  //----------------------------------------------------------------------------
  class DirectoryList
  {
    public:
      //------------------------------------------------------------------------
      //! Directory entry
      //------------------------------------------------------------------------
      class ListEntry
      {
        public:
          //--------------------------------------------------------------------
          //! Constructor
          //--------------------------------------------------------------------
          ListEntry( const std::string &hostAddress,
                     const std::string &name,
                     StatInfo          *statInfo = 0):
            pHostAddress( hostAddress ),
            pName( name ),
            pStatInfo( statInfo )
          {}

          //--------------------------------------------------------------------
          //! Destructor
          //--------------------------------------------------------------------
          ~ListEntry()
          {
            delete pStatInfo;
          }

          //--------------------------------------------------------------------
          //! Get host address
          //--------------------------------------------------------------------
          const std::string &GetHostAddress() const
          {
            return pHostAddress;
          }

          //--------------------------------------------------------------------
          //! Get file name
          //--------------------------------------------------------------------
          const std::string &GetName() const
          {
            return pName;
          }

          //--------------------------------------------------------------------
          //! Get the stat info object
          //--------------------------------------------------------------------
          StatInfo *GetStatInfo()
          {
            return pStatInfo;
          }

          //--------------------------------------------------------------------
          //! Get the stat info object
          //--------------------------------------------------------------------
          const StatInfo *GetStatInfo() const
          {
            return pStatInfo;
          }

          //--------------------------------------------------------------------
          //! Set the stat info object (and transfer the ownership)
          //--------------------------------------------------------------------
          void SetStatInfo( StatInfo *info )
          {
            pStatInfo = info;
          }

        private:
          std::string  pHostAddress;
          std::string  pName;
          StatInfo    *pStatInfo;
      };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      DirectoryList();

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~DirectoryList();

      //------------------------------------------------------------------------
      //! Directory listing
      //------------------------------------------------------------------------
      typedef std::vector<ListEntry*>  DirList;

      //------------------------------------------------------------------------
      //! Directory listing iterator
      //------------------------------------------------------------------------
      typedef DirList::iterator       Iterator;

      //------------------------------------------------------------------------
      //! Directory listing const iterator
      //------------------------------------------------------------------------
      typedef DirList::const_iterator ConstIterator;

      //------------------------------------------------------------------------
      //! Add an entry to the list - takes ownership
      //------------------------------------------------------------------------
      void Add( ListEntry *entry )
      {
        pDirList.push_back( entry );
      }

      //------------------------------------------------------------------------
      //! Get an entry at given index
      //------------------------------------------------------------------------
      ListEntry *At( uint32_t index )
      {
        return pDirList[index];
      }

      //------------------------------------------------------------------------
      //! Get the begin iterator
      //------------------------------------------------------------------------
      Iterator Begin()
      {
        return pDirList.begin();
      }

      //------------------------------------------------------------------------
      //! Get the begin iterator
      //------------------------------------------------------------------------
      ConstIterator Begin() const
      {
        return pDirList.begin();
      }

      //------------------------------------------------------------------------
      //! Get the end iterator
      //------------------------------------------------------------------------
      Iterator End()
      {
        return pDirList.end();
      }

      //------------------------------------------------------------------------
      //! Get the end iterator
      //------------------------------------------------------------------------
      ConstIterator End() const
      {
        return pDirList.end();
      }

      //------------------------------------------------------------------------
      //! Get the size of the listing
      //------------------------------------------------------------------------
      uint32_t GetSize() const
      {
        return pDirList.size();
      }

      //------------------------------------------------------------------------
      //! Get parent directory name
      //------------------------------------------------------------------------
      const std::string &GetParentName() const
      {
        return pParent;
      }

      //------------------------------------------------------------------------
      //! Set name of the parent directory
      //------------------------------------------------------------------------
      void SetParentName( const std::string &parent )
      {
        pParent = parent;
        if( !pParent.empty() && pParent[pParent.length()-1] != '/' )
          pParent += "/";
      }

      //------------------------------------------------------------------------
      //! Parse server response and fill up the object
      //------------------------------------------------------------------------
      bool ParseServerResponse( const std::string &hostId,
                                const char *data );

    private:
      DirList     pDirList;
      std::string pParent;
  };

  //----------------------------------------------------------------------------
  //! Information returned by file open operation
  //----------------------------------------------------------------------------
  class OpenInfo
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      OpenInfo( const uint8_t *fileHandle,
                uint64_t       sessionId,
                StatInfo *statInfo        = 0 ):
        pSessionId(sessionId), pStatInfo( statInfo )
      {
        memcpy( pFileHandle, fileHandle, 4 );
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~OpenInfo()
      {
        delete pStatInfo;
      }

      //------------------------------------------------------------------------
      //! Get the file handle (4bytes)
      //------------------------------------------------------------------------
      void GetFileHandle( uint8_t *fileHandle ) const
      {
        memcpy( fileHandle, pFileHandle, 4 );
      }

      //------------------------------------------------------------------------
      //! Get the stat info
      //------------------------------------------------------------------------
      const StatInfo *GetStatInfo() const
      {
        return pStatInfo;
      }

      //------------------------------------------------------------------------
      // Get session ID
      //------------------------------------------------------------------------
      uint64_t GetSessionId() const
      {
        return pSessionId;
      }

    private:
      uint8_t   pFileHandle[4];
      uint64_t  pSessionId;
      StatInfo *pStatInfo;
  };

  //----------------------------------------------------------------------------
  //! Describe a data chunk for vector read
  //----------------------------------------------------------------------------
  struct ChunkInfo
  {
    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    ChunkInfo( uint64_t off = 0, uint32_t len = 0, void *buff = 0 ):
      offset( off ), length( len ), buffer(buff) {}

    uint64_t  offset; //! offset in the file
    uint32_t  length; //! length of the chunk
    void     *buffer; //! optional buffer pointer
  };

  //----------------------------------------------------------------------------
  //! List of chunks
  //----------------------------------------------------------------------------
  typedef std::vector<ChunkInfo> ChunkList;

  //----------------------------------------------------------------------------
  //! Vector read info
  //----------------------------------------------------------------------------
  class VectorReadInfo
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      VectorReadInfo(): pSize( 0 ) {}

      //------------------------------------------------------------------------
      //! Get Size
      //------------------------------------------------------------------------
      uint32_t GetSize() const
      {
        return pSize;
      }

      //------------------------------------------------------------------------
      //! Set size
      //------------------------------------------------------------------------
      void SetSize( uint32_t size )
      {
        pSize = size;
      }

      //------------------------------------------------------------------------
      //! Get chunks
      //------------------------------------------------------------------------
      ChunkList &GetChunks()
      {
        return pChunks;
      }

      //------------------------------------------------------------------------
      //! Get chunks
      //------------------------------------------------------------------------
      const ChunkList &GetChunks() const
      {
        return pChunks;
      }

    private:
      ChunkList pChunks;
      uint32_t  pSize;
  };

  //----------------------------------------------------------------------------
  // List of URLs
  //----------------------------------------------------------------------------
  struct HostInfo
  {
    HostInfo():
      flags(0), protocol(0), loadBalancer(false) {}
    HostInfo( const URL &u, bool lb = false ):
      flags(0), protocol(0), loadBalancer(lb), url(u) {}
    uint32_t flags;        //!< Host type
    uint32_t protocol;     //!< Version of the protocol the host is speaking
    bool     loadBalancer; //!< Was the host used as a load balancer
    URL      url;          //!< URL of the host
  };

  typedef std::vector<HostInfo> HostList;

  //----------------------------------------------------------------------------
  //! Handle an async response
  //----------------------------------------------------------------------------
  class ResponseHandler
  {
    public:
      virtual ~ResponseHandler() {}

      //------------------------------------------------------------------------
      //! Called when a response to associated request arrives or an error
      //! occurs
      //!
      //! @param status   status of the request
      //! @param response an object associated with the response
      //!                 (request dependent)
      //! @param urlList  list of hosts the request was redirected to
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XRootDStatus *status,
                                            AnyObject    *response,
                                            HostList     *hostList )
      {
        delete hostList;
        HandleResponse( status, response );
      }

      //------------------------------------------------------------------------
      //! Called when a response to associated request arrives or an error
      //! occurs
      //!
      //! @param status   status of the request
      //! @param response an object associated with the response
      //!                 (request dependent)
      //------------------------------------------------------------------------
      virtual void HandleResponse( XRootDStatus *status,
                                   AnyObject    *response )
      {
        (void)status; (void)response;
      }
  };
}

#endif // __XRD_CL_XROOTD_RESPONSES_HH__
