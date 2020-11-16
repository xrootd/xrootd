/*
 * XrdZipArchiveReader.hh
 *
 *  Created on: 10 Nov 2020
 *      Author: simonm
 */

#ifndef SRC_XRDZIP_XRDZIPARCHIVEREADER_HH_
#define SRC_XRDZIP_XRDZIPARCHIVEREADER_HH_

#include "XrdZip/XrdZipEOCD.hh"
#include "XrdZip/XrdZipCDFH.hh"
#include "XrdZip/XrdZipZIP64EOCD.hh"
#include "XrdZip/XrdZipInflCache.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClOperations.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClResponseJob.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPostMaster.hh"

#include <memory>
#include <unordered_map>

#include <iostream>

namespace XrdZip
{

  class ArchiveReader
  {
    public:

      ArchiveReader();
      virtual ~ArchiveReader();

      XrdCl::XRootDStatus OpenArchive( const std::string       &url,
                                       XrdCl::ResponseHandler *handler,
                                       uint16_t                timeout = 0 );

      XrdCl::XRootDStatus OpenFile( const std::string       &fn,
                                    XrdCl::OpenFlags::Flags  flags );

      XrdCl::XRootDStatus Read( uint64_t                offset,
                                uint32_t                size,
                                void                   *buffer,
                                XrdCl::ResponseHandler *handler,
                                uint16_t                timeout = 0 );

      XrdCl::XRootDStatus Write( uint64_t                offset,
                                 uint32_t                size,
                                 const void             *buffer,
                                 XrdCl::ResponseHandler *handler,
                                 uint16_t                timeout = 0 ){return XrdCl::XRootDStatus();}

      XrdCl::XRootDStatus Stat( XrdCl::ResponseHandler *handler,
                                uint16_t                timeout = 0 ){return XrdCl::XRootDStatus();}

      XrdCl::XRootDStatus CloseArchive( XrdCl::ResponseHandler *handler,
                                        uint16_t                timeout = 0 );

      inline XrdCl::XRootDStatus CloseFile()
      {
        openfn.clear();
        return XrdCl::XRootDStatus();
      }

      XrdCl::XRootDStatus List( XrdCl::ResponseHandler *handler,
                                uint16_t                timeout = 0 ){return XrdCl::XRootDStatus();}

      void Print()
      {
        auto itr = cdvec.begin();
        for( ; itr != cdvec.end() ; ++itr )
        {
          CDFH* cdfh = itr->get();
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

      template<typename Response>
      inline static XrdCl::AnyObject* PkgRsp( Response *rsp )
      {
        if( !rsp ) return nullptr;
        XrdCl::AnyObject *pkg = new XrdCl::AnyObject();
        pkg->Set( rsp );
        return pkg;
      }

      template<typename Response>
      inline static void Free( XrdCl::XRootDStatus *st, Response *rsp )
      {
        delete st;
        delete rsp;
      }

      template<typename Response>
      inline static void Schedule( XrdCl::ResponseHandler *handler, XrdCl::XRootDStatus *st, Response *rsp = 0 )
      {
        if( !handler ) return Free( st, rsp );
        XrdCl::ResponseJob *job = new XrdCl::ResponseJob( handler, st, PkgRsp( rsp ), 0 );
        XrdCl::DefaultEnv::GetPostMaster()->GetJobManager()->QueueJob( job );
      }

      enum OpenStages
      {
        None = 0,
        HaveEocdBlk,
        HaveZip64EocdlBlk,
        HaveZip64EocdBlk,
        HaveCdRecords,
        Done
      };

      typedef std::unordered_map<std::string, InflCache> inflcache_t;

      XrdCl::File                 archive;
      uint64_t                    archsize;
      std::unique_ptr<char[]>     buffer;
      std::unique_ptr<EOCD>       eocd;
      cdvec_t                     cdvec;
      cdmap_t                     cdmap;
      std::unique_ptr<ZIP64_EOCD> zip64eocd;
      OpenStages                  openstage;
      std::string                 openfn;
      inflcache_t                 inflcache;
  };

} /* namespace XrdZip */

#endif /* SRC_XRDZIP_XRDZIPARCHIVEREADER_HH_ */
