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

      XrdCl::XRootDStatus Open( uint16_t                  flags,
                                XrdCl::ResponseHandler   *handler,
                                uint16_t                  timeout )
      {
        if( ( flags & XrdCl::OpenFlags::Write ) || ( flags & XrdCl::OpenFlags::Update ) )
        {
          if( !( flags & XrdCl::OpenFlags::New )    || // it has to be a new file
               ( flags & XrdCl::OpenFlags::Delete ) || // truncation is not supported
               ( flags & XrdCl::OpenFlags::Read ) )    // write + read is not supported
            return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported );
    
          writer.reset( new XrdEc::StrmWriter( *objcfg ) );
          writer->Open( handler, timeout );
          return XrdCl::XRootDStatus();
        } 
    
        if( flags & XrdCl::OpenFlags::Read )
        {
          if( flags & XrdCl::OpenFlags::Write )
            return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported );
    
          reader.reset( new XrdEc::Reader( *objcfg ) );
          reader->Open( handler, timeout );
          return XrdCl::XRootDStatus(); 
        }
    
        return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported );
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

  EcHandler* GetEcHandler( const URL &headnode, const URL &redirurl )
  {
    const URL::ParamsMap &params = redirurl.GetParams();
    // make sure all the xrdec. tokens are present and the values are sane
    URL::ParamsMap::const_iterator itr = params.find( "xrdec.nbdta" );
    if( itr == params.end() ) return nullptr;
    uint8_t nbdta = std::stoul( itr->second );

    itr = params.find( "xrdec.nbprt" );
    if( itr == params.end() ) return nullptr;
    uint8_t nbprt = std::stoul( itr->second );

    itr = params.find( "xrdec.blksz" );
    if( itr == params.end() ) return nullptr;
    uint64_t blksz = std::stoul( itr->second );

    itr = params.find( "xrdec.plgr" );
    if( itr == params.end() ) return nullptr;
    std::vector<std::string> plgr;
    Utils::splitString( plgr, itr->second, "," );
    if( plgr.size() < nbdta + nbprt ) return nullptr;

    itr = params.find( "xrdec.objid" );
    if( itr == params.end() ) return nullptr;
    std::string objid = itr->second;

    itr = params.find( "xrdec.format" );
    if( itr == params.end() ) return nullptr;
    size_t format = std::stoul( itr->second );
    if( format != 1 ) return nullptr; // TODO use constant

    std::vector<std::string> dtacgi;
    itr = params.find( "xrdec.dtacgi" );
    if( itr != params.end() )
    {
      Utils::splitString( dtacgi, itr->second, "," );
      if( plgr.size() != dtacgi.size() ) return nullptr;
    }

    std::vector<std::string> mdtacgi;
    itr = params.find( "xrdec.mdtacgi" );
    if( itr != params.end() ) 
    {
      Utils::splitString( mdtacgi, itr->second, "," );
      if( plgr.size() != mdtacgi.size() ) return nullptr;
    }

    itr = params.find( "xrdec.cosc" );
    if( itr == params.end() ) return nullptr;
    std::string cosc_str = itr->second;
    if( cosc_str != "true" && cosc_str != "false" ) return nullptr;
    bool cosc = cosc_str == "true";

    itr = params.find( "xrdec.cksum" );
    if( cosc && itr == params.end() ) return nullptr;
    std::string ckstype = itr->second;

    std::string chdigest;
    itr = params.find( "xrdec.chdigest" );
    if( itr == params.end() )
      chdigest = "crc32c";
    else
      chdigest = itr->second;
    bool usecrc32c = ( chdigest == "crc32c" );

    XrdEc::ObjCfg *objcfg = new XrdEc::ObjCfg( objid, nbdta, nbprt, blksz / nbdta, usecrc32c );
    objcfg->plgr    = std::move( plgr );
    objcfg->dtacgi  = std::move( dtacgi );
    objcfg->mdtacgi = std::move( mdtacgi );

    std::unique_ptr<CheckSumHelper> cksHelper( cosc ? new CheckSumHelper( "", ckstype ) : nullptr );
    if( cksHelper )
    {
      auto st = cksHelper->Initialize();
      if( !st.IsOK() ) return nullptr;
    }

    return new EcHandler( headnode, objcfg, std::move( cksHelper ) );
  }

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLECHANDLER_HH_ */

