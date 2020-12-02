/*
 * XrdZipArchiveReader.hh
 *
 *  Created on: 10 Nov 2020
 *      Author: simonm
 */

#ifndef SRC_XRDZIP_XRDZIPARCHIVE_HH_
#define SRC_XRDZIP_XRDZIPARCHIVE_HH_

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClOperations.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClResponseJob.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdZip/XrdZipEOCD.hh"
#include "XrdZip/XrdZipCDFH.hh"
#include "XrdZip/XrdZipZIP64EOCD.hh"
#include "XrdZip/XrdZipLFH.hh"
#include "XrdCl/XrdClZipCache.hh"

#include <memory>
#include <unordered_map>

namespace XrdCl
{

  using namespace XrdZip;

  class ZipArchive
  {
    public:

      ZipArchive();
      virtual ~ZipArchive();

      XRootDStatus OpenArchive( const std::string  &url,
                                OpenFlags::Flags    flags,
                                ResponseHandler    *handler,
                                uint16_t            timeout = 0 );

      XRootDStatus OpenFile( const std::string  &fn,
                             OpenFlags::Flags    flags = OpenFlags::None,
                             uint64_t            size  = 0,
                             uint32_t            crc32 = 0,
                             ResponseHandler    *handler = nullptr,
                             uint16_t            timeout = 0 );

      XRootDStatus Read( uint64_t         offset,
                         uint32_t         size,
                         void            *buffer,
                         ResponseHandler *handler,
                         uint16_t         timeout = 0 );

      XRootDStatus Write( uint32_t          size,
                          const void       *buffer,
                          ResponseHandler  *handler,
                          uint16_t          timeout = 0 );

      XRootDStatus Stat( StatInfo *&info )
      {
        info = make_stat( openfn );
        return XRootDStatus();
      }

      XRootDStatus CloseArchive( ResponseHandler *handler,
                                 uint16_t         timeout = 0 );

      inline XRootDStatus CloseFile( ResponseHandler  *handler = nullptr,
                                     uint16_t          timeout = 0 )
      {
        openfn.clear();
        lfh.reset();
        if( handler ) Schedule( handler, make_status() );
        return XRootDStatus();
      }

      XRootDStatus List( DirectoryList *&list );

    private:

      template<typename Response>
      inline static AnyObject* PkgRsp( Response *rsp )
      {
        if( !rsp ) return nullptr;
        AnyObject *pkg = new AnyObject();
        pkg->Set( rsp );
        return pkg;
      }

      template<typename Response>
      inline static void Free( XRootDStatus *st, Response *rsp )
      {
        delete st;
        delete rsp;
      }

      inline void Schedule( ResponseHandler *handler, XRootDStatus *st )
      {
        if( !handler ) return delete st;
        JobManager *jobMgr = DefaultEnv::GetPostMaster()->GetJobManager();
        if( jobMgr->IsWorker() )
          // this is a worker thread so we can simply call the handler
          handler->HandleResponse( st, nullptr );
        else
        {
          ResponseJob *job = new ResponseJob( handler, st, nullptr, nullptr );
          DefaultEnv::GetPostMaster()->GetJobManager()->QueueJob( job );
        }
      }

      template<typename Response>
      inline static void Schedule( ResponseHandler *handler, XRootDStatus *st, Response *rsp )
      {
        if( !handler ) return Free( st, rsp );
        JobManager *jobMgr = DefaultEnv::GetPostMaster()->GetJobManager();
        if( jobMgr->IsWorker() )
          // this is a worker thread so we can simply call the handler
          handler->HandleResponse( st, PkgRsp( rsp ) );
        else
        {
          ResponseJob *job = new ResponseJob( handler, st, PkgRsp( rsp ), 0 );
          DefaultEnv::GetPostMaster()->GetJobManager()->QueueJob( job );
        }
      }

      inline static StatInfo* make_stat( const StatInfo &starch, uint64_t size )
      {
        StatInfo *info = new StatInfo( starch );
        uint32_t flags = info->GetFlags();
        info->SetFlags( flags & ( ~StatInfo::IsWritable ) ); // make sure it is not listed as writable
        info->SetSize( size );
        return info;
      }

      inline StatInfo* make_stat( const std::string &fn )
      {
        StatInfo *infoptr = 0;
        XRootDStatus st = archive.Stat( false, infoptr );
        std::unique_ptr<StatInfo> stinfo( infoptr );
        auto itr = cdmap.find( fn );
        if( itr == cdmap.end() ) return nullptr;
        size_t index = itr->second;
        return make_stat( *stinfo, cdvec[index]->uncompressedSize );
      }

      inline static XRootDStatus* make_status( const XRootDStatus &status )
      {
        return new XRootDStatus( status );
      }

      inline static XRootDStatus* make_status()
      {
        return new XRootDStatus();
      }

      inline void Clear()
      {
        buffer.reset();
        eocd.reset();
        cdvec.clear();
        cdmap.clear();
        zip64eocd.reset();
        openstage = None;
      }

      enum OpenStages
      {
        None = 0,
        HaveEocdBlk,
        HaveZip64EocdlBlk,
        HaveZip64EocdBlk,
        HaveCdRecords,
        Done,
        Error
      };

      typedef std::unordered_map<std::string, ZipCache> zipcache_t;

      File                        archive;
      uint64_t                    archsize;
      bool                        cdexists;
      bool                        updated;
      std::unique_ptr<char[]>     buffer;
      std::unique_ptr<EOCD>       eocd;
      cdvec_t                     cdvec;
      cdmap_t                     cdmap;
      uint64_t                    cdoff;
      uint32_t                    orgcdsz;  //> original CD size
      uint32_t                    orgcdcnt; //> original number CDFH records
      buffer_t                    orgcdbuf; //> buffer with the original CDFH records
      std::unique_ptr<ZIP64_EOCD> zip64eocd;
      OpenStages                  openstage;
      std::string                 openfn;
      zipcache_t                  zipcache;

      OpenFlags::Flags            flags;
      std::unique_ptr<LFH>        lfh;
  };

} /* namespace XrdZip */

#endif /* SRC_XRDZIP_XRDZIPARCHIVE_HH_ */
