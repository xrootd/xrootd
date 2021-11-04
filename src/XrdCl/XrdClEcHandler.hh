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

#include "XrdEc/XrdEcReader.hh"
#include "XrdEc/XrdEcStrmWriter.hh"

#include <memory>

namespace XrdCl
{
  class EcHandler : public FilePlugIn
  {
    public:
      EcHandler( const URL                       &redir,
                 XrdEc::ObjCfg                   *objcfg,
                 std::unique_ptr<CheckSumHelper>  cksHelper ) : redir( redir ),
                                                                fs( redir ),
                                                                objcfg( objcfg ),
                                                                curroff( 0 ),
                                                                cksHelper( std::move( cksHelper ) )
      {
      }

      virtual ~EcHandler()
      {
      }

      XRootDStatus Open( uint16_t                  flags,
                         XrdCl::ResponseHandler   *handler,
                         uint16_t                  timeout )
      {
        // TODO if plgr is empty issue locate to figure out the plgr

        if( ( flags & XrdCl::OpenFlags::Write ) || ( flags & XrdCl::OpenFlags::Update ) )
        {
          if( !( flags & XrdCl::OpenFlags::New )    || // it has to be a new file
               ( flags & XrdCl::OpenFlags::Delete ) || // truncation is not supported
               ( flags & XrdCl::OpenFlags::Read ) )    // write + read is not supported
            return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported ); // TODO call handler
          writer.reset( new XrdEc::StrmWriter( *objcfg ) );
          writer->Open( handler, timeout );
          return XrdCl::XRootDStatus();
        } 
    
        if( flags & XrdCl::OpenFlags::Read )
        {
          if( flags & XrdCl::OpenFlags::Write )
            return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported ); // TODO call handler
    
          reader.reset( new XrdEc::Reader( *objcfg ) );
          reader->Open( handler, timeout );
          return XrdCl::XRootDStatus(); 
        }

        return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported ); // TODO call handler
      }

      XRootDStatus Open( const std::string &url,
                                 OpenFlags::Flags   flags,
                                 Access::Mode       mode,
                                 ResponseHandler   *handler,
                                 uint16_t           timeout )
      {
        (void)url; (void)mode;
        return Open( flags, handler, timeout );
      }


      //------------------------------------------------------------------------
      //!
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus Close( XrdCl::ResponseHandler *handler,
                                 uint16_t                timeout )
      {
        if( writer )
        {
          writer->Close( XrdCl::ResponseHandler::Wrap( [this, handler]( XrdCl::XRootDStatus *st, XrdCl::AnyObject *rsp )
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
          return XrdCl::XRootDStatus();
        }
    
        if( reader )
        {
          reader->Close( XrdCl::ResponseHandler::Wrap( [this, handler]( XrdCl::XRootDStatus *st, XrdCl::AnyObject *rsp )
            {
              reader.reset();
              handler->HandleResponse( st, rsp );
            } ), timeout );
          return XrdCl::XRootDStatus();
        }
    
        return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported );
      }

      //------------------------------------------------------------------------
      //!
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus Stat( bool                    force,
                                XrdCl::ResponseHandler *handler,
                                uint16_t                timeout )
      {
        return fs.Stat( redir.GetPath(), handler, timeout );
      }

      //------------------------------------------------------------------------
      //!
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus Read( uint64_t                offset,
                                uint32_t                size,
                                void                   *buffer,
                                XrdCl::ResponseHandler *handler,
                                uint16_t                timeout )
      {
        if( !reader ) return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInternal );
    
        reader->Read( offset, size, buffer, handler, timeout );
        return XrdCl::XRootDStatus();
      }
    
      //------------------------------------------------------------------------
      //! @see XrdCl::File::Write
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus Write( uint64_t                offset,
                                 uint32_t                size,
                                 const void             *buffer,
                                 XrdCl::ResponseHandler *handler,
                                 uint16_t                timeout )
      {
        if( cksHelper )
          cksHelper->Update( buffer, size );

        if( !writer ) return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInternal );
        if( offset != curroff ) return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported );
        writer->Write( size, buffer, handler );
        curroff += size;
        return XrdCl::XRootDStatus();
      }
    
      //------------------------------------------------------------------------
      //!
      //------------------------------------------------------------------------
      bool IsOpen() const
      {
        return writer || reader;
      }  

    private:

      URL                                redir;
      FileSystem                         fs;
      std::unique_ptr<XrdEc::ObjCfg>     objcfg;
      std::unique_ptr<XrdEc::StrmWriter> writer;
      std::unique_ptr<XrdEc::Reader>     reader;
      uint64_t                           curroff;
      std::unique_ptr<CheckSumHelper>    cksHelper;

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

