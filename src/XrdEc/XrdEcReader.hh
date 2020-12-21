/*
 * XrdEcReader.hh
 *
 *  Created on: 18 Dec 2020
 *      Author: simonm
 */

#ifndef SRC_XRDEC_XRDECREADER_HH_
#define SRC_XRDEC_XRDECREADER_HH_

#include "XrdEc/XrdEcObjCfg.hh"

#include "XrdCl/XrdClFileOperations.hh"

#include "XrdZip/XrdZipLFH.hh"
#include "XrdZip/XrdZipCDFH.hh"
#include "XrdZip/XrdZipUtils.hh"

#include "XrdOuc/XrdOucCRC32C.hh"

#include <string>
#include <iostream>

namespace XrdEc
{

  class Reader
  {
    public:
      Reader( ObjCfg &objcfg ) : objcfg( objcfg )
      {
      }

      virtual ~Reader()
      {
      }

      void Open( XrdCl::ResponseHandler *handler )
      {
        const size_t size = objcfg.plgr.size();
        std::vector<XrdCl::Pipeline> opens; opens.reserve( size );
        for( size_t i = 0; i < size; ++i )
        { // create the file object
          dataarchs.emplace_back( std::make_shared<XrdCl::File>() );
          // open the archive
          std::string url = objcfg.plgr[i] + objcfg.obj + ".zip";
          opens.emplace_back( XrdCl::Open( *dataarchs[i], url, XrdCl::OpenFlags::Read ) );
        }
        // in parallel open the data files and read the metadata
        XrdCl::Pipeline p = XrdCl::Parallel( ReadMetadata( 0 ), XrdCl::Parallel( opens ) ) >> handler;
        XrdCl::Async( std::move( p ) );
      }

    private:

      XrdCl::Pipeline ReadMetadata( size_t index )
      {
        const size_t size = objcfg.plgr.size();
        // create the File object
        auto file = std::make_shared<XrdCl::File>();
        // prepare the URL for Open operation
        std::string url = objcfg.plgr[index] + objcfg.obj + ".metadata.zip";
        // arguments for the Read operation
        XrdCl::Fwd<uint32_t> rdsize;
        XrdCl::Fwd<void*>    rdbuff;

        return XrdCl::Open( *file, url, XrdCl::OpenFlags::Read ) >>
                 [=]( XrdCl::XRootDStatus &st, XrdCl::StatInfo &info )
                 {
                   if( !st.IsOK() )
                   {
                     if( index + 1 < size )
                       XrdCl::Pipeline::Replace( ReadMetadata( index + 1 ) );
                     return;
                   }
                   // prepare the args for the subsequent operation
                   rdsize = info.GetSize();
                   rdbuff = new char[info.GetSize()];
                 }
             | XrdCl::Read( *file, 0, rdsize, rdbuff ) >>
                 [=]( XrdCl::XRootDStatus &st, XrdCl::ChunkInfo &ch )
                 {
                   if( !st.IsOK() )
                   {
                     if( index + 1 < size )
                       XrdCl::Pipeline::Replace( ReadMetadata( index + 1 ) );
                     return;
                   }
                   // now parse the metadata
                   if( !ParseMetadata( ch ) )
                   {
                     if( index + 1 < size )
                       XrdCl::Pipeline::Replace( ReadMetadata( index + 1 ) );
                     return;
                   }
                 }
             | XrdCl::Final(
                 [=]( const XrdCl::XRootDStatus& )
                 {
                   // deallocate the buffer if necessary
                   char* buffer = reinterpret_cast<char*>( *rdbuff );
                   delete[] buffer;
                   // close the file if necessary (we don't really care about the result)
                   if( file->IsOpen() ) file->Close( nullptr );
                 } );
      }

      bool ParseMetadata( XrdCl::ChunkInfo &ch )
      {
        std::cout << __func__ << std::endl;
        const size_t mincnt = objcfg.nbdata + objcfg.nbparity;
        const size_t maxcnt = objcfg.plgr.size();

        char    *buffer = reinterpret_cast<char*>( ch.buffer );
        size_t  length = ch.length;

        for( size_t i = 0; i < maxcnt; ++i )
        {
          uint32_t signature = XrdZip::to<uint32_t>( buffer );
          if( signature != XrdZip::LFH::lfhSign )
          {
            if( i + 1 < mincnt ) return false;
            break;
          }
          XrdZip::LFH lfh( buffer );
          // check if we are not reading passed the end of the buffer
          if( lfh.lfhSize + lfh.uncompressedSize > length ) return false;
          buffer += lfh.lfhSize;
          length -= lfh.lfhSize;
          // verify the checksum
          uint32_t crc32val = crc32c( 0, buffer, lfh.uncompressedSize );
          if( crc32val != lfh.ZCRC32 ) return false;
          XrdZip::CDFH::Parse( buffer, lfh.uncompressedSize, cdrecs );
          buffer += lfh.uncompressedSize;
          length -= lfh.uncompressedSize;
        }

        auto itr = cdrecs.begin();
        for( ; itr != cdrecs.end() ; ++itr )
        {
          std::cout << itr->first << " : " << itr->second.get()->filename << std::endl;
        }

        return true;
      }

      ObjCfg &objcfg;
      std::vector<std::shared_ptr<XrdCl::File>>  dataarchs;
      XrdZip::cdrecs_t cdrecs;
  };

} /* namespace XrdEc */

#endif /* SRC_XRDEC_XRDECREADER_HH_ */
