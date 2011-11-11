//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClFile.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Open the file pointed to by the given URL
  //----------------------------------------------------------------------------
  bool File::Open( const std::string &url, uint16_t mode )
  {
    return true;
  }

  //----------------------------------------------------------------------------
  // Close the file
  //----------------------------------------------------------------------------
  bool File::Close()
  {
    return true;
  }

  //----------------------------------------------------------------------------
  // Synchronously read data at given offset
  //----------------------------------------------------------------------------
  bool File::Read( uint64_t offset, uint32_t size, void *buffer )
  {
    return true;
  }

  //----------------------------------------------------------------------------
  // Send a data read request and call the given callback when the data
  // is available
  //----------------------------------------------------------------------------
  bool File::Read( uint64_t offset, uint32_t size, ReadCallback *callBack )
  {
    return true;
  }

  //----------------------------------------------------------------------------
  // Synchronously write data at given offset
  //----------------------------------------------------------------------------
  bool File::Write( uint64_t offset, uint32_t size, void *buffer )
  {
    return true;
  }

  //----------------------------------------------------------------------------
  // Send a data write request and call the given callback when the
  // confirmation has been received.
  //----------------------------------------------------------------------------
  bool File::Write( uint64_t offset, uint32_t size, WriteCallback *callBack )
  {
  }
}
