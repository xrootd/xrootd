/*
 * XrdZipArchiveIntfc.hh
 *
 *  Created on: 10 Nov 2020
 *      Author: simonm
 */

#ifndef SRC_XRDZIP_XRDZIPARCHIVEINTFC_HH_
#define SRC_XRDZIP_XRDZIPARCHIVEINTFC_HH_

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClFile.hh"
#include <string>

namespace XrdZip
{
  struct ArchiveIntfc
  {
    virtual ~ArchiveIntfc()
    {
    }

    virtual XrdCl::XRootDStatus OpenArchive( const std::string       &url,
                                             XrdCl::ResponseHandler *handler,
                                             uint16_t                timeout = 0 ) = 0;

    virtual XrdCl::XRootDStatus OpenFile( const std::string      &fn,
                                          XrdCl::ResponseHandler *handler,
                                          uint16_t                timeout = 0 ) = 0;

    virtual XrdCl::XRootDStatus Read( uint64_t                offset,
                                      uint32_t                size,
                                      void                   *buffer,
                                      XrdCl::ResponseHandler *handler,
                                      uint16_t                timeout = 0 ) = 0;

    virtual XrdCl::XRootDStatus Write( uint64_t                offset,
                                       uint32_t                size,
                                       const void             *buffer,
                                       XrdCl::ResponseHandler *handler,
                                       uint16_t                timeout = 0 ) = 0;

    virtual XrdCl::XRootDStatus Stat( XrdCl::ResponseHandler *handler,
                                      uint16_t                timeout = 0 ) = 0;

    virtual XrdCl::XRootDStatus CloseArchive( XrdCl::ResponseHandler *handler,
                                              uint16_t                timeout = 0 ) = 0;

    virtual XrdCl::XRootDStatus CloseFile( XrdCl::ResponseHandler *handler,
                                           uint16_t                timeout = 0 ) = 0;

    virtual XrdCl::XRootDStatus List( XrdCl::ResponseHandler *handler,
                                      uint16_t                timeout = 0 ) = 0;

    XrdCl::File archive;
  };
}

#endif /* SRC_XRDZIP_XRDZIPARCHIVEINTFC_HH_ */
