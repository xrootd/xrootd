/*
 * XrdZipArchiveReader.hh
 *
 *  Created on: 10 Nov 2020
 *      Author: simonm
 */

#ifndef SRC_XRDZIP_XRDZIPARCHIVEREADER_HH_
#define SRC_XRDZIP_XRDZIPARCHIVEREADER_HH_

#include "XrdZip/XrdZipArchiveIntfc.hh"
#include "XrdZip/XrdZipEOCD.hh"
#include "XrdZip/XrdZipCDFH.hh"
#include "XrdZip/XrdZipZIP64EOCD.hh"
#include "XrdCl/XrdClOperations.hh"

#include <memory>
#include <unordered_map>

#include <iostream>

namespace XrdZip
{

  class ArchiveReader : public ArchiveIntfc
  {
    public:

      ArchiveReader();
      virtual ~ArchiveReader();

      XrdCl::XRootDStatus OpenArchive( const std::string       &url,
                                       XrdCl::ResponseHandler *handler,
                                       uint16_t                timeout = 0 );

      XrdCl::XRootDStatus OpenFile( const std::string      &fn,
                                    XrdCl::ResponseHandler *handler,
                                    uint16_t                timeout = 0 ){return XrdCl::XRootDStatus();}

      XrdCl::XRootDStatus Read( uint64_t                offset,
                                uint32_t                size,
                                void                   *buffer,
                                XrdCl::ResponseHandler *handler,
                                uint16_t                timeout = 0 ){return XrdCl::XRootDStatus();}

      XrdCl::XRootDStatus Write( uint64_t                offset,
                                 uint32_t                size,
                                 const void             *buffer,
                                 XrdCl::ResponseHandler *handler,
                                 uint16_t                timeout = 0 ){return XrdCl::XRootDStatus();}

      XrdCl::XRootDStatus Stat( XrdCl::ResponseHandler *handler,
                                uint16_t                timeout = 0 ){return XrdCl::XRootDStatus();}

      XrdCl::XRootDStatus CloseArchive( XrdCl::ResponseHandler *handler,
                                        uint16_t                timeout = 0 ){return XrdCl::XRootDStatus();}

      XrdCl::XRootDStatus CloseFile( XrdCl::ResponseHandler *handler,
                                     uint16_t                timeout = 0 ){return XrdCl::XRootDStatus();}

      XrdCl::XRootDStatus List( XrdCl::ResponseHandler *handler,
                                uint16_t                timeout = 0 ){return XrdCl::XRootDStatus();}

      void Print()
      {
        central_directory::iterator itr = cdRecords.begin();
        for( ; itr != cdRecords.end() ; ++itr )
        {
          std::cout << itr->first << " :\n";
          CDFH* cdfh = itr->second.get();
          std::cout << "ZCRC32 = " << cdfh->ZCRC32 << '\n';
          std::cout << "cdfhBaseSize = " << cdfh->cdfhBaseSize << '\n';
          std::cout << "cdfhSize = " << cdfh->cdfhSize << '\n';
          std::cout << "comment = " << cdfh->comment << '\n';
          std::cout << "commentLength = " << cdfh->commentLength << '\n';
          std::cout << "compressedSize = " << cdfh->compressedSize << '\n';
          std::cout << "compressionMethod = " << cdfh->compressionMethod << '\n';
          std::cout << "externAttr = " << cdfh->externAttr << '\n';
          std::cout << "extraLength = " << cdfh->extraLength << '\n';
          std::cout << "filename = " << cdfh->filename << '\n';
          std::cout << "filenameLength = " << cdfh->filenameLength << '\n';
          std::cout << "generalBitFlag = " << cdfh->generalBitFlag << '\n';
          std::cout << "internAttr = " << cdfh->internAttr << '\n';
          std::cout << "minZipVersion = " << cdfh->minZipVersion << '\n';
          std::cout << "nbDisk = " << cdfh->nbDisk << '\n';
          std::cout << "offset = " << cdfh->offset << '\n';
          std::cout << "timestmp.date = " << cdfh->timestmp.date << '\n';
          std::cout << "timestmp.time = " << cdfh->timestmp.time << '\n';
          std::cout << "uncompressedSize = " << cdfh->uncompressedSize << '\n';
          std::cout << "zipVersion = " << cdfh->zipVersion << '\n';

          if( cdfh->extra )
          {
            std::cout << "extra : \n";
            Extra *extra = cdfh->extra.get();
            std::cout << "extra->compressedSize = " << extra->compressedSize << '\n';
            std::cout << "extra->dataSize = " << extra->dataSize << '\n';
            std::cout << "extra->nbDisk = " << extra->nbDisk << '\n';
            std::cout << "extra->offset = " << extra->offset << '\n';
            std::cout << "extra->uncompressedSize = " << extra->uncompressedSize << '\n';
          }

          std::cout << std::endl;
        }
      }

    private:

      enum OpenStages
      {
        None = 0,
        HaveEocdBlk,
        HaveZip64EocdlBlk,
        HaveZip64EocdBlk,
        HaveCdRecords,
        Done
      };

      uint64_t                    archsize;
      std::unique_ptr<char[]>     buffer;
      bool                        isopen;
      std::unique_ptr<EOCD>       eocd;
      central_directory           cdRecords;
      std::unique_ptr<ZIP64_EOCD> zip64eocd;
      OpenStages                  openstage;
  };

} /* namespace XrdZip */

#endif /* SRC_XRDZIP_XRDZIPARCHIVEREADER_HH_ */
