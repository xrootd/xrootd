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
        return "OpenArchive";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( uint16_t pipelineTimeout )
      {
        try
        {
          std::string      url     = std::get<UrlArg>( this->args ).Get();
          OpenFlags::Flags flags   = std::get<FlagsArg>( this->args ).Get();
          uint16_t         timeout = pipelineTimeout < this->timeout ?
                                     pipelineTimeout : this->timeout;
          return this->zip->OpenArchive( url, flags, this->handler.get(), timeout );
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
  typedef OpenArchiveImpl<false> OpenArchive;

  //----------------------------------------------------------------------------
  //! OpenFile operation (@see ZipOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class OpenFileImpl: public ZipOperation<OpenFileImpl, HasHndl, Resp<void>,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using ZipOperation<OpenFileImpl, HasHndl, Resp<void>, Arg<std::string>>::ZipOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { FnArg, };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "OpenFile";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( uint16_t pipelineTimeout )
      {
        try
        {
          std::string fn      = std::get<FnArg>( this->args ).Get();
          uint16_t    timeout = pipelineTimeout < this->timeout ?
                                     pipelineTimeout : this->timeout;
          return this->zip->OpenFile( fn, OpenFlags::None, 0, 0, this->handler.get(), timeout );
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
  inline OpenFileImpl<false> OpenFile( ZIP &zip, Arg<std::string> fn )
  {
    return OpenFileImpl<false>( zip, std::move( fn ) );
  }

  //----------------------------------------------------------------------------
  //! OpenFile operation (@see ZipOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class OpenFileImpl2: public ZipOperation<OpenFileImpl2, HasHndl, Resp<void>,
      Arg<std::string>, Arg<OpenFlags::Flags>, Arg<uint64_t>, Arg<uint32_t>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using ZipOperation<OpenFileImpl2, HasHndl, Resp<void>, Arg<std::string>,
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
        return "OpenFile";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( uint16_t pipelineTimeout )
      {
        try
        {
          std::string      fn      = std::get<FnArg>( this->args ).Get();
          OpenFlags::Flags flags   = std::get<FlagsArg>( this->args ).Get();
          uint64_t         size    = std::get<SizeArg>( this->args ).Get();
          uint32_t         crc32   = std::get<Crc32Arg>( this->args ).Get();
          uint16_t         timeout = pipelineTimeout < this->timeout ?
                                     pipelineTimeout : this->timeout;
          return this->zip->OpenFile( fn, flags, size, crc32, this->handler.get(), timeout );
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
  //! Factory for creating OpenFileImpl2 objects
  //----------------------------------------------------------------------------
  template<typename ZIP>
  inline OpenFileImpl2<false> OpenFile( ZIP &zip, Arg<std::string> fn,
      Arg<OpenFlags::Flags> flags, Arg<uint64_t> size, Arg<uint32_t> crc32 )
  {
    return OpenFileImpl2<false>( zip, std::move( fn ), std::move( flags ),
        std::move( size ), std::move( crc32 ) );
  }

}

#endif /* SRC_XRDCL_XRDCLZIPOPERATIONS_HH_ */
