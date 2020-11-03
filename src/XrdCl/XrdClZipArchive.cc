/*
 * XrdClZipArchive.cpp
 *
 *  Created on: 2 Nov 2020
 *      Author: simonm
 */

#include "XrdClZipArchive.hh"

#include "XrdCl/XrdClZipArchiveReader.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClResponseJob.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPostMaster.hh"

namespace XrdCl
{
  struct ZipArchiveImpl
  {
    virtual ~ZipArchiveImpl()
    {
    }

    virtual XRootDStatus OpenArchive( const std::string &url,
                                      ResponseHandler   *handler,
                                      uint16_t           timeout ) = 0;

    virtual XRootDStatus OpenFile( const std::string &fn,
                                   ResponseHandler   *handler,
                                   uint16_t           timeout ) = 0;

    virtual XRootDStatus Read( uint64_t           offset,
                               uint32_t           size,
                               void              *buffer,
                               ResponseHandler   *handler,
                               uint16_t           timeout ) = 0;

    virtual XRootDStatus Write( uint64_t           offset,
                                uint32_t           size,
                                const void        *buffer,
                                ResponseHandler   *handler,
                                uint16_t           timeout ) = 0;

    virtual XRootDStatus Stat( ResponseHandler   *handler,
                               uint16_t           timeout ) = 0;

    virtual XRootDStatus CloseArchive( ResponseHandler *handler,
                                       uint16_t         timeout ) = 0;

    virtual XRootDStatus CloseFile( ResponseHandler *handler,
                                    uint16_t         timeout ) = 0;

    virtual XRootDStatus List( ResponseHandler *handler,
                               uint16_t         timeout ) = 0;

    File archive;
  };

  struct ZipArchiveRead : public ZipArchiveImpl
  {
    ZipArchiveRead() : reader( archive )
    {
    }

    XRootDStatus OpenArchive( const std::string &url,
                              ResponseHandler   *handler,
                              uint16_t           timeout )
    {
      return reader.Open( url, handler, timeout );
    }

    XRootDStatus OpenFile( const std::string &fn,
                           ResponseHandler   *handler,
                           uint16_t           timeout )
    {
      XRootDStatus st = reader.Bind( fn );
      ResponseJob *job = new ResponseJob( handler, new XRootDStatus( st ), nullptr, nullptr );
      JobManager *jobMan = DefaultEnv::GetPostMaster()->GetJobManager();
      jobMan->QueueJob( job );
      return XRootDStatus();
    }

    XRootDStatus Read( uint64_t           offset,
                       uint32_t           size,
                       void              *buffer,
                       ResponseHandler   *handler,
                       uint16_t           timeout )
    {
      return reader.Read( offset, size, buffer, handler, timeout );
    }

    XRootDStatus Write( uint64_t           offset,
                        uint32_t           size,
                        const void        *buffer,
                        ResponseHandler   *handler,
                        uint16_t           timeout )
    {
      return XRootDStatus( stError, errInvalidOp );
    }

    XRootDStatus Stat( ResponseHandler   *handler,
                       uint16_t           timeout )
    {
      uint64_t size = 0;
      XRootDStatus st = reader.GetSize( size );
      if( !st.IsOK() ) return st;
      StatInfo *info = new StatInfo( "", size, 0, 0 ); // TODO create more meaningful StatInfo
      AnyObject *rsp = new AnyObject();
      rsp->Set( info );
      ResponseJob *job = new ResponseJob( handler, new XRootDStatus(), rsp, nullptr );
      JobManager *jobMan = DefaultEnv::GetPostMaster()->GetJobManager();
      jobMan->QueueJob( job );
      return XRootDStatus();
    }

    XRootDStatus CloseArchive( ResponseHandler *handler,
                               uint16_t         timeout )
    {
      return reader.Close( handler, timeout );
    }

    XRootDStatus CloseFile( ResponseHandler *handler,
                            uint16_t         timeout )
    {
      reader.Unbind();
      ResponseJob *job = new ResponseJob( handler, new XRootDStatus(), nullptr, nullptr );
      JobManager *jobMan = DefaultEnv::GetPostMaster()->GetJobManager();
      jobMan->QueueJob( job );
      return XRootDStatus();
    }

    XRootDStatus List( ResponseHandler *handler,
                       uint16_t         timeout )
    {
      DirectoryList *list = 0;
      XRootDStatus st = reader.List( list );
      if( !st.IsOK() ) return st;
      AnyObject *rsp = new AnyObject();
      rsp->Set( list );
      ResponseJob *job = new ResponseJob( handler, new XRootDStatus(), rsp, nullptr );
      JobManager *jobMan = DefaultEnv::GetPostMaster()->GetJobManager();
      jobMan->QueueJob( job );
      return XRootDStatus();
    }

    ZipArchiveReader reader;
  };

//  struct ZipArchiveNew : public ZipArchiveImpl
//  {
//
//  };
//
//  struct ZipArchiveAppend : public ZipArchiveImpl
//  {
//
//  };


  ZipArchive::ZipArchive()
  {

  }

  ZipArchive::~ZipArchive()
  {

  }

  XRootDStatus ZipArchive::OpenArchive( const std::string &url,
                                        ZipFlags::Flags    flags,
                                        ResponseHandler   *handler,
                                        uint16_t           timeout )
  {
    if( flags & ZipFlags::Read )
    {
      pImpl.reset( new ZipArchiveRead() );
    }
//    else if( flags & ZipFlags::Append )
//    {
//      if( ( flags & ZipFlags::Delete ) || ( flags & ZipFlags::New ) )
//      {
//        pImpl.reset( new ZipArchiveNew() );
//        // TODO
//      }
//      else
//      {
//        pImpl.reset( new ZipArchiveAppend() );
//        // TODO
//      }
//
//    }

    if( pImpl )
      return pImpl->OpenArchive( url, handler, timeout );

    return XRootDStatus( stError, errInvalidArgs );
  }

  XRootDStatus ZipArchive::OpenFile( const std::string &fn,
                                     ResponseHandler   *handler,
                                     uint16_t           timeout )
  {
    if( pImpl )
      return pImpl->OpenFile( fn, handler, timeout );

    return XRootDStatus( stError, errInvalidOp );
  }

  XRootDStatus ZipArchive::Read( uint64_t           offset,
                                 uint32_t           size,
                                 void              *buffer,
                                 ResponseHandler   *handler,
                                 uint16_t           timeout )
  {
    if( pImpl )
      return pImpl->Read( offset, size, buffer, handler, timeout );

    return XRootDStatus( stError, errInvalidOp );
  }

  XRootDStatus ZipArchive::Write( uint64_t           offset,
                                  uint32_t           size,
                                  const void        *buffer,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
  {
    if( pImpl )
      return pImpl->Write( offset, size, buffer, handler, timeout );

    return XRootDStatus( stError, errInvalidOp );
  }



  XRootDStatus ZipArchive::List( ResponseHandler *handler,
                                 uint16_t         timeout )
  {
    if( pImpl )
      return pImpl->List( handler, timeout );

    return XRootDStatus( stError, errInvalidOp );
  }

  XRootDStatus ZipArchive::Stat( ResponseHandler   *handler,
                                 uint16_t           timeout )
  {
    if( pImpl )
      return pImpl->Stat( handler, timeout );

    return XRootDStatus( stError, errInvalidOp );
  }

  XRootDStatus ZipArchive::CloseArchive( ResponseHandler *handler,
                                         uint16_t         timeout )
  {
    if( pImpl )
      return pImpl->CloseArchive( handler, timeout );

    return XRootDStatus( stError, errInvalidOp );
  }

  XRootDStatus ZipArchive::CloseFile( ResponseHandler *handler,
                                      uint16_t         timeout )
  {
    if( pImpl )
      return pImpl->CloseFile( handler, timeout );

    return XRootDStatus( stError, errInvalidOp );
  }

} /* namespace XrdCl */
