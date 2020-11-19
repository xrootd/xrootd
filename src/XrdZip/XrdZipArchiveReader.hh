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
#include "XrdZip/XrdZipLFH.hh"
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

namespace XrdZip
{

  class ArchiveReader
  {
    public:

      ArchiveReader();
      virtual ~ArchiveReader();

      XrdCl::XRootDStatus OpenArchive( const std::string       &url,
                                       XrdCl::OpenFlags::Flags flags,
                                       XrdCl::ResponseHandler *handler,
                                       uint16_t                timeout = 0 );

      XrdCl::XRootDStatus OpenFile( const std::string       &fn,
                                    XrdCl::OpenFlags::Flags  flags = XrdCl::OpenFlags::None,
                                    uint64_t                 size  = 0,
                                    uint32_t                 crc32 = 0,
                                    XrdCl::ResponseHandler  *handler = nullptr,
                                    uint16_t                 timeout = 0 );

      XrdCl::XRootDStatus Read( uint64_t                offset,
                                uint32_t                size,
                                void                   *buffer,
                                XrdCl::ResponseHandler *handler,
                                uint16_t                timeout = 0 );

      XrdCl::XRootDStatus Write( uint32_t                size,
                                 const void             *buffer,
                                 XrdCl::ResponseHandler *handler,
                                 uint16_t                timeout = 0 );

      XrdCl::XRootDStatus Stat( XrdCl::StatInfo *&info )
      {
        info = make_stat( openfn );
        return XrdCl::XRootDStatus();
      }

      XrdCl::XRootDStatus CloseArchive( XrdCl::ResponseHandler *handler,
                                        uint16_t                timeout = 0 );

      inline XrdCl::XRootDStatus CloseFile( XrdCl::ResponseHandler  *handler = nullptr,
                                            uint16_t                 timeout = 0 )
      {
        // TODO if this is a write it is more complicated
        openfn.clear();
        return XrdCl::XRootDStatus();
      }

      XrdCl::XRootDStatus List( XrdCl::DirectoryList *&list );

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

      inline void Schedule( XrdCl::ResponseHandler *handler, XrdCl::XRootDStatus *st )
      {
        if( !handler ) return delete st;
        XrdCl::ResponseJob *job = new XrdCl::ResponseJob( handler, st, 0, 0 );
        XrdCl::DefaultEnv::GetPostMaster()->GetJobManager()->QueueJob( job );
      }

      template<typename Response>
      inline static void Schedule( XrdCl::ResponseHandler *handler, XrdCl::XRootDStatus *st, Response *rsp )
      {
        if( !handler ) return Free( st, rsp );
        XrdCl::ResponseJob *job = new XrdCl::ResponseJob( handler, st, PkgRsp( rsp ), 0 );
        XrdCl::DefaultEnv::GetPostMaster()->GetJobManager()->QueueJob( job );
      }

      inline static XrdCl::StatInfo* make_stat( const XrdCl::StatInfo &starch, uint64_t size )
      {
        XrdCl::StatInfo *info = new XrdCl::StatInfo( starch );
        uint32_t flags = info->GetFlags();
        info->SetFlags( flags & ( ~XrdCl::StatInfo::IsWritable ) ); // make sure it is not listed as writable
        info->SetSize( size );
        return info;
      }

      inline XrdCl::StatInfo* make_stat( const std::string &fn )
      {
        XrdCl::StatInfo *infoptr = 0;
        XrdCl::XRootDStatus st = archive.Stat( false, infoptr );
        std::unique_ptr<XrdCl::StatInfo> stinfo( infoptr );
        auto itr = cdmap.find( fn );
        if( itr == cdmap.end() ) return nullptr;
        size_t index = itr->second;
        return make_stat( *stinfo, cdvec[index]->uncompressedSize );
      }

      inline static XrdCl::XRootDStatus* make_status( const XrdCl::XRootDStatus &status )
      {
        return new XrdCl::XRootDStatus( status );
      }

      inline static XrdCl::XRootDStatus* make_status()
      {
        return new XrdCl::XRootDStatus();
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
      bool                        newarch;
      std::unique_ptr<char[]>     buffer;
      std::unique_ptr<EOCD>       eocd;
      cdvec_t                     cdvec;
      cdmap_t                     cdmap;
      std::unique_ptr<ZIP64_EOCD> zip64eocd;
      OpenStages                  openstage;
      std::string                 openfn;
      inflcache_t                 inflcache;

      XrdCl::OpenFlags::Flags     flags;
      std::unique_ptr<LFH>        lfh;
  };

} /* namespace XrdZip */

#endif /* SRC_XRDZIP_XRDZIPARCHIVEREADER_HH_ */
