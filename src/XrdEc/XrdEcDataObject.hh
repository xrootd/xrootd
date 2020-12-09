/*
 * XrdEcDataObject.hh
 *
 *  Created on: Jan 23, 2020
 *      Author: simonm
 */

#ifndef SRC_API_TEST_XRDECDATAOBJECT_HH_
#define SRC_API_TEST_XRDECDATAOBJECT_HH_

#include "XrdEc/XrdEcObjCfg.hh"
#include "XrdEc/XrdEcWrtBuff.hh"
#include "XrdEc/XrdEcZipUtilities.hh"
#include "XrdEc/XrdEcStrmWriter.hh"
#include "XrdEc/XrdEcObjInterfaces.hh"

#include "XrdOuc/XrdOucCRC32C.hh"

#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClBuffer.hh"


#include <vector>
#include <string>
#include <future>
#include <memory>
#include <atomic>
#include <list>
#include <unordered_map>
#include <tuple>

namespace XrdCl
{
  class ResponseHandler;
}

namespace XrdEc
{
  class DataObject
  {
    public:

      DataObject() : mode( None )
      {
      }

      virtual ~DataObject()
      {
      }

      inline void Open( const ObjCfg &objcfg, OpenMode mode, XrdCl::ResponseHandler *handler )
      {
        this->objcfg.reset( new ObjCfg( objcfg ) );
        this->mode = mode;

        if( mode == OpenMode::StrmWrtMode )
        {
          operation.reset( new StrmWriter( objcfg ) );
          operation->Open( handler );
//          OpenForWrite( handler );
//          size_t size = objcfg.plgr.size();
//          offsets.reset( new std::atomic<uint32_t>[size] );
//          std::fill( offsets.get(), offsets.get() + size, 0 );
//          dirs.reserve( size );
//          for( size_t i = 0; i < size; ++i )
//            dirs.emplace_back( &objcfg );
        }
      }

      void Write( uint64_t offset, uint32_t size, const void *buffer, XrdCl::ResponseHandler *handler )
      {
        operation->Write( offset, size, buffer, handler );
      }

      void Read( uint64_t offset, uint32_t size, void *buffer, XrdCl::ResponseHandler *handler )
      {
        operation->Read( offset, size, buffer, handler );
      }

      void Close( XrdCl::ResponseHandler *handler )
      {
        operation->Close( handler );
      }

    private:

      struct WrtCtx
      {
        WrtCtx( uint32_t size, uint32_t checksum, const std::string &fn, const void *buffer, uint32_t bufflen ) : lfh( size, checksum, fn.size() ), fn( fn), buffer( buffer ), bufflen( bufflen ), total_size( 0 )
        {
          iov[0].iov_base = lfh.get_buff();
          iov[0].iov_len  = LFH::size;
          total_size     += LFH::size;

          iov[1].iov_base = (void*)fn.c_str();
          iov[1].iov_len  = fn.size();
          total_size     += fn.size();

          iov[2].iov_base = (void*)buffer;
          iov[2].iov_len  = bufflen;
          total_size     += bufflen;
        }

        LFH               lfh;
        std::string       fn;
        const void       *buffer;
        uint32_t          bufflen;
        static const int  iovcnt = 3;
        iovec             iov[iovcnt];
        uint32_t          total_size;
      };

      struct CentralDirectory
      {
          CentralDirectory( const ObjCfg *objcfg ) : cd_buffer( DefaultSize( objcfg ) ), cd_size( 0 ), cd_crc32c( 0 ), cd_offset( 0 ), nb_entries( 0 )
          {
          }

          CentralDirectory( CentralDirectory && cd ) : cd_buffer( std::move( cd.cd_buffer ) ), cd_size( cd.cd_size ), cd_crc32c( cd.cd_crc32c ), cd_offset( cd.cd_offset ), nb_entries( cd.nb_entries )
          {
          }

          ~CentralDirectory()
          {
          }

          inline void Add( const std::string &fn, uint32_t size, uint32_t checksum, uint32_t offset )
          {
            uint32_t reqsize = CDH::size + fn.size();
            // if we don't have enough space in the buffer double its size
            if( reqsize > cd_buffer.GetSize() - cd_buffer.GetCursor() )
              cd_buffer.ReAllocate( cd_buffer.GetSize() * 2 );

            // Use placement new to construct Central Directory File Header in the buffer
            new ( cd_buffer.GetBufferAtCursor() ) CDH( size, checksum, fn.size(), offset );
            cd_buffer.AdvanceCursor( CDH::size );

            // Copy the File Name into the buffer
            memcpy( cd_buffer.GetBufferAtCursor(), fn.c_str(), fn.size() );
            cd_buffer.AdvanceCursor( fn.size() );

            // Update the Central Directory size and offset
            cd_size += CDH::size + fn.size();
            cd_offset = offset + LFH::size + fn.size() + size;

            // Update the number of entries in the Central Directory
            ++nb_entries;
          }

          inline bool Empty()
          {
            return cd_buffer.GetCursor() == 0;
          }

          void CreateEOCD()
          {
            // make sure we have enough space for the End Of Central Directory record
            if( EOCD::size >  cd_buffer.GetSize() - cd_buffer.GetCursor() )
              cd_buffer.ReAllocate( cd_buffer.GetSize() + EOCD::size );

            // Now create End of Central Directory record
            new ( cd_buffer.GetBufferAtCursor() ) EOCD( nb_entries, cd_size, cd_offset );
            cd_buffer.AdvanceCursor( EOCD::size );
          }

          XrdCl::Buffer cd_buffer;
          uint32_t      cd_size;
          uint32_t      cd_crc32c;

        private:

          inline static size_t DefaultSize( const ObjCfg *objcfg )
          {
            // this should more or less give us space to accommodate 32 headers
            // witch with 32MB block corresponds to 1GB of data

            return ( CDH::size + objcfg->obj.size() + 6 /*account for the BLKID and CHID*/ ) * 32 + EOCD::size;
          }

          uint32_t               cd_offset;
          uint16_t               nb_entries;
      };


      struct MetaDataCtx
      {
          MetaDataCtx( ObjCfg *objcfg, std::vector<CentralDirectory> &dirs ) : iovcnt( 0 ), iov( nullptr ), cd( objcfg )
          {
            size_t size = objcfg->plgr.size();

            // figure out how many buffers will we need
            for( size_t i = 0; i < size; ++i )
            {
              if( dirs[i].Empty() ) continue;

              iovcnt += 3;                  // a buffer for LFH, a buffer for file name, a buffer for the binary data
            }
            ++iovcnt; // a buffer for the Central Directory

            // now once we know the size allocate the iovec
            iov = new iovec[iovcnt];
            int index = 0;
            uint32_t offset = 0;

            // add the Local File Headers and respective binary data
            for( size_t i = 0; i < size; ++i )
            {
              if( dirs[i].Empty() ) continue;

              std::string fn = objcfg->plgr[i] + objcfg->obj + ".data.zip";
              fns.emplace_back( fn );
              lheaders.emplace_back( dirs[i].cd_size, dirs[i].cd_crc32c, fn.size() );

              // Local File Header
              iov[index].iov_base = lheaders.back().get_buff();
              iov[index].iov_len  = LFH::size;
              ++index;

              // File Name
              iov[index].iov_base = (void*)fns.back().c_str();
              iov[index].iov_len  = fns.back().size();
              ++index;

              // Binary Data
              iov[index].iov_base = dirs[i].cd_buffer.GetBuffer();
              iov[index].iov_len  = dirs[i].cd_buffer.GetCursor() - EOCD::size;
              ++index;

              // update Central Directory
              cd.Add( fn, dirs[i].cd_size, dirs[i].cd_crc32c, offset );

              // update the offset
              offset += LFH::size + fn.size() + dirs[i].cd_size;
            }

            // add the Central Directory and End Of Central Directory record
            cd.CreateEOCD();
            iov[index].iov_base = cd.cd_buffer.GetBuffer();
            iov[index].iov_len  = cd.cd_buffer.GetCursor();
            ++index;
          }

          ~MetaDataCtx()
          {
            delete[] iov;
          }

          int    iovcnt;
          iovec *iov;

        private:

          std::list<LFH>         lheaders;
          std::list<std::string> fns;
          CentralDirectory       cd;
      };

      struct BlockMetadata
      {
          std::vector<std::string> URLS;
      };

      XrdCl::File& ToFile( const std::string &url )
      {
        for( size_t i = 0; i < objcfg->plgr.size(); ++i )
        {
          std::string u = objcfg->plgr[i] + objcfg->obj + ".data.zip";
          if( u == url ) return files[i];
        }

        throw std::exception(); // TODO
      }

//      void OpenForWrite( XrdCl::ResponseHandler *handler );
//
//      void OpenForRead( XrdCl::ResponseHandler *handler );
//
//      void CloseAfterWrite( XrdCl::ResponseHandler *handler );
//
//      void CloseAfterRead( XrdCl::ResponseHandler *handler );
//
//      void WriteBlock();
//
//      void ParseMetadata( const void *buffer, uint32_t length );

      // common
      std::unique_ptr<ObjCfg>            objcfg;
      std::vector<XrdCl::File>           files;
      OpenMode mode;

      // For writing
      std::unique_ptr<std::atomic<uint32_t>[]>     offsets;
      std::unique_ptr<WrtBuff>                     wrtbuff;
      std::queue<std::future<XrdCl::XRootDStatus>> pending_wrts;
      std::vector<CentralDirectory>                dirs;

      // For reading
      // mapping from block ID to a vector index by chunk ID containing a tuple of URL, offset and size
      std::unordered_map<uint64_t, std::vector<std::tuple<std::string,uint32_t, uint32_t>>> metadata;


      std::unique_ptr<ObjOperation> operation;
  };

} /* namespace XrdEc */

#endif /* SRC_API_TEST_XRDECDATAOBJECT_HH_ */
