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
}

#endif /* SRC_XRDCL_XRDCLZIPOPERATIONS_HH_ */
