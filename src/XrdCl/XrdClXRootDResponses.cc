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

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClUtils.hh"
#include <cstdlib>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // LocationInfo constructor
  //----------------------------------------------------------------------------
  LocationInfo::LocationInfo()
  {
  }

  //----------------------------------------------------------------------------
  // Parse the server location response
  //----------------------------------------------------------------------------
  bool LocationInfo::ParseServerResponse( const char *data )
  {
    if( !data || strlen( data ) == 0 )
      return false;

    std::vector<std::string>           locations;
    std::vector<std::string>::iterator it;
    Utils::splitString( locations, data, " " );
    for( it = locations.begin(); it != locations.end(); ++it )
      if( ProcessLocation( *it ) == false )
        return false;
    return true;
  }

  //----------------------------------------------------------------------------
  // Process location
  //----------------------------------------------------------------------------
  bool LocationInfo::ProcessLocation( std::string &location )
  {
    if( location.length() < 5 )
      return false;

    //--------------------------------------------------------------------------
    // Decode location type
    //--------------------------------------------------------------------------
    LocationInfo::LocationType type;
    switch( location[0] )
    {
      case 'M':
        type = LocationInfo::ManagerOnline;
        break;
      case 'm':
        type = LocationInfo::ManagerPending;
        break;
      case 'S':
        type = LocationInfo::ServerOnline;
        break;
      case 's':
        type = LocationInfo::ServerPending;
        break;
      default:
        return false;
    }

    //--------------------------------------------------------------------------
    // Decode access type
    //--------------------------------------------------------------------------
    LocationInfo::AccessType access;
    switch( location[1] )
    {
      case 'r':
        access = LocationInfo::Read;
        break;
      case 'w':
        access = LocationInfo::ReadWrite;
        break;
      default:
        return false;
    }

    //--------------------------------------------------------------------------
    // Push the location info
    //--------------------------------------------------------------------------
    pLocations.push_back( Location( location.substr(2), type, access ) );

    return true;
  }

  //----------------------------------------------------------------------------
  // StatInfo implementation
  //----------------------------------------------------------------------------
  struct StatInfoImpl
  {
      StatInfoImpl() : pSize( 0 ), pFlags( 0 ), pModifyTime( 0 ),
                       pChangeTime( 0 ), pAccessTime( 0 ),
                       pExtended( false ), pHasCksum( false )
      {
      }

      StatInfoImpl( const StatInfoImpl & pimpl ) : pId( pimpl.pId ),
                                                   pSize( pimpl.pSize ),
                                                   pFlags( pimpl.pFlags ),
                                                   pModifyTime( pimpl.pModifyTime ),
                                                   pChangeTime( pimpl.pChangeTime ),
                                                   pAccessTime( pimpl.pAccessTime ),
                                                   pMode( pimpl.pMode ),
                                                   pOwner( pimpl.pOwner ),
                                                   pGroup( pimpl.pGroup ),
                                                   pExtended( pimpl.pExtended ),
                                                   pHasCksum( pimpl.pHasCksum )
      {
      }

      //------------------------------------------------------------------------
      // Parse the stat info returned by the server
      //------------------------------------------------------------------------
      bool ParseServerResponse( const char *data )
      {
        if( !data || strlen( data ) == 0 )
          return false;

        std::vector<std::string> chunks;
        Utils::splitString( chunks, data, " " );

        if( chunks.size() < 4 )
          return false;

        pId = chunks[0];

        char *result;
        pSize = ::strtoll( chunks[1].c_str(), &result, 0 );
        if( *result != 0 )
        {
          pSize = 0;
          return false;
        }

        pFlags = ::strtol( chunks[2].c_str(), &result, 0 );
        if( *result != 0 )
        {
          pFlags = 0;
          return false;
        }

        pModifyTime = ::strtoll( chunks[3].c_str(), &result, 0 );
        if( *result != 0 )
        {
          pModifyTime = 0;
          return false;
        }

        if( chunks.size() >= 9 )
        {
          pChangeTime = ::strtoll( chunks[4].c_str(), &result, 0 );
          if( *result != 0 )
          {
            pChangeTime = 0;
            return false;
          }

          pAccessTime = ::strtoll( chunks[5].c_str(), &result, 0 );
          if( *result != 0 )
          {
            pAccessTime = 0;
            return false;
          }

          // we are expecting at least 4 characters, e.g.: 0644
          if( chunks[6].size() < 4 ) return false;
          pMode  = chunks[6];

          pOwner = chunks[7];
          pGroup = chunks[8];

          pExtended = true;
        }

        // after the extended stat information, we might have the checksum
        if( chunks.size() >= 10 )
        {
          if( ( chunks[9] == "[" ) && ( chunks[11] == "]" ) )
          {
            pHasCksum = true;
            pCksum     = chunks[10];
          }
        }

        return true;
      }

      std::string pId;
      uint64_t    pSize;
      uint32_t    pFlags;
      uint64_t    pModifyTime;
      uint64_t    pChangeTime;
      uint64_t    pAccessTime;
      std::string pMode;
      std::string pOwner;
      std::string pGroup;

      bool        pExtended;
      bool        pHasCksum;
      std::string pCksum;
  };

  //----------------------------------------------------------------------------
  // StatInfo constructor
  //----------------------------------------------------------------------------
  StatInfo::StatInfo() : pImpl( new StatInfoImpl() )
  {
  }

  //------------------------------------------------------------------------
  // Constructor
  //------------------------------------------------------------------------
  StatInfo::StatInfo( const std::string &id, uint64_t size, uint32_t flags,
                      uint64_t modTime ) : pImpl( new StatInfoImpl() )

  {
    pImpl->pId         = id;
    pImpl->pSize       = size;
    pImpl->pFlags      = flags;
    pImpl->pModifyTime = modTime;
  }

  //------------------------------------------------------------------------
  // Copy constructor
  //------------------------------------------------------------------------
  StatInfo::StatInfo( const StatInfo &info ) : pImpl( new StatInfoImpl( *info.pImpl) )
  {
  }

  //------------------------------------------------------------------------
  // Destructor (it can be only defined after StatInfoImpl is defined!!!)
  //------------------------------------------------------------------------
  StatInfo::~StatInfo() = default;

  //----------------------------------------------------------------------------
  // Parse the stat info returned by the server
  //----------------------------------------------------------------------------
  bool StatInfo::ParseServerResponse( const char *data )
  {
    return pImpl->ParseServerResponse( data );
  }

  //------------------------------------------------------------------------
  //! Get id
  //------------------------------------------------------------------------
  const std::string& StatInfo::GetId() const
  {
    return pImpl->pId;
  }

  //------------------------------------------------------------------------
  //! Get size (in bytes)
  //------------------------------------------------------------------------
  uint64_t StatInfo::GetSize() const
  {
    return pImpl->pSize;
  }

  //------------------------------------------------------------------------
  //! Set size
  //------------------------------------------------------------------------
  void StatInfo::SetSize( uint64_t size )
  {
    pImpl->pSize = size;
  }

  //------------------------------------------------------------------------
  //! Get flags
  //------------------------------------------------------------------------
  uint32_t StatInfo::GetFlags() const
  {
    return pImpl->pFlags;
  }

  //------------------------------------------------------------------------
  //! Set flags
  //------------------------------------------------------------------------
  void StatInfo::SetFlags( uint32_t flags )
  {
    pImpl->pFlags = flags;
  }

  //------------------------------------------------------------------------
  //! Test flags
  //------------------------------------------------------------------------
  bool StatInfo::TestFlags( uint32_t flags ) const
  {
    return pImpl->pFlags & flags;
  }

  //------------------------------------------------------------------------
  //! Get modification time (in seconds since epoch)
  //------------------------------------------------------------------------
  uint64_t StatInfo::GetModTime() const
  {
    return pImpl->pModifyTime;
  }

  //------------------------------------------------------------------------
  //! Get modification time
  //------------------------------------------------------------------------
  std::string StatInfo::GetModTimeAsString() const
  {
    return TimeToString( pImpl->pModifyTime );
  }

  //------------------------------------------------------------------------
  //! Get change time (in seconds since epoch)
  //------------------------------------------------------------------------
  uint64_t StatInfo::GetChangeTime() const
  {
    return pImpl->pChangeTime;
  }

  //------------------------------------------------------------------------
  //! Get change time
  //------------------------------------------------------------------------
  std::string StatInfo::GetChangeTimeAsString() const
  {
    return TimeToString( pImpl->pChangeTime );
  }

  //------------------------------------------------------------------------
  //! Get change time (in seconds since epoch)
  //------------------------------------------------------------------------
  uint64_t StatInfo::GetAccessTime() const
  {
    return pImpl->pAccessTime;
  }

  //------------------------------------------------------------------------
  //! Get change time
  //------------------------------------------------------------------------
  std::string StatInfo::GetAccessTimeAsString() const
  {
    return TimeToString( pImpl->pAccessTime );
  }

  //------------------------------------------------------------------------
  //! Get mode
  //------------------------------------------------------------------------
  const std::string& StatInfo::GetModeAsString() const
  {
    return pImpl->pMode;
  }

  //------------------------------------------------------------------------
  //! Get mode
  //------------------------------------------------------------------------
  const std::string StatInfo::GetModeAsOctString() const
  {
    std::string ret;
    ret.reserve( 9 );

    // we care about 3 last digits
    size_t size = pImpl->pMode.size();

    uint8_t oct = pImpl->pMode[size - 3] - '0';
    OctToString( oct, ret );

    oct = pImpl->pMode[size - 2] - '0';
    OctToString( oct, ret );

    oct = pImpl->pMode[size - 1] - '0';
    OctToString( oct, ret );

    return ret;
  }

  //------------------------------------------------------------------------
  //! Get owner
  //------------------------------------------------------------------------
  const std::string& StatInfo::GetOwner() const
  {
    return pImpl->pOwner;
  }

  //------------------------------------------------------------------------
  //! Get group
  //------------------------------------------------------------------------
  const std::string& StatInfo::GetGroup() const
  {
    return pImpl->pGroup;
  }

  //------------------------------------------------------------------------
  //! Get checksum
  //------------------------------------------------------------------------
  const std::string& StatInfo::GetChecksum() const
  {
    return pImpl->pCksum;
  }

  //------------------------------------------------------------------------
  //! Parse server response and fill up the object
  //------------------------------------------------------------------------
  bool StatInfo::ExtendedFormat() const
  {
    return pImpl->pExtended;
  }

  //------------------------------------------------------------------------
  //! Has checksum
  //------------------------------------------------------------------------
  bool StatInfo::HasChecksum() const
  {
    return pImpl->pHasCksum;
  }

  //----------------------------------------------------------------------------
  // StatInfo constructor
  //----------------------------------------------------------------------------
  StatInfoVFS::StatInfoVFS():
    pNodesRW( 0 ),
    pFreeRW( 0 ),
    pUtilizationRW( 0 ),
    pNodesStaging( 0 ),
    pFreeStaging( 0 ),
    pUtilizationStaging( 0 )
  {
  }

  //----------------------------------------------------------------------------
  // Parse the stat info returned by the server
  //----------------------------------------------------------------------------
  bool StatInfoVFS::ParseServerResponse( const char *data )
  {
    if( !data || strlen( data ) == 0 )
      return false;

    std::vector<std::string> chunks;
    Utils::splitString( chunks, data, " " );

    if( chunks.size() < 6 )
      return false;

    char *result;
    pNodesRW = ::strtoll( chunks[0].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pNodesRW = 0;
      return false;
    }

    pFreeRW = ::strtoll( chunks[1].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pFreeRW = 0;
      return false;
    }

    pUtilizationRW = ::strtol( chunks[2].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pUtilizationRW = 0;
      return false;
    }

    pNodesStaging = ::strtoll( chunks[3].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pNodesStaging = 0;
      return false;
    }

    pFreeStaging = ::strtoll( chunks[4].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pFreeStaging = 0;
      return false;
    }

    pUtilizationStaging = ::strtol( chunks[5].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pUtilizationStaging = 0;
      return false;
    }

    return true;
  }

  const std::string DirectoryList::dStatPrefix = ".\n0 0 0 0";

  //----------------------------------------------------------------------------
  // DirectoryList constructor
  //----------------------------------------------------------------------------
  DirectoryList::DirectoryList()
  {
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  DirectoryList::~DirectoryList()
  {
    for( Iterator it = pDirList.begin(); it != pDirList.end(); ++it )
      delete *it;
  }

  //----------------------------------------------------------------------------
  // Parse the directory list
  //----------------------------------------------------------------------------
  bool DirectoryList::ParseServerResponse( const std::string &hostId,
                                           const char *data )
  {
    if( !data )
      return false;

    //--------------------------------------------------------------------------
    // Check what kind of response we're dealing with
    //--------------------------------------------------------------------------
    bool isDStat = HasStatInfo( data );
    if( isDStat )
      data += dStatPrefix.size();
    return ParseServerResponse( hostId, data, isDStat );
  }

  //------------------------------------------------------------------------
  //! Parse chunked server response and fill up the object
  //------------------------------------------------------------------------
  bool DirectoryList::ParseServerResponse( const std::string &hostId,
                                           const char *data,
                                           bool isDStat )
  {
    if( !data )
      return false;

    std::string dat = data;
    std::vector<std::string> entries;
    std::vector<std::string>::iterator it;
    Utils::splitString( entries, dat, "\n" );

    //--------------------------------------------------------------------------
    // Normal response
    //--------------------------------------------------------------------------
    if( !isDStat )
    {
      for( it = entries.begin(); it != entries.end(); ++it )
        Add( new ListEntry( hostId, *it ) );
      return true;
    }

    //--------------------------------------------------------------------------
    // kXR_dstat
    //--------------------------------------------------------------------------
    if( entries.size() % 2 )
      return false;

    it = entries.begin(); //++it; ++it;
    for( ; it != entries.end(); ++it )
    {
      ListEntry *entry = new ListEntry( hostId, *it );
      Add( entry );
      ++it;
      StatInfo *i = new StatInfo();
      entry->SetStatInfo( i );
      bool ok = i->ParseServerResponse( it->c_str() );
      if( !ok )
        return false;
    }
    return true;
  }

  //------------------------------------------------------------------------
  // Returns true if data contain stat info
  //------------------------------------------------------------------------
  bool DirectoryList::HasStatInfo( const char *data )
  {
    std::string dat = data;
    return !dat.compare( 0, dStatPrefix.size(), dStatPrefix );
  }

  struct PageInfoImpl
  {
    PageInfoImpl( uint64_t offset = 0, uint32_t length = 0, void *buffer = 0,
                  std::vector<uint32_t> &&cksums = std::vector<uint32_t>() ) :
      offset( offset ),
      length( length ),
      buffer( buffer ),
      cksums( std::move( cksums ) ),
      nbrepair( 0 )
    {
    }

    PageInfoImpl( PageInfoImpl &&pginf ) : offset( pginf.offset ),
                                           length( pginf.length ),
                                           buffer( pginf.buffer ),
                                           cksums( std::move( pginf.cksums ) ),
                                           nbrepair( pginf.nbrepair )
    {
    }

    uint64_t               offset;   //> offset in the file
    uint32_t               length;   //> length of the data read
    void                  *buffer;   //> buffer with the read data
    std::vector<uint32_t>  cksums;   //> a vector of crc32c checksums
    size_t                 nbrepair; //> number of repaired pages
  };

  //----------------------------------------------------------------------------
  // Default constructor
  //----------------------------------------------------------------------------
  PageInfo::PageInfo( uint64_t offset, uint32_t length, void *buffer,
                      std::vector<uint32_t> &&cksums ) :
    pImpl( new PageInfoImpl( offset, length, buffer, std::move( cksums ) ) )
  {
  }

  //----------------------------------------------------------------------------
  // Move constructor
  //----------------------------------------------------------------------------
  PageInfo::PageInfo( PageInfo &&pginf ) : pImpl( std::move( pginf.pImpl ) )
  {
  }

  //----------------------------------------------------------------------------
  //! Move assigment operator
  //----------------------------------------------------------------------------
  PageInfo& PageInfo::operator=( PageInfo &&pginf )
  {
    pImpl.swap( pginf.pImpl );
    return *this;
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  PageInfo::~PageInfo()
  {
  }

  //----------------------------------------------------------------------------
  // Get the offset
  //----------------------------------------------------------------------------
  uint64_t PageInfo::GetOffset() const
  {
    return pImpl->offset;
  }

  //----------------------------------------------------------------------------
  // Get the data length
  //----------------------------------------------------------------------------
  uint32_t PageInfo::GetLength() const
  {
    return pImpl->length;
  }

  //----------------------------------------------------------------------------
  // Get the buffer
  //----------------------------------------------------------------------------
  void* PageInfo::GetBuffer()
  {
    return pImpl->buffer;
  }

  //----------------------------------------------------------------------------
  // Get the buffer
  //----------------------------------------------------------------------------
  std::vector<uint32_t>& PageInfo::GetCksums()
  {
    return pImpl->cksums;
  }

  //----------------------------------------------------------------------------
  // Set number of repaired pages
  //----------------------------------------------------------------------------
  void PageInfo::SetNbRepair( size_t nbrepair )
  {
    pImpl->nbrepair = nbrepair;
  }

  //----------------------------------------------------------------------------
  // Get number of repaired pages
  //----------------------------------------------------------------------------
  size_t PageInfo::GetNbRepair()
  {
    return pImpl->nbrepair;
  }

  struct RetryInfoImpl
  {
      RetryInfoImpl( std::vector<std::tuple<uint64_t, uint32_t>> && retries ) :
          retries( std::move( retries ) )
      {

      }

      std::vector<std::tuple<uint64_t, uint32_t>> retries;
  };

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  RetryInfo::RetryInfo( std::vector<std::tuple<uint64_t, uint32_t>> && retries ) :
      pImpl( new RetryInfoImpl( std::move( retries ) ) )
  {

  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  RetryInfo::~RetryInfo(){ }

  //----------------------------------------------------------------------------
  // @return : true if some pages need retrying, false otherwise
  //----------------------------------------------------------------------------
  bool RetryInfo::NeedRetry()
  {
    return !pImpl->retries.empty();
  }

  //----------------------------------------------------------------------------
  // @return number of pages that need to be retransmitted
  //----------------------------------------------------------------------------
  size_t RetryInfo::Size()
  {
    return pImpl->retries.size();
  }

  //----------------------------------------------------------------------------
  // @return : offset and size of respective page that requires to be
  //           retransmitted
  //----------------------------------------------------------------------------
  std::tuple<uint64_t, uint32_t> RetryInfo::At( size_t i )
  {
    return pImpl->retries[i];
  }

  //------------------------------------------------------------------------
  // Factory function for generating handler objects from lambdas
  //------------------------------------------------------------------------
  ResponseHandler* ResponseHandler::Wrap( std::function<void(XRootDStatus&, AnyObject&)> func )
  {
    struct FuncHandler : public ResponseHandler
    {
      FuncHandler( std::function<void(XRootDStatus&, AnyObject&)> func ) : func( std::move( func ) )
      {
      }

      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        // make sure the arguments will be released
        std::unique_ptr<XRootDStatus> stptr( status );
        std::unique_ptr<AnyObject> rspptr( response );
        // if there is no response provide a null reference placeholder
        static AnyObject nullref;
        if( response == nullptr )
          response = &nullref;
        // call the user completion handler
        func( *status, *response );
        // check if this is a final respons
        bool finalrsp = !( status->IsOK() && status->code == suContinue );
        // deallocate the wrapper if final
        if( finalrsp )
          delete this;
      }

      std::function<void(XRootDStatus&, AnyObject&)> func;
    };

    return new FuncHandler( func );
  }

  ResponseHandler* ResponseHandler::Wrap( std::function<void(XRootDStatus*, AnyObject*)> func )
  {
    struct FuncHandler : public ResponseHandler
    {
      FuncHandler( std::function<void(XRootDStatus*, AnyObject*)> func ) : func( std::move( func ) )
      {
      }

      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        // check if this is a final respons
        bool finalrsp = !( status->IsOK() && status->code == suContinue );
        // call the user completion handler
        func( status, response );
        // deallocate the wrapper if final
        if( finalrsp )
          delete this;
      }

      std::function<void(XRootDStatus*, AnyObject*)> func;
    };

    return new FuncHandler( func );
  }
}
