//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_XROOTD_RESPONSES_HH__
#define __XRD_CL_XROOTD_RESPONSES_HH__

#include "XrdCl/XrdClBuffer.hh"
#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClAnyObject.hh"
#include "XProtocol/XProtocol.hh"
#include <string>
#include <vector>
#include <list>

namespace XrdClient
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
      typedef LocationList::iterator       LocationIterator;

      //------------------------------------------------------------------------
      //! Iterator over locations
      //------------------------------------------------------------------------
      typedef LocationList::const_iterator LocationConstIterator;

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      LocationInfo( const char *data = "" );

      //------------------------------------------------------------------------
      //! Get number of locations
      //------------------------------------------------------------------------
      uint32_t GetSize() const
      {
        return pLocations.size();
      }

      //------------------------------------------------------------------------
      //! Get the location begin iterator
      //------------------------------------------------------------------------
      LocationIterator Begin()
      {
        return pLocations.begin();
      }

      //------------------------------------------------------------------------
      //! Get the location begin iterator
      //------------------------------------------------------------------------
      LocationConstIterator Begin() const
      {
        return pLocations.begin();
      }

      //------------------------------------------------------------------------
      //! Get the location end iterator
      //------------------------------------------------------------------------
      LocationIterator End()
      {
        return pLocations.end();
      }

      //------------------------------------------------------------------------
      //! Get the location end iterator
      //------------------------------------------------------------------------
      LocationConstIterator End() const
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

    private:
      void ParseServerResponse( const char *data );
      void ProcessLocation( std::string &location );
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

    private:
      std::string pMessage;
  };

  //----------------------------------------------------------------------------
  //! Directory list
  //----------------------------------------------------------------------------
  typedef std::list<std::string> DirectoryListIfno;

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
        AttrSuper = kXR_attrSuper,   //!< Supervisor attribute
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
  //! Stat info
  //----------------------------------------------------------------------------
  class StatInfo
  {
    public:
      //------------------------------------------------------------------------
      //! Possible statistics types represented by this object
      //------------------------------------------------------------------------
      enum StatType
      {
        Invalid,            //!< Invalid stat info
        Object,             //!< File/directory related stats
        VFS,                //!< Virtual file system related stats
      };

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
                                      //!< successfuly closed
        Readable     = kXR_readable,  //!< Read access is alowed
        Writable     = kXR_writable,  //!< Write access is allowed
      };

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      StatInfo( const char *data );

      //------------------------------------------------------------------------
      //! Get type
      //------------------------------------------------------------------------
      const StatType GetType() const
      {
        return pType;
      }

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
      //! Get number of nodes that can provide read/write space
      //------------------------------------------------------------------------
      uint64_t GetNodesRW() const
      {
        return pNodesRW;
      }

      //------------------------------------------------------------------------
      //! Get size of the largest contiguous aread of free r/w space (in MB)
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
      //! Get size of the largest contiguous aread of free staging space (in MB)
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

    private:

      //------------------------------------------------------------------------
      // Parse the stat info returned by the server
      //------------------------------------------------------------------------
      void ParseServerResponse( const char *data  );
      void ProcessObjectStat( std::vector<std::string> &chunks  );
      void ProcessVFSStat( std::vector<std::string> &chunks  );

      StatType   pType;

      //------------------------------------------------------------------------
      // Normal stat
      //------------------------------------------------------------------------
      std::string pId;
      uint64_t    pSize;
      uint32_t    pFlags;
      uint64_t    pModTime;

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
  //! Handle an async response
  //----------------------------------------------------------------------------
  class ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Called when a response to associated request arrives or an error
      //! occurs
      //!
      //! @param status   status of the request
      //! @param response an object associated with the response
      //!                 (request dependent)
      //------------------------------------------------------------------------
      virtual void HandleResponse( XRootDStatus *status,
                                   AnyObject    *response ) = 0;
  };
}

#endif // __XRD_CL_XROOTD_RESPONSES_HH__
