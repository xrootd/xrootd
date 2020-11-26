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
}

#endif /* SRC_XRDCL_XRDCLZIPOPERATIONS_HH_ */
