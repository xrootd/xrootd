//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------
// Forward declaration needed for friendship
//-----------------------------------------------------------------------------
namespace XrdEc{ class StrmWriter; class Reader; };

namespace XrdCl
{

  using namespace XrdZip;

  class ZipArchive
  {
    friend class XrdEc::StrmWriter;
    friend class XrdEc::Reader;

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
                             uint32_t            crc32 = 0 );

      inline
      XRootDStatus Read( uint64_t         offset,
                         uint32_t         size,
                         void            *buffer,
                         ResponseHandler *handler,
                         uint16_t         timeout = 0 )
      {
        if( openfn.empty() ) return XRootDStatus( stError, errInvalidOp );
        return ReadFrom( openfn, offset, size, buffer, handler, timeout );
      }

      XRootDStatus ReadFrom( const std::string &fn,
                             uint64_t           offset,
                             uint32_t           size,
                             void              *buffer,
                             ResponseHandler   *handler,
                             uint16_t           timeout = 0 );

      XRootDStatus Write( uint32_t          size,
                          const void       *buffer,
                          ResponseHandler  *handler,
                          uint16_t          timeout = 0 );

      inline XRootDStatus Stat( const std::string &fn, StatInfo *&info )
      { // make sure archive has been opened and CD has been parsed
        if( openstage != Done )
          return XRootDStatus( stError, errInvalidOp );
        // make sure the file is part of the archive
        auto cditr = cdmap.find( fn );
        if( cditr == cdmap.end() )
          return XRootDStatus( stError, errNotFound );
        // create the result
        info = make_stat( fn );
        return XRootDStatus();
      }

      inline XRootDStatus Stat( StatInfo *&info )
      {
        if( openfn.empty() )
          return XRootDStatus( stError, errInvalidOp );
        return Stat( openfn, info );
      }

      inline XRootDStatus GetCRC32( const std::string &fn, uint32_t &cksum )
      { // make sure archive has been opened and CD has been parsed
        if( openstage != Done )
          return XRootDStatus( stError, errInvalidOp );
        // make sure the file is part of the archive
        auto cditr = cdmap.find( fn );
        if( cditr == cdmap.end() )
          return XRootDStatus( stError, errNotFound );
        cksum = cdvec[cditr->second]->ZCRC32;
        return XRootDStatus();
      }

      XRootDStatus CloseArchive( ResponseHandler *handler,
                                 uint16_t         timeout = 0 );

      inline XRootDStatus CloseFile()
      {
        if( openstage != Done || openfn.empty() )
          return XRootDStatus( stError, errInvalidOp,
                               errInvalidOp, "Archive not opened." );
        openfn.clear();
        lfh.reset();
        return XRootDStatus();
      }

      XRootDStatus List( DirectoryList *&list );

    private:

      XRootDStatus OpenOnly( const std::string  &url,
                             ResponseHandler    *handler,
                             uint16_t            timeout = 0 );

      buffer_t GetCD();

      void SetCD( const buffer_t &buffer );

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
        Error,
        NotParsed
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
