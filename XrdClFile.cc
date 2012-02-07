//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
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
  // Open the file pointed to by the given URL - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Open( const std::string &/*url*/,
                           uint16_t           /*flags*/,
                           uint16_t           /*mode*/,
                           ResponseHandler   */*handler*/,
                           uint16_t           /*timeout*/ )
  {
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Open the file pointed to by the given URL - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Open( const std::string &/*url*/,
                           uint16_t           /*flags*/,
                           uint16_t           /*mode*/,
                           uint16_t           /*timeout*/ )
  {
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Close the file
  //----------------------------------------------------------------------------
  XRootDStatus File::Close()
  {
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Read a data chunk at a given offset - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Read( uint64_t         /*offset*/,
                           uint32_t         /*size*/,
                           void            */*buffer*/,
                           ResponseHandler */*handler*/,
                           uint16_t         /*timeout*/ )
  {
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Read a data chunk at a given offset - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Read( uint64_t  /*offset*/,
                           uint32_t  /*size*/,
                           void     */*buffer*/,
                           uint16_t  /*timeout*/ )
  {
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Write a data chank at a given offset - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Write( uint64_t         /*offset*/,
                            uint32_t         /*size*/,
                            void            */*buffer*/,
                            ResponseHandler */*handler*/,
                            uint16_t         /*timeout*/ )
  {
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Write a data chunk at a given offset - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Write( uint64_t  /*offset*/,
                            uint32_t  /*size*/,
                            void     */*buffer*/,
                            uint16_t  /*timeout*/ )
  {
    return XRootDStatus();
  }

}
