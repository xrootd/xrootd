/*
 * XrdClEcHandler.hh
 *
 *  Created on: 23 Mar 2021
 *      Author: simonm
 */

#ifndef SRC_XRDCL_XRDCLECHANDLER_HH_
#define SRC_XRDCL_XRDCLECHANDLER_HH_

#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClCheckSumHelper.hh"
#include "XrdCl/XrdClResponseJob.hh"

#include "XrdEc/XrdEcReader.hh"
#include "XrdEc/XrdEcStrmWriter.hh"

#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucPgrwUtils.hh"

#include <memory>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <mutex>

namespace XrdCl
{
  class FreeSpace {
  public:
    std::string address;
    uint64_t freeSpace;
    FreeSpace() {};
    bool operator<(const FreeSpace &a) const
    {
      return ((freeSpace > a.freeSpace) ? true : false);
    }
    void Dump() const
    {
      std::cout << address << " : " << freeSpace << std::endl;
    }
  };

  class ServerSpaceInfo {
  public:
    ServerSpaceInfo();
    ~ServerSpaceInfo() {};
    // From the old location list, select a new location list
    // n: select at least "n" nodes in the new location list
    void SelectLocations(XrdCl::LocationInfo &oldList,
                         XrdCl::LocationInfo &newList,
                         uint32_t n);
    void Dump();
  private:
    std::vector<FreeSpace> ServerList;
    std::vector<std::string> ExportPaths;
    time_t lastUpdateT = 0;
    int xRatio = 10;
    std::mutex lock;
    bool initExportPaths = false;

    void TryInitExportPaths();
    uint64_t GetFreeSpace(const std::string addr);
    bool BlindSelect();
    void UpdateSpaceInfo();
    bool Exists(XrdCl::LocationInfo::Location &loc);
    void AddServers(XrdCl::LocationInfo &locInfo);
  };

  class EcPgReadResponseHandler : public ResponseHandler
  {
    private:
      XrdCl::ResponseHandler *realHandler;
    public:
    // constructor
    EcPgReadResponseHandler(ResponseHandler *a) : realHandler(a) {}
  
    // Response Handler
    void HandleResponse(XRootDStatus *status,
                        AnyObject    *rdresp)
    {
      if( !status->IsOK() )
      {
        realHandler->HandleResponse( status, rdresp );
        delete this;
        return;
      }
  
      ChunkInfo *chunk = 0;
      rdresp->Get(chunk);

      if (!chunk) {
        delete this;
        return;
      }
  
      std::vector<uint32_t> cksums;
      size_t nbpages = chunk->length / XrdSys::PageSize;
      if( chunk->length % XrdSys::PageSize )
        ++nbpages;
      cksums.reserve( nbpages );

      size_t  size = chunk->length;
      char   *buffer = reinterpret_cast<char*>( chunk->buffer );

      for( size_t pg = 0; pg < nbpages; ++pg )
      {
        size_t pgsize = XrdSys::PageSize;
        if( pgsize > size ) pgsize = size;
        uint32_t crcval = XrdOucCRC::Calc32C( buffer, pgsize );
        cksums.push_back( crcval );
        buffer += pgsize;
        size   -= pgsize;
      }
  
      PageInfo *pages = new PageInfo(chunk->offset, chunk->length, chunk->buffer, std::move(cksums));
      delete rdresp;
      AnyObject *response = new AnyObject();
      response->Set( pages );
      realHandler->HandleResponse( status, response );
  
      delete this;
    }
  };

  class EcHandler : public FilePlugIn
  {
    public:
      EcHandler( const URL                       &redir,
                 XrdEc::ObjCfg                   *objcfg,
                 std::unique_ptr<CheckSumHelper>  cksHelper ) : redir( redir ),
                                                                fs( redir, false ),
                                                                objcfg( objcfg ),
                                                                curroff( 0 ),
                                                                cksHelper( std::move( cksHelper ) )
      {
        XrdEc::Config::Instance().enable_plugins = false;
      }

      virtual ~EcHandler()
      {
      }

      XRootDStatus Open( uint16_t           flags,
                         ResponseHandler   *handler,
                         time_t             timeout )
      {
        if( ( flags & OpenFlags::Write ) || ( flags & OpenFlags::Update ) )
        {
          if( !( flags & OpenFlags::New )    || // it has to be a new file
               ( flags & OpenFlags::Delete ) || // truncation is not supported
               ( flags & OpenFlags::Read ) )    // write + read is not supported
            return XRootDStatus( stError, errNotSupported );

          if( objcfg->plgr.empty() )
          {
            XRootDStatus st = LoadPlacement();
            if( !st.IsOK() ) return st;
          }
          writer.reset( new XrdEc::StrmWriter( *objcfg ) );
          writer->Open( handler, timeout );
          return XRootDStatus();
        } 
    
        if( flags & OpenFlags::Read )
        {
          if( flags & OpenFlags::Write )
            return XRootDStatus( stError, errNotSupported );

          if( objcfg->plgr.empty() )
          {
            XRootDStatus st = LoadPlacement( redir.GetPath() );
            if( !st.IsOK() ) return st;
          }
          reader.reset( new XrdEc::Reader( *objcfg ) );
          reader->Open( handler, timeout );
          return XRootDStatus();
        }

        return XRootDStatus( stError, errNotSupported );
      }

      XRootDStatus Open( const std::string &url,
                         OpenFlags::Flags   flags,
                         Access::Mode       mode,
                         ResponseHandler   *handler,
                         time_t             timeout )
      {
        (void)url; (void)mode;
        return Open( flags, handler, timeout );
      }


      //------------------------------------------------------------------------
      //!
      //------------------------------------------------------------------------
      XRootDStatus Close( ResponseHandler *handler,
                                 time_t                  timeout )
      {
        if( writer )
        {
          writer->Close( ResponseHandler::Wrap( [this, handler]( XRootDStatus *st, AnyObject *rsp )
            {
              writer.reset();
              if( st->IsOK() && bool( cksHelper ) )
              {
                std::string commit =  redir.GetPath()
                                   + "?xrdec.objid=" + objcfg->obj
                                   + "&xrdec.close=true&xrdec.size=" + std::to_string( curroff );
                if( cksHelper )
                {
                  std::string ckstype = cksHelper->GetType();
                  std::string cksval;
                  auto st = cksHelper->GetCheckSum( cksval, ckstype );
                  if( !st.IsOK() )
                  {
                    handler->HandleResponse( new XRootDStatus( st ), nullptr );
                    return;
                  }
                  commit += "&xrdec.cksum=" + cksval;
                }
                Buffer arg; arg.FromString( commit );
                auto st = fs.Query( QueryCode::OpaqueFile, arg, handler );
                if( !st.IsOK() ) handler->HandleResponse( new XRootDStatus( st ), nullptr );
                return;
              }
              handler->HandleResponse( st, rsp );
            } ), timeout );
          return XRootDStatus();
        }
    
        if( reader )
        {
          reader->Close( ResponseHandler::Wrap( [this, handler]( XRootDStatus *st, AnyObject *rsp )
            {
              reader.reset();
              handler->HandleResponse( st, rsp );
            } ), timeout );
          return XRootDStatus();
        }
    
        return XRootDStatus( stError, errNotSupported );
      }

      //------------------------------------------------------------------------
      //!
      //------------------------------------------------------------------------
      XRootDStatus Stat( bool             force,
                         ResponseHandler *handler,
                         time_t           timeout )
      {

        if( !objcfg->nomtfile )
          return fs.Stat( redir.GetPath(), handler, timeout );

        if( !force && statcache )
        {
          auto rsp = StatRsp( statcache->GetSize() );
          Schedule( handler, rsp );
          return XRootDStatus();
        }

        if( writer )
        {
          statcache.reset( new StatInfo() );
          statcache->SetSize( writer->GetSize() );
          auto rsp = StatRsp( statcache->GetSize() );
          Schedule( handler, rsp );
          return XRootDStatus();
        }

        if( reader )
        {
          statcache.reset( new StatInfo() );
          statcache->SetSize( reader->GetSize() );
          auto rsp = StatRsp( statcache->GetSize() );
          Schedule( handler, rsp );
          return XRootDStatus();
        }

        return XRootDStatus( stError, errInvalidOp, 0, "File not open." );
      }

      //------------------------------------------------------------------------
      //!
      //------------------------------------------------------------------------
      XRootDStatus Read( uint64_t                offset,
                                uint32_t                size,
                                void                   *buffer,
                                ResponseHandler *handler,
                                time_t                  timeout )
      {
        if( !reader ) return XRootDStatus( stError, errInternal );
    
        reader->Read( offset, size, buffer, handler, timeout );
        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! @see XrdCl::File::PgRead - async
      //------------------------------------------------------------------------
      XRootDStatus PgRead(uint64_t offset, uint32_t size, void *buffer,
                                    ResponseHandler *handler,
                                    time_t timeout)
      {
        ResponseHandler *substitHandler = new EcPgReadResponseHandler( handler );
        XRootDStatus st = Read(offset, size, buffer, substitHandler, timeout);
        return st;
      }
    

      //------------------------------------------------------------------------
      //! @see File::Write
      //------------------------------------------------------------------------
      XRootDStatus Write( uint64_t                offset,
                                 uint32_t                size,
                                 const void             *buffer,
                                 ResponseHandler *handler,
                                 time_t                  timeout )
      {
        if( cksHelper )
          cksHelper->Update( buffer, size );

        if( !writer ) return XRootDStatus( stError, errInternal );
        if( offset != curroff ) return XRootDStatus( stError, errNotSupported );
        writer->Write( size, buffer, handler );
        curroff += size;
        return XRootDStatus();
      }
    
      //------------------------------------------------------------------------
      //! @see XrdCl::File::PgWrite - async
      //------------------------------------------------------------------------
      XRootDStatus PgWrite( uint64_t               offset,
                            uint32_t               size,
                            const void            *buffer,
                            std::vector<uint32_t> &cksums,
                            ResponseHandler       *handler,
                            time_t                 timeout = 0 )
      {
        if(! cksums.empty() )
        {
          const char *data = static_cast<const char*>( buffer );
          std::vector<uint32_t> local_cksums;
          XrdOucPgrwUtils::csCalc( data, offset, size, local_cksums );
          if (data) delete data;
          if (local_cksums != cksums)
            return XRootDStatus( stError, errInvalidArgs, 0, "data and crc32c digests do not match." );
        }
        return Write(offset, size, buffer, handler, timeout); 
      }

      //------------------------------------------------------------------------
      //!
      //------------------------------------------------------------------------
      bool IsOpen() const
      {
        return writer || reader;
      }  

    private:

      inline XRootDStatus LoadPlacement()
      {
        LocationInfo *infoAll = nullptr;
        XRootDStatus st = fs.DeepLocate( "*", OpenFlags::PrefName, infoAll );
        std::unique_ptr<LocationInfo> ptr( infoAll );
        if( !st.IsOK() ) return st;

        LocationInfo *info = new LocationInfo();
        std::unique_ptr<LocationInfo> ptr1( info );

        static ServerSpaceInfo ssi;
        ssi.SelectLocations(*infoAll, *info, objcfg->nbchunks);

        if( info->GetSize() < objcfg->nbchunks )
          return XRootDStatus( stError, errInvalidOp, 0, "Too few data servers." );
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        shuffle (info->Begin(), info->End(), std::default_random_engine(seed));
        for( size_t i = 0; i < objcfg->nbchunks; ++i )
        {
          auto &location = info->At( i );
          objcfg->plgr.emplace_back( "root://" + location.GetAddress() + '/' );
        }
        return XRootDStatus();
      }

      inline XRootDStatus LoadPlacement( const std::string &path )
      {
        LocationInfo *info = nullptr;
        XRootDStatus st = fs.DeepLocate( "*", OpenFlags::PrefName, info );
        std::unique_ptr<LocationInfo> ptr( info );
        if( !st.IsOK() ) return st;
        // The following check become meaningless
        if( info->GetSize() < objcfg->nbdata )
          return XRootDStatus( stError, errInvalidOp, 0, "Too few data servers." );

        uint64_t verNumMax = 0;
        std::vector<uint64_t> verNums;
        std::vector<std::string>  xattrkeys;
        std::vector<XrdCl::XAttr> xattrvals;
        xattrkeys.push_back("xrdec.strpver");
        for( size_t i = 0; i < info->GetSize(); ++i )
        {
          FileSystem *fs_i = new FileSystem(info->At( i ).GetAddress());
          xattrvals.clear();
          st = fs_i->GetXAttr(path, xattrkeys, xattrvals, 0);
          if (st.IsOK() && ! xattrvals[0].value.empty())
          {
            std::stringstream sstream(xattrvals[0].value);
            uint64_t verNum;
            sstream >> verNum;
            verNums.push_back(verNum);
            if (verNum > verNumMax) 
              verNumMax = verNum;
          }
          else
            verNums.push_back(0);
          delete fs_i;
        }

        int n = 0;
        for( size_t i = 0; i < info->GetSize(); ++i )
        {
          if ( verNums.at(i) == 0 || verNums.at(i) != verNumMax )
            continue; 
          else
            n++;
          auto &location = info->At( i );
          objcfg->plgr.emplace_back( "root://" + location.GetAddress() + '/' );
        }
        if (n < objcfg->nbdata )
          return XRootDStatus( stError, errInvalidOp, 0, "Too few data servers." );
        return XRootDStatus();
      }

      inline static AnyObject* StatRsp( uint64_t size )
      {
        StatInfo *info = new StatInfo();
        info->SetSize( size );
        AnyObject *rsp = new AnyObject();
        rsp->Set( info );
        return rsp;
      }

      inline static void Schedule( ResponseHandler *handler, AnyObject *rsp )
      {
        ResponseJob *job = new ResponseJob( handler, new XRootDStatus(), rsp, nullptr );
        XrdCl::DefaultEnv::GetPostMaster()->GetJobManager()->QueueJob( job );
      }

      URL                                redir;
      FileSystem                         fs;
      std::unique_ptr<XrdEc::ObjCfg>     objcfg;
      std::unique_ptr<XrdEc::StrmWriter> writer;
      std::unique_ptr<XrdEc::Reader>     reader;
      uint64_t                           curroff;
      std::unique_ptr<CheckSumHelper>    cksHelper;
      std::unique_ptr<StatInfo>          statcache;

  };

  //----------------------------------------------------------------------------
  //! Plugin factory
  //----------------------------------------------------------------------------
  class EcPlugInFactory : public PlugInFactory
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      EcPlugInFactory( uint8_t nbdta, uint8_t nbprt, uint64_t chsz,
                       std::vector<std::string> && plgr ) :
        nbdta( nbdta ), nbprt( nbprt ), chsz( chsz ), plgr( std::move( plgr ) )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~EcPlugInFactory()
      {
      }

      //------------------------------------------------------------------------
      //! Create a file plug-in for the given URL
      //------------------------------------------------------------------------
      virtual FilePlugIn *CreateFile( const std::string &u )
      {
        URL url( u );
        XrdEc::ObjCfg *objcfg = new XrdEc::ObjCfg( url.GetPath(), nbdta, nbprt,
                                                   chsz, false, true );
        objcfg->plgr = std::move( plgr );
        return new EcHandler( url, objcfg, nullptr );
      }

      //------------------------------------------------------------------------
      //! Create a file system plug-in for the given URL
      //------------------------------------------------------------------------
      virtual FileSystemPlugIn *CreateFileSystem( const std::string &url )
      {
        return nullptr;
      }

    private:
      uint8_t  nbdta;
      uint8_t  nbprt;
      uint64_t chsz;
      std::vector<std::string> plgr;
  };

  EcHandler* GetEcHandler( const URL &headnode, const URL &redirurl );

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLECHANDLER_HH_ */

