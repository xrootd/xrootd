/*
 * XrdClZipOperations.hh
 *
 *  Created on: 26 Nov 2020
 *      Author: simonm
 */

#ifndef SRC_XRDCL_XRDCLZIPOPERATIONS_HH_
#define SRC_XRDCL_XRDCLZIPOPERATIONS_HH_

#include "XrdCl/XrdClZipArchive.hh"
#include "XrdCl/XrdClOperations.hh"
#include "XrdCl/XrdClOperationHandlers.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Base class for all zip archive related operations
  //!
  //! @arg Derived : the class that derives from this template (CRTP)
  //! @arg HasHndl : true if operation has a handler, false otherwise
  //! @arg Args    : operation arguments
  //----------------------------------------------------------------------------
  template<template<bool> class Derived, bool HasHndl, typename Response, typename ... Arguments>
  class ZipOperation: public ConcreteOperation<Derived, HasHndl, Response, Arguments...>
  {

      template<template<bool> class, bool, typename, typename ...> friend class ZipOperation;

    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param f    : file on which the operation will be performed
      //! @param args : file operation arguments
      //------------------------------------------------------------------------
      ZipOperation( ZipArchive *zip, Arguments... args): ConcreteOperation<Derived, false, Response, Arguments...>( std::move( args )... ), zip( zip )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param f    : file on which the operation will be performed
      //! @param args : file operation arguments
      //------------------------------------------------------------------------
      ZipOperation( ZipArchive &zip, Arguments... args): ZipOperation( &zip, std::move( args )... )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<bool from>
      ZipOperation( ZipOperation<Derived, from, Response, Arguments...> && op ) :
        ConcreteOperation<Derived, HasHndl, Response, Arguments...>( std::move( op ) ), zip( op.zip )
      {

      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~ZipOperation()
      {

      }

    protected:

      //------------------------------------------------------------------------
      //! The file object itself
      //------------------------------------------------------------------------
      ZipArchive *zip;
  };

  //----------------------------------------------------------------------------
  //! OpenArchive operation (@see ZipOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class OpenArchiveImpl: public ZipOperation<OpenArchiveImpl, HasHndl, Resp<void>,
      Arg<std::string>, Arg<OpenFlags::Flags>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using ZipOperation<OpenArchiveImpl, HasHndl, Resp<void>, Arg<std::string>,
                          Arg<OpenFlags::Flags>>::ZipOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { UrlArg, FlagsArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "ZipOpen";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, uint16_t pipelineTimeout )
      {
        try
        {
          std::string      url     = std::get<UrlArg>( this->args ).Get();
          OpenFlags::Flags flags   = std::get<FlagsArg>( this->args ).Get();
          uint16_t         timeout = pipelineTimeout < this->timeout ?
                                     pipelineTimeout : this->timeout;
          return this->zip->OpenArchive( url, flags, handler, timeout );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating OpenFileImpl objects
  //----------------------------------------------------------------------------
  template<typename ZIP>
  inline OpenArchiveImpl<false> OpenArchive( ZIP &&zip, Arg<std::string> fn,
      Arg<OpenFlags::Flags> flags, uint16_t timeout = 0 )
  {
    return OpenArchiveImpl<false>( zip, std::move( fn ),
                                   std::move( flags ) ).Timeout( timeout );
  }


  //----------------------------------------------------------------------------
  //! OpenFile operation (@see ZipOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class OpenFileImpl: public ZipOperation<OpenFileImpl, HasHndl, Resp<void>,
      Arg<std::string>, Arg<OpenFlags::Flags>, Arg<uint64_t>, Arg<uint32_t>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using ZipOperation<OpenFileImpl, HasHndl, Resp<void>, Arg<std::string>,
          Arg<OpenFlags::Flags>, Arg<uint64_t>, Arg<uint32_t>>::ZipOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { FnArg, FlagsArg, SizeArg, Crc32Arg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "ZipOpenFile";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, uint16_t pipelineTimeout )
      {
        try
        {
          std::string      fn      = std::get<FnArg>( this->args ).Get();
          OpenFlags::Flags flags   = std::get<FlagsArg>( this->args ).Get();
          uint64_t         size    = std::get<SizeArg>( this->args ).Get();
          uint32_t         crc32   = std::get<Crc32Arg>( this->args ).Get();
          uint16_t         timeout = pipelineTimeout < this->timeout ?
                                     pipelineTimeout : this->timeout;
          return this->zip->OpenFile( fn, flags, size, crc32, handler, timeout );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating OpenFileImpl objects
  //----------------------------------------------------------------------------
  template<typename ZIP>
  inline OpenFileImpl<false> OpenFile( ZIP &&zip, Arg<std::string> fn,
      Arg<OpenFlags::Flags> flags = OpenFlags::None, Arg<uint64_t> size = 0,
      Arg<uint32_t> crc32 = 0, uint16_t timeout = 0 )
  {
    return OpenFileImpl<false>( zip, std::move( fn ), std::move( flags ),
        std::move( size ), std::move( crc32 ) ).Timeout( timeout );
  }


  //----------------------------------------------------------------------------
  //! Read operation (@see ZipOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class ZipReadImpl: public ZipOperation<ZipReadImpl, HasHndl, Resp<ChunkInfo>,
      Arg<uint64_t>, Arg<uint32_t>, Arg<void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using ZipOperation<ZipReadImpl, HasHndl, Resp<ChunkInfo>, Arg<uint64_t>,
          Arg<uint32_t>, Arg<void*>>::ZipOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { OffsetArg, SizeArg, BufferArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "ZipRead";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, uint16_t pipelineTimeout )
      {
        try
        {
          uint64_t  offset  = std::get<OffsetArg>( this->args ).Get();
          uint32_t  size    = std::get<SizeArg>( this->args ).Get();
          void     *buffer  = std::get<BufferArg>( this->args ).Get();
          uint16_t  timeout = pipelineTimeout < this->timeout ?
                              pipelineTimeout : this->timeout;
          return this->zip->Read( offset, size, buffer, handler, timeout );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating ArchiveReadImpl objects
  //----------------------------------------------------------------------------
  template<typename ZIP>
  inline ZipReadImpl<false> Read( ZIP &&zip, Arg<uint64_t> offset, Arg<uint32_t> size,
                                  Arg<void*> buffer, uint16_t timeout = 0 )
  {
    return ZipReadImpl<false>( zip, std::move( offset ), std::move( size ),
                               std::move( buffer ) ).Timeout( timeout );
  }


  //----------------------------------------------------------------------------
  //! Write operation (@see ZipOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class ZipWriteImpl: public ZipOperation<ZipWriteImpl, HasHndl, Resp<void>,
      Arg<uint32_t>, Arg<const void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using ZipOperation<ZipWriteImpl, HasHndl, Resp<void>, Arg<uint32_t>,
                         Arg<const void*>>::ZipOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { SizeArg, BufferArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "ZipWrite";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, uint16_t pipelineTimeout )
      {
        try
        {
          uint32_t    size    = std::get<SizeArg>( this->args ).Get();
          const void *buffer  = std::get<BufferArg>( this->args ).Get();
          uint16_t    timeout = pipelineTimeout < this->timeout ?
                                pipelineTimeout : this->timeout;
          return this->zip->Write( size, buffer, handler, timeout );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating ArchiveReadImpl objects
  //----------------------------------------------------------------------------
  template<typename ZIP>
  inline ZipWriteImpl<false> Write( ZIP &&zip, Arg<uint32_t> size, Arg<const void*> buffer,
                                    uint16_t timeout = 0 )
  {
    return ZipWriteImpl<false>( zip, std::move( size ),
                                std::move( buffer ) ).Timeout( timeout );
  }


  //----------------------------------------------------------------------------
  //! CloseFile operation (@see ZipOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class CloseFileImpl: public ZipOperation<CloseFileImpl, HasHndl, Resp<void>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using ZipOperation<CloseFileImpl, HasHndl, Resp<void>>::ZipOperation;

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "ZipCloseFile";
      }

    private:

      //------------------------------------------------------------------------
      // this is not an async operation so we don't need a handler
      //------------------------------------------------------------------------
      using ZipOperation<CloseFileImpl, HasHndl, Resp<void>>::operator>>;

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, uint16_t pipelineTimeout )
      {
        XRootDStatus st = this->zip->CloseFile();
        if( !st.IsOK() ) return st;
        handler->HandleResponse( new XRootDStatus(), nullptr );
        return XRootDStatus();
      }
  };
  typedef CloseFileImpl<false> CloseFile;


  //----------------------------------------------------------------------------
  //! ZipStat operation (@see ZipOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class ZipStatImpl: public ZipOperation<ZipStatImpl, HasHndl, Resp<StatInfo>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using ZipOperation<ZipStatImpl, HasHndl, Resp<StatInfo>>::ZipOperation;

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "ZipStat";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, uint16_t pipelineTimeout )
      {
        StatInfo *info = nullptr;
        XRootDStatus st = this->zip->Stat( info );
        if( !st.IsOK() ) return st;
        AnyObject *rsp = new AnyObject();
        rsp->Set( info );
        handler->HandleResponse( new XRootDStatus(), rsp );
        return XRootDStatus();
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating ZipStatImpl objects
  //----------------------------------------------------------------------------
  template<typename ZIP>
  inline ZipStatImpl<false> Stat( ZIP &&zip )
  {
    return ZipStatImpl<false>( zip );
  }

}

#endif /* SRC_XRDCL_XRDCLZIPOPERATIONS_HH_ */
