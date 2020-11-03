/*
 * XrdClZipArchive.h
 *
 *  Created on: 2 Nov 2020
 *      Author: simonm
 */

#ifndef SRC_XRDCL_XRDCLZIPARCHIVE_HH_
#define SRC_XRDCL_XRDCLZIPARCHIVE_HH_

#include "XrdCl/XrdClXRootDResponses.hh"

#include <memory>
#include <string>

namespace XrdCl
{
  class ZipArchiveImpl;

  struct ZipFlags
  {
    enum Flags
    {
      Read     = kXR_open_read,
      Append   = kXR_open_wrto,
      New      = kXR_new,
      Delete   = kXR_delete,
      Compress = kXR_compress
    };
  };

  class ZipArchive
  {
    public:

      ZipArchive();

      virtual ~ZipArchive();

      XRootDStatus OpenArchive( const std::string &url,
                                ZipFlags::Flags    flags,
                                ResponseHandler   *handler,
                                uint16_t           timeout = 0 );

      XRootDStatus OpenFile( const std::string &fn,
                             ResponseHandler   *handler,
                             uint16_t           timeout = 0 );

      XRootDStatus Read( uint64_t           offset,
                         uint32_t           size,
                         void              *buffer,
                         ResponseHandler   *handler,
                         uint16_t           timeout = 0 );

      XRootDStatus Write( uint64_t           offset,
                          uint32_t           size,
                          const void        *buffer,
                          ResponseHandler   *handler,
                          uint16_t           timeout = 0 );

      XRootDStatus Stat( ResponseHandler   *handler,
                         uint16_t           timeout = 0 );

      XRootDStatus List( ResponseHandler *handler,
                         uint16_t         timeout = 0 );


      XRootDStatus CloseArchive( ResponseHandler *handler,
                                 uint16_t         timeout = 0 );

      XRootDStatus CloseFile( ResponseHandler *handler,
                              uint16_t         timeout = 0 );

    private:

      std::unique_ptr<ZipArchiveImpl> pImpl;
  };

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLZIPARCHIVE_HH_ */
