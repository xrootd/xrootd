//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Krzysztof Jamrog <krzysztof.piotr.jamrog@cern.ch>,
//         Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_OPERATION_HANDLERS_HH__
#define __XRD_CL_OPERATION_HANDLERS_HH__

#include "XrdCl/XrdClOperations.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //----------------------------------------------------------------------------
  class SimpleFunctionWrapper: public ForwardingHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      SimpleFunctionWrapper(
          std::function<void( XrdCl::XRootDStatus& )> handleFunction ) :
          fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XrdCl::XRootDStatus *status,
          XrdCl::AnyObject *response )
      {
        fun( *status );
        delete status;
        delete response;
        delete this;
      }

    private:
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<void( XrdCl::XRootDStatus& )> fun;
  };

  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //----------------------------------------------------------------------------
  class SimpleForwardingFunctionWrapper: public ForwardingHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      SimpleForwardingFunctionWrapper(
          std::function<void( XrdCl::XRootDStatus&, OperationContext& )> handleFunction ) :
          fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XrdCl::XRootDStatus *status,
          XrdCl::AnyObject *response )
      {
        auto paramsContainerWrapper = GetOperationContext();
        fun( *status, *paramsContainerWrapper.get() );
        delete status;
        delete response;
        delete this;
      }

    private:
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<void( XrdCl::XRootDStatus&, OperationContext& )> fun;
  };

  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //!
  //! @arg ResponseType : type of response returned by the server
  //----------------------------------------------------------------------------
  template<typename ResponseType>
  class FunctionWrapper: public ForwardingHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      FunctionWrapper(
          std::function<void( XrdCl::XRootDStatus&, ResponseType& )> handleFunction ) :
          fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XrdCl::XRootDStatus *status,
          XrdCl::AnyObject *response )
      {
        ResponseType *res = nullptr;
        if( status->IsOK() )
          response->Get( res );
        else
          res = &nullref;
        fun( *status, *res );
        delete status;
        delete response;
        delete this;
      }

    private:
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<void( XrdCl::XRootDStatus&, ResponseType& )> fun;

      //------------------------------------------------------------------------
      //! Null reference to the response (not really but acts as one)
      //------------------------------------------------------------------------
      static ResponseType nullref;
  };

  //----------------------------------------------------------------------------
  // Initialize the 'null-reference'
  //----------------------------------------------------------------------------
  template<typename ResponseType>
  ResponseType FunctionWrapper<ResponseType>::nullref;

  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //----------------------------------------------------------------------------
  template<typename ResponseType>
  class ForwardingFunctionWrapper: public ForwardingHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      ForwardingFunctionWrapper(
          std::function<
              void( XrdCl::XRootDStatus&, ResponseType&, OperationContext& )> handleFunction ) :
          fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XrdCl::XRootDStatus *status,
          XrdCl::AnyObject *response )
      {
        ResponseType *res = nullptr;
        if( status->IsOK() )
          response->Get( res );
        else
          res = &nullref;
        auto paramsContainerWrapper = GetOperationContext();
        fun( *status, *res, *paramsContainerWrapper.get() );
        delete status;
        delete response;
        delete this;
      }

    private:
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<
          void( XrdCl::XRootDStatus&, ResponseType&, OperationContext &wrapper )> fun;

      //------------------------------------------------------------------------
      //! Null reference to the response (not really but acts as one)
      //------------------------------------------------------------------------
      static ResponseType nullref;
  };

  //----------------------------------------------------------------------------
  // Initialize the 'null-reference'
  //----------------------------------------------------------------------------
  template<typename ResponseType>
  ResponseType ForwardingFunctionWrapper<ResponseType>::nullref;

  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //----------------------------------------------------------------------------
  class ExOpenFuncWrapper: public ForwardingHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      ExOpenFuncWrapper( File &f,
          std::function<void( XrdCl::XRootDStatus&, StatInfo& )> handleFunction ) :
          f( f ), fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XrdCl::XRootDStatus *status,
          XrdCl::AnyObject *response )
      {
        StatInfo *info = nullptr;
        if( status->IsOK() )
          f.Stat( false, info );
        else
          info = &nullref;
        fun( *status, *info );
        if( info != &nullref ) delete info;
        delete status;
        delete response;
        delete this;
      }

    private:
      File &f;
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<void( XrdCl::XRootDStatus&, StatInfo& )> fun;

      //------------------------------------------------------------------------
      //! Null reference to the response (not really but acts as one)
      //------------------------------------------------------------------------
      static StatInfo nullref;
  };

  //----------------------------------------------------------------------------
  // Initialize the 'null-reference'
  //----------------------------------------------------------------------------
  StatInfo ExOpenFuncWrapper::nullref;

  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //----------------------------------------------------------------------------
  class ForwardingExOpenFuncWrapper: public ForwardingHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      ForwardingExOpenFuncWrapper( File &f,
          std::function<
              void( XrdCl::XRootDStatus&, StatInfo&, OperationContext& )> handleFunction ) :
          f( f ), fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XrdCl::XRootDStatus *status,
          XrdCl::AnyObject *response )
      {
        StatInfo *info = nullptr;
        if( status->IsOK() )
          f.Stat( false, info );
        else
          info = &nullref;
        auto paramsContainerWrapper = GetOperationContext();
        fun( *status, *info, *paramsContainerWrapper.get() );
        if( info != &nullref ) delete info;
        delete status;
        delete response;
        delete this;
      }

    private:
      File &f;
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<void( XrdCl::XRootDStatus&, StatInfo&, OperationContext& )> fun;

      //------------------------------------------------------------------------
      //! Null reference to the response (not really but acts as one)
      //------------------------------------------------------------------------
      static StatInfo nullref;
  };

  //----------------------------------------------------------------------------
  // Initialize the 'null-reference'
  //----------------------------------------------------------------------------
  StatInfo ForwardingExOpenFuncWrapper::nullref;

}

#endif // __XRD_CL_OPERATIONS_HANDLERS_HH__
