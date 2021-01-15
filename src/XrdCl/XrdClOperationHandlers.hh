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

#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClCtx.hh"

#include<functional>
#include<future>
#include <memory>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Helper class for unpacking single XAttrStatus from bulk response
  //----------------------------------------------------------------------------
  class UnpackXAttrStatus : public ResponseHandler
  {
    public:

      UnpackXAttrStatus( ResponseHandler *handler ) : handler( handler )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        // status maybe error for old servers not supporting xattrs
        if( !status->IsOK() )
        {
          handler->HandleResponse( status, nullptr );
          return;
        }

        std::vector<XAttrStatus> *bulk = nullptr;
        response->Get( bulk );
        *status = bulk->front().status;
        handler->HandleResponse( status, nullptr );
        delete response;
      }

    private:

      ResponseHandler *handler;
  };

  //----------------------------------------------------------------------------
  //! Helper class for unpacking single XAttr from bulk response
  //----------------------------------------------------------------------------
  class UnpackXAttr : public ResponseHandler
  {
    public:

      UnpackXAttr( ResponseHandler *handler ) : handler( handler )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        // status is always OK for bulk response

        std::vector<XAttr> *bulk = nullptr;
        response->Get( bulk );
        *status = bulk->front().status;
        std::string *rsp = new std::string( std::move( bulk->front().value ) );
        delete bulk;
        response->Set( rsp );
        handler->HandleResponse( status, response );
      }

    private:

      ResponseHandler *handler;
  };

  //----------------------------------------------------------------------------
  // Helper class for creating null references for particular types
  //
  // @arg Response : type for which we need a null reference
  //----------------------------------------------------------------------------
  template<typename Response>
  struct NullRef
  {
      static Response value;
  };

  //----------------------------------------------------------------------------
  // Initialize the 'null-reference'
  //----------------------------------------------------------------------------
  template<typename Response>
  Response NullRef<Response>::value;

  //----------------------------------------------------------------------------
  //! Unpack response
  //!
  //! @param rsp : AnyObject holding response
  //! @return    : the response
  //----------------------------------------------------------------------------
  template<typename Response>
  inline Response* GetResponse( AnyObject *rsp )
  {
    Response *ret = nullptr;
    rsp->Get( ret );
    return ret;
  }

  //----------------------------------------------------------------------------
  //! Unpack response
  //!
  //! @param rsp    : AnyObject holding response
  //! @param status :
  //! @return       : the response
  //----------------------------------------------------------------------------
  template<typename Response>
  inline Response* GetResponse( XRootDStatus *status, AnyObject *rsp )
  {
    if( !status->IsOK() ) return &NullRef<Response>::value;
    return GetResponse<Response>( rsp );
  }

  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //!
  //! @arg ResponseType : type of response returned by the server
  //----------------------------------------------------------------------------
  template<typename Response>
  class FunctionWrapper: public ResponseHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      FunctionWrapper(
          std::function<void( XRootDStatus&, Response& )> handleFunction ) :
          fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        std::unique_ptr<XRootDStatus> delst( status );
        std::unique_ptr<AnyObject> delrsp( response );
        Response *res = GetResponse<Response>( status, response );
        fun( *status, *res );
      }

    private:
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<void( XRootDStatus&, Response& )> fun;
  };

  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //!
  //! Template specialization for responses that return no value (void)
  //----------------------------------------------------------------------------
  template<>
  class FunctionWrapper<void> : public ResponseHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      FunctionWrapper(
          std::function<void( XRootDStatus& )> handleFunction ) :
          fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        std::unique_ptr<XRootDStatus> delst( status );
        std::unique_ptr<AnyObject> delrsp( response );
        fun( *status );
      }

    private:
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<void( XRootDStatus& )> fun;
  };

  //----------------------------------------------------------------------------
  //! Packaged Task wrapper
  //!
  //! @arg Response : type of response returned by the server
  //! @arg Return   : type of the value returned by the task
  //----------------------------------------------------------------------------
  template<typename Response, typename Return>
  class TaskWrapper: public ResponseHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param task : a std::packaged_task
      //------------------------------------------------------------------------
      TaskWrapper( std::packaged_task<Return( XRootDStatus&, Response& )> && task ) :
        task( std::move( task ) )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        std::unique_ptr<XRootDStatus> delst( status );
        std::unique_ptr<AnyObject> delrsp( response );
        Response *resp = GetResponse<Response>( status, response );
        task( *status, *resp );
      }

    private:

      //------------------------------------------------------------------------
      //! user defined task
      //------------------------------------------------------------------------
      std::packaged_task<Return( XRootDStatus&, Response& )> task;
  };

  //----------------------------------------------------------------------------
  //! Packaged Task wrapper, specialization for requests that have no response
  //! except for status.
  //!
  //! @arg Response : type of response returned by the server
  //! @arg Return   : type of the value returned by the task
  //----------------------------------------------------------------------------
  template<typename Return>
  class TaskWrapper<void, Return>: public ResponseHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param task : a std::packaged_task
      //------------------------------------------------------------------------
      TaskWrapper( std::packaged_task<Return( XRootDStatus& )> && task ) :
        task( std::move( task ) )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        std::unique_ptr<XRootDStatus> delst( status );
        std::unique_ptr<AnyObject> delrsp( response );
        task( *status );
      }

    private:

      //------------------------------------------------------------------------
      //! user defined task
      //------------------------------------------------------------------------
      std::packaged_task<Return( XRootDStatus& )> task;
  };


  //----------------------------------------------------------------------------
  //! Lambda wrapper
  //----------------------------------------------------------------------------
  class ExOpenFuncWrapper: public ResponseHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //
      //! @param func : function, functor or lambda
      //------------------------------------------------------------------------
      ExOpenFuncWrapper( const Ctx<File> &f,
          std::function<void( XRootDStatus&, StatInfo& )> handleFunction ) :
          f( f ), fun( handleFunction )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        delete response;
        std::unique_ptr<XRootDStatus> delst( status );
        std::unique_ptr<StatInfo> delrsp;
        StatInfo *info = nullptr;
        if( status->IsOK() )
        {
          XRootDStatus st = f->Stat( false, info );
          delrsp.reset( info );
        }
        else
          info = &NullRef<StatInfo>::value;
        fun( *status, *info );
      }

    private:
      Ctx<File> f;
      //------------------------------------------------------------------------
      //! user defined function, functor or lambda
      //------------------------------------------------------------------------
      std::function<void( XRootDStatus&, StatInfo& )> fun;
  };

  //----------------------------------------------------------------------------
  //! Pipeline exception, wrapps an XRootDStatus
  //----------------------------------------------------------------------------
  class PipelineException : public std::exception
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor from XRootDStatus
      //------------------------------------------------------------------------
      PipelineException( const XRootDStatus &error ) : error( error )
      {

      }

      //------------------------------------------------------------------------
      //! Copy constructor.
      //------------------------------------------------------------------------
      PipelineException( const PipelineException &ex ) : error( ex.error )
      {

      }

      //------------------------------------------------------------------------
      //! Assigment operator
      //------------------------------------------------------------------------
      PipelineException& operator=( const PipelineException &ex )
      {
        error = ex.error;
        return *this;
      }

      //------------------------------------------------------------------------
      //! inherited from std::exception
      //------------------------------------------------------------------------
      const char* what() const noexcept
      {
        return error.ToString().c_str();
      }

      //------------------------------------------------------------------------
      //! @return : the XRootDStatus
      //------------------------------------------------------------------------
      const XRootDStatus& GetError() const
      {
        return error;
      }

    private:

      //------------------------------------------------------------------------
      //! the XRootDStatus associated with this exception
      //------------------------------------------------------------------------
      XRootDStatus error;
  };

  //----------------------------------------------------------------------------
  //! A wrapper handler for a std::promise / std::future.
  //!
  //! @arg Response : response type
  //----------------------------------------------------------------------------
  template<typename Response>
  class FutureWrapperBase : public ResponseHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor, initializes the std::future argument from its
      //! own std::promise
      //!
      //! @param ftr : the future to be linked with this handler
      //------------------------------------------------------------------------
      FutureWrapperBase( std::future<Response> &ftr ) : fulfilled( false )
      {
        ftr = prms.get_future();
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~FutureWrapperBase()
      {
        if( !fulfilled ) SetException( XRootDStatus( stError, errPipelineFailed ) );
      }

    protected:

      //------------------------------------------------------------------------
      //! Set exception in the std::promise / std::future
      //!
      //! @param err : the error
      //------------------------------------------------------------------------
      inline void SetException( const XRootDStatus &err )
      {
        std::exception_ptr ex = std::make_exception_ptr( PipelineException( err ) );
        prms.set_exception( ex );
        fulfilled = true;
      }

      //------------------------------------------------------------------------
      //! promise that corresponds to the future
      //------------------------------------------------------------------------
      std::promise<Response> prms;
      bool                   fulfilled;
  };

  //----------------------------------------------------------------------------
  //! A wrapper handler for a std::promise / std::future.
  //!
  //! @arg Response : response type
  //----------------------------------------------------------------------------
  template<typename Response>
  class FutureWrapper : public FutureWrapperBase<Response>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor, @see FutureWrapperBase
      //!
      //! @param ftr : the future to be linked with this handler
      //------------------------------------------------------------------------
      FutureWrapper( std::future<Response> &ftr ) : FutureWrapperBase<Response>( ftr )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        std::unique_ptr<XRootDStatus> delst( status );
        std::unique_ptr<AnyObject> delrsp( response );
        if( status->IsOK() )
        {
          Response *resp = GetResponse<Response>( response );
          if( resp == &NullRef<Response>::value )
            this->SetException( XRootDStatus( stError, errInternal ) );
          else
          {
            this->prms.set_value( std::move( *resp ) );
            this->fulfilled = true;
          }
        }
        else
          this->SetException( *status );
      }
  };

  //----------------------------------------------------------------------------
  //! A wrapper handler for a std::promise / std::future, overload for void type
  //----------------------------------------------------------------------------
  template<>
  class FutureWrapper<void> : public FutureWrapperBase<void>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor, @see FutureWrapperBase
      //!
      //! @param ftr : the future to be linked with this handler
      //------------------------------------------------------------------------
      FutureWrapper( std::future<void> &ftr ) : FutureWrapperBase<void>( ftr )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        std::unique_ptr<XRootDStatus> delst( status );
        std::unique_ptr<AnyObject> delrsp( response );
        if( status->IsOK() )
        {
          prms.set_value();
          fulfilled = true;
        }
        else
          SetException( *status );
      }
  };


  //----------------------------------------------------------------------------
  //! Wrapper class for raw response handler (ResponseHandler).
  //----------------------------------------------------------------------------
  class RawWrapper : public ResponseHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param handler : the actual operation handler
      //------------------------------------------------------------------------
      RawWrapper( ResponseHandler *handler ) : handler( handler )
      {
      }

      //------------------------------------------------------------------------
      //! Callback method (@see ResponseHandler)
      //!
      //! Note: does not delete itself because it is assumed that it is owned
      //!       by the PipelineHandler (@see PipelineHandler)
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XRootDStatus *status,
                                            AnyObject    *response,
                                            HostList     *hostList )
      {
        handler->HandleResponseWithHosts( status, response, hostList );
      }

    private:
      //------------------------------------------------------------------------
      //! The actual operation handler (we don't own the pointer)
      //------------------------------------------------------------------------
      ResponseHandler *handler;
  };


  //----------------------------------------------------------------------------
  //! A base class for factories, creates ForwardingHandlers from
  //! ResponseHandler*, ResponseHandler& and std::future<Response>
  //!
  //! @arg Response : response type
  //----------------------------------------------------------------------------
  template<typename Response>
  struct RespBase
  {
      //------------------------------------------------------------------------
      //!  A factory method, simply forwards the given handler
      //!
      //! @param h : the ResponseHandler that should be wrapped
      //! @return  : a ForwardingHandler instance
      //------------------------------------------------------------------------
      inline static ResponseHandler* Create( ResponseHandler *hdlr )
      {
        return new RawWrapper( hdlr );
      }

      //------------------------------------------------------------------------
      //!  A factory method, simply forwards the given handler
      //!
      //! @param h : the ResponseHandler that should be wrapped
      //! @return  : a ForwardingHandler instance
      //------------------------------------------------------------------------
      inline static ResponseHandler* Create( ResponseHandler &hdlr )
      {
        return new RawWrapper( &hdlr );
      }

      //------------------------------------------------------------------------
      //! A factory method
      //!
      //! @arg   Response : response type
      //! @param ftr      : the std::future that should be wrapped
      //------------------------------------------------------------------------
      inline static ResponseHandler* Create( std::future<Response> &ftr )
      {
        return new FutureWrapper<Response>( ftr );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory class, creates ForwardingHandler from std::function, in addition
  //! to what RespBase provides (@see RespBase)
  //!
  //! @arg Response : response type
  //----------------------------------------------------------------------------
  template<typename Response>
  struct Resp: RespBase<Response>
  {
      //------------------------------------------------------------------------
      //! A factory method
      //!
      //! @param func : the function/functor/lambda that should be wrapped
      //! @return     : FunctionWrapper instance
      //------------------------------------------------------------------------
      inline static ResponseHandler* Create( std::function<void( XRootDStatus&,
          Response& )> func )
      {
        return new FunctionWrapper<Response>( func );
      }

      //------------------------------------------------------------------------
      //! A factory method
      //!
      //! @param func : the task that should be wrapped
      //! @return     : TaskWrapper instance
      //------------------------------------------------------------------------
      template<typename Return>
      inline static ResponseHandler* Create( std::packaged_task<Return( XRootDStatus&,
          Response& )> &task )
      {
        return new TaskWrapper<Response, Return>( std::move( task ) );
      }

      //------------------------------------------------------------------------
      //! Make the Create overloads from RespBase visible
      //------------------------------------------------------------------------
      using RespBase<Response>::Create;
  };

  //----------------------------------------------------------------------------
  //! Factory class, overloads Resp for void type
  //!
  //! @arg Response : response type
  //----------------------------------------------------------------------------
  template<>
  struct Resp<void>: RespBase<void>
  {
      //------------------------------------------------------------------------
      //! A factory method
      //!
      //! @param func : the function/functor/lambda that should be wrapped
      //! @return     : SimpleFunctionWrapper instance
      //------------------------------------------------------------------------
      inline static ResponseHandler* Create( std::function<void( XRootDStatus& )> func )
      {
        return new FunctionWrapper<void>( func );
      }

      //------------------------------------------------------------------------
      //! A factory method
      //!
      //! @param func : the task that should be wrapped
      //! @return     : TaskWrapper instance
      //------------------------------------------------------------------------
      template<typename Return>
      inline static ResponseHandler* Create( std::packaged_task<Return( XRootDStatus& )> &task )
      {
        return new TaskWrapper<void, Return>( std::move( task ) );
      }

      //------------------------------------------------------------------------
      //! Make the Create overloads from RespBase visible
      //------------------------------------------------------------------------
      using RespBase<void>::Create;
  };
}

#endif // __XRD_CL_OPERATIONS_HANDLERS_HH__
