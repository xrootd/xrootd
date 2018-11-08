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

#ifndef __XRD_CL_OPERATIONS_HH__
#define __XRD_CL_OPERATIONS_HH__

#include <memory>
#include <stdexcept>
#include <sstream>
#include <tuple>
#include <future>
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClOperationHandlers.hh"
#include "XrdClArg.hh"
#include "XrdSys/XrdSysPthread.hh"

namespace XrdCl
{

  template<bool HasHndl> class Operation;

  class Pipeline;

  //----------------------------------------------------------------------------
  //! Wrapper for ResponseHandler, used only internally to run next operation
  //! after previous one is finished
  //----------------------------------------------------------------------------
  class PipelineHandler: public ResponseHandler
  {
      template<bool> friend class Operation;

    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param handler : the handler of our operation
      //! @param own     : if true we have the ownership of handler (it's
      //!                  memory), and it is our responsibility to deallocate it
      //------------------------------------------------------------------------
      PipelineHandler( ResponseHandler *handler, bool own );

      //------------------------------------------------------------------------
      //! Default Constructor.
      //------------------------------------------------------------------------
      PipelineHandler();

      //------------------------------------------------------------------------
      //! Callback function.
      //------------------------------------------------------------------------
      void HandleResponseWithHosts( XRootDStatus *status, AnyObject *response,
          HostList *hostList );

      //------------------------------------------------------------------------
      //! Callback function.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response );

      //------------------------------------------------------------------------
      //! Destructor.
      //------------------------------------------------------------------------
      ~PipelineHandler();

      //------------------------------------------------------------------------
      //! Add new operation to the pipeline
      //!
      //! @param operation  :  operation to add
      //------------------------------------------------------------------------
      void AddOperation( Operation<true> *operation );

      //------------------------------------------------------------------------
      //! Set workflow to this and all next handlers. In the last handler
      //! it is used to finish workflow execution
      //!
      //! @param  prms         :  a promis that the pipeline will have a result
      //! @param  final        :  a callable that should be called at the end of
      //!                         pipeline
      //------------------------------------------------------------------------
      void Assign( std::promise<XRootDStatus>               prms,
                   std::function<void(const XRootDStatus&)> final );

    private:

      //------------------------------------------------------------------------
      //! Callback function implementation;
      //------------------------------------------------------------------------
      void HandleResponseImpl( XRootDStatus *status, AnyObject *response,
          HostList *hostList = nullptr );

      inline void dealloc( XRootDStatus *status, AnyObject *response,
          HostList *hostList )
      {
        delete status;
        delete response;
        delete hostList;
      }

      //------------------------------------------------------------------------
      //! The handler of our operation
      //------------------------------------------------------------------------
      ResponseHandler *responseHandler;

      //------------------------------------------------------------------------
      //! true, if we own the handler
      //------------------------------------------------------------------------
      bool ownHandler;

      //------------------------------------------------------------------------
      //! Next operation in the pipeline
      //------------------------------------------------------------------------
      std::unique_ptr<Operation<true>> nextOperation;

      //------------------------------------------------------------------------
      //! The promise that there will be a result (traveling along the pipeline)
      //------------------------------------------------------------------------
      std::promise<XRootDStatus> prms;

      //------------------------------------------------------------------------
      //! The lambda/function/functor that should be called at the end of the
      //! pipeline (traveling along the pipeline)
      //------------------------------------------------------------------------
      std::function<void(const XRootDStatus&)> final;
  };

  //----------------------------------------------------------------------------
  //! Operation template. An Operation is a once-use-only object - once executed
  //! by a Workflow engine it is invalidated. Also if used as an argument for
  //! >> or | the original object gets invalidated.
  //!
  //! @arg HasHndl : true if operation has a handler, false otherwise
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class Operation
  {
      // Declare friendship between templates
      template<bool>
      friend class Operation;

      friend std::future<XRootDStatus> Async( Pipeline );

      friend class Pipeline;
      friend class PipelineHandler;

    public:

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Operation() : valid( true )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor between template instances.
      //------------------------------------------------------------------------
      template<bool from>
      Operation( Operation<from> && op ) :
          handler( std::move( op.handler ) ), valid( true )
      {
        if( !op.valid ) throw std::invalid_argument( "Cannot construct "
            "Operation from an invalid Operation!" );
        op.valid = false;
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~Operation()
      {
      }

      //------------------------------------------------------------------------
      //! Name of the operation.
      //------------------------------------------------------------------------
      virtual std::string ToString() = 0;

      //------------------------------------------------------------------------
      //! Move current object into newly allocated instance
      //!
      //! @return : the new instance
      //------------------------------------------------------------------------
      virtual Operation<HasHndl>* Move() = 0;

       //------------------------------------------------------------------------
      //! Move current object into newly allocated instance, and convert
      //! it into 'handled' operation.
      //!
      //! @return : the new instance
      //------------------------------------------------------------------------
      virtual Operation<true>* ToHandled() = 0;

    protected:

      //------------------------------------------------------------------------
      //! Run operation
      //!
      //! @param prom   : the promise that we will have a result
      //! @param final  : the object to call at the end of pipeline
      //! @param args   : forwarded arguments
      //! @param bucket : number of the bucket with arguments
      //!
      //! @return       : stOK if operation was scheduled for execution
      //!                 successfully, stError otherwise
      //------------------------------------------------------------------------
      void Run( std::promise<XRootDStatus>                prms,
                std::function<void(const XRootDStatus&)>  final )
      {
        static_assert(HasHndl, "Only an operation that has a handler can be assigned to workflow");
        handler->Assign( std::move( prms ), std::move( final ) );
        XRootDStatus st = RunImpl();
        if( st.IsOK() ) handler.release();
        else
          ForceHandler( st );
      }

      //------------------------------------------------------------------------
      //! Run the actual operation
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //! @param bucket : number of the bucket with arguments
      //------------------------------------------------------------------------
      virtual XRootDStatus RunImpl() = 0;

      //------------------------------------------------------------------------
      //! Handle error caused by missing parameter
      //!
      //! @param err : error object
      //!
      //! @return    : default operation status (actual status containg
      //!              error information is passed to the handler)
      //------------------------------------------------------------------------
      void ForceHandler( const XRootDStatus &status )
      {
        handler->HandleResponse( new XRootDStatus( status ), nullptr );
        // HandleResponse already freed the memory so we have to
        // release the unique pointer
        handler.release();
      }

      //------------------------------------------------------------------------
      //! Add next operation in the pipeline
      //!
      //! @param op : operation to add
      //------------------------------------------------------------------------
      void AddOperation( Operation<true> *op )
      {
        if( handler )
        {
          handler->AddOperation( op );
        }
      }

      //------------------------------------------------------------------------
      //! Operation handler
      //------------------------------------------------------------------------
      std::unique_ptr<PipelineHandler> handler;

      //------------------------------------------------------------------------
      //! Flag indicating if it is a valid object
      //------------------------------------------------------------------------
      bool valid;
  };

  //----------------------------------------------------------------------------
  //! A wrapper around operation pipeline. A Pipeline is a once-use-only
  //! object - once executed by a Workflow engine it is invalidated.
  //!
  //! Takes ownership of given operation pipeline (which is in most would
  //! be a temporary object)
  //----------------------------------------------------------------------------
  class Pipeline
  {
      template<bool> friend class ParallelOperation;
      friend std::future<XRootDStatus> Async( Pipeline );

    public:

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Pipeline( Operation<true> *op ) :
          operation( op->Move() )
      {

      }

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Pipeline( Operation<true> &op ) :
          operation( op.Move() )
      {

      }

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Pipeline( Operation<true> &&op ) :
          operation( op.Move() )
      {

      }

      Pipeline( Operation<false> *op ) :
          operation( op->ToHandled() )
      {

      }

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Pipeline( Operation<false> &op ) :
          operation( op.ToHandled() )
      {

      }

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Pipeline( Operation<false> &&op ) :
          operation( op.ToHandled() )
      {

      }

      Pipeline( Pipeline &&pipe ) :
          operation( std::move( pipe.operation ) )
      {

      }

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Pipeline& operator=( Pipeline &&pipe )
      {
        operation = std::move( pipe.operation );
        return *this;
      }

      //------------------------------------------------------------------------
      //! Conversion to Operation<true>
      //!
      //! @throws : std::logic_error if pipeline is invalid
      //------------------------------------------------------------------------
      operator Operation<true>&()
      {
        if( !bool( operation ) ) throw std::logic_error( "Invalid pipeline." );
        return *operation.get();
      }

      //------------------------------------------------------------------------
      //! Conversion to boolean
      //!
      //! @return : true if it's a valid pipeline, false otherwise
      //------------------------------------------------------------------------
      operator bool()
      {
        return bool( operation );
      }

    private:

      //------------------------------------------------------------------------
      //! Member access declaration, provides access to the underlying
      //! operation.
      //!
      //! @return : pointer to the underlying
      //------------------------------------------------------------------------
      Operation<true>* operator->()
      {
        return operation.get();
      }

      //------------------------------------------------------------------------
      //! Schedules the underlying pipeline for execution.
      //!
      //! @param args   : forwarded arguments
      //! @param bucket : number of bucket with forwarded params
      //! @param final  : to be called at the end of the pipeline
      //------------------------------------------------------------------------
      void Run( std::function<void(const XRootDStatus&)> final = nullptr )
      {
        if( ftr.valid() )
          throw std::logic_error( "Pipeline is already running" );

        // a promise that the pipe will have a result
        std::promise<XRootDStatus> prms;
        ftr = prms.get_future();
        operation->Run( std::move( prms ), std::move( final ) );
      }

      //------------------------------------------------------------------------
      //! First operation in the pipeline
      //------------------------------------------------------------------------
      std::unique_ptr<Operation<true>> operation;

      //------------------------------------------------------------------------
      //! The future result of the pipeline
      //------------------------------------------------------------------------
      std::future<XRootDStatus> ftr;

  };

  //----------------------------------------------------------------------------
  //! Helper function, schedules execution of given pipeline
  //!
  //! @param pipeline : the pipeline to be executed
  //!
  //! @return         : future status of the operation
  //----------------------------------------------------------------------------
  inline std::future<XRootDStatus> Async( Pipeline pipeline )
  {
    pipeline.Run();
    return std::move( pipeline.ftr );
  }

  //----------------------------------------------------------------------------
  //! Helper function, schedules execution of given pipeline and waits for
  //! the status
  //!
  //! @param pipeline : the pipeline to be executed
  //!
  //! @return         : status of the operation
  //----------------------------------------------------------------------------
  inline XRootDStatus WaitFor( Pipeline pipeline )
  {
    return Async( std::move( pipeline ) ).get();
  }

  //----------------------------------------------------------------------------
  //! Concrete Operation template
  //! Defines | and >> operator as well as operation arguments.
  //!
  //! @arg Derived : the class that derives from this template (CRTP)
  //! @arg HasHndl : true if operation has a handler, false otherwise
  //! @arg Args    : operation arguments
  //----------------------------------------------------------------------------
  template<template<bool> class Derived, bool HasHndl, typename HdlrFactory, typename ... Args>
  class ConcreteOperation: public Operation<HasHndl>
  {
      template<template<bool> class, bool, typename, typename ...>
      friend class ConcreteOperation;

    public:

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param args : operation arguments
      //------------------------------------------------------------------------
      ConcreteOperation( Args&&... args ) : args( std::tuple<Args...>( std::move( args )... ) )
      {
        static_assert( !HasHndl, "It is only possible to construct operation without handler" );
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<bool from>
      ConcreteOperation( ConcreteOperation<Derived, from, HdlrFactory, Args...> && op ) :
        Operation<HasHndl>( std::move( op ) ), args( std::move( op.args ) )
      {

      }

      //------------------------------------------------------------------------
      //! Adds ResponseHandler/function/functor/lambda/future handler for
      //! the operation.
      //!
      //! Note: due to reference collapsing this covers both l-value and
      //!       r-value references.
      //!
      //! @param func : function/functor/lambda
      //------------------------------------------------------------------------
      template<typename Hdlr>
      Derived<true> operator>>( Hdlr &&hdlr )
      {
        // check if the resulting handler should be owned by us or by the user,
        // if the user passed us directly a ResponseHandler it's owned by the
        // user, otherwise we need to wrap the argument in a handler and in this
        // case the resulting handler will be owned by us
        constexpr bool own = !IsResponseHandler<Hdlr>::value;
        return this->StreamImpl( HdlrFactory::Create( hdlr ), own );
      }

      //------------------------------------------------------------------------
      //! Creates a pipeline of 2 or more operations
      //!
      //! @param op  : operation to add
      //!
      //! @return    : handled operation
      //------------------------------------------------------------------------
      Derived<true> operator|( Operation<true> &op )
      {
        return PipeImpl( *this, op );
      }

      //------------------------------------------------------------------------
      //! Creates a pipeline of 2 or more operations
      //!
      //! @param op :  operation to add
      //!
      //! @return   :  handled operation
      //------------------------------------------------------------------------
      Derived<true> operator|( Operation<true> &&op )
      {
        return PipeImpl( *this, op );
      }

      //------------------------------------------------------------------------
      //! Creates a pipeline of 2 or more operations
      //!
      //! @param op   operation to add
      //!
      //! @return     handled operation
      //------------------------------------------------------------------------
      Derived<true> operator|( Operation<false> &op )
      {
        return PipeImpl( *this, op );
      }

      //------------------------------------------------------------------------
      //! Creates a pipeline of 2 or more operations
      //!
      //! @param op  : operation to add
      //!
      //! @return    : handled operation
      //------------------------------------------------------------------------
      Derived<true> operator|( Operation<false> &&op )
      {
        return PipeImpl( *this, op );
      }

    protected:

      //------------------------------------------------------------------------
      //! Move current object into newly allocated instance
      //!
      //! @return : the new instance
      //------------------------------------------------------------------------
      Operation<HasHndl>* Move()
      {
        Derived<HasHndl> *me = static_cast<Derived<HasHndl>*>( this );
        return new Derived<HasHndl>( std::move( *me ) );
      }

      //------------------------------------------------------------------------
      //! Transform operation to handled
      //!
      //! @return Operation<true>&
      //------------------------------------------------------------------------
      Operation<true>* ToHandled()
      {
        this->handler.reset( new PipelineHandler() );
        Derived<HasHndl> *me = static_cast<Derived<HasHndl>*>( this );
        return new Derived<true>( std::move( *me ) );
      }

      //------------------------------------------------------------------------
      //! Transform into a new instance with desired state
      //!
      //! @return : new instance in the desired state
      //------------------------------------------------------------------------
      template<bool to>
      Derived<to> Transform()
      {
        Derived<HasHndl> *me = static_cast<Derived<HasHndl>*>( this );
        return Derived<to>( std::move( *me ) );
      }

      //------------------------------------------------------------------------
      //! Implements operator>> functionality
      //!
      //! @param h  :  handler to be added
      //! @
      //! @return   :  return an instance of Derived<true>
      //------------------------------------------------------------------------
      inline Derived<true> StreamImpl( ResponseHandler *handler, bool own )
      {
        static_assert( !HasHndl, "Operator >> is available only for operation without handler" );
        this->handler.reset( new PipelineHandler( handler, own ) );
        return Transform<true>();
      }

      //------------------------------------------------------------------------
      //! Implements operator| functionality
      //!
      //! @param me  :  reference to myself (*this)
      //! @param op  :  reference to the other operation
      //!
      //! @return    :  move-copy of myself
      //------------------------------------------------------------------------
      inline static
      Derived<true> PipeImpl( ConcreteOperation<Derived, true, HdlrFactory,
          Args...> &me, Operation<true> &op )
      {
        me.AddOperation( op.Move() );
        return me.template Transform<true>();
      }

      //------------------------------------------------------------------------
      //! Implements operator| functionality
      //!
      //! @param me  :  reference to myself (*this)
      //! @param op  :  reference to the other operation
      //!
      //! @return    :  move-copy of myself
      //------------------------------------------------------------------------
      inline static
      Derived<true> PipeImpl( ConcreteOperation<Derived, true, HdlrFactory,
          Args...> &me, Operation<false> &op )
      {
        me.AddOperation( op.ToHandled() );
        return me.template Transform<true>();
      }

      //------------------------------------------------------------------------
      //! Implements operator| functionality
      //!
      //! @param me  :  reference to myself (*this)
      //! @param op  :  reference to the other operation
      //!
      //! @return    :  move-copy of myself
      //------------------------------------------------------------------------
      inline static
      Derived<true> PipeImpl( ConcreteOperation<Derived, false, HdlrFactory,
          Args...> &me, Operation<true> &op )
      {
        me.handler.reset( new PipelineHandler() );
        me.AddOperation( op.Move() );
        return me.template Transform<true>();
      }

      //------------------------------------------------------------------------
      //! Implements operator| functionality
      //!
      //! @param me  :  reference to myself (*this)
      //! @param op  :  reference to the other operation
      //!
      //! @return    :  move-copy of myself
      //------------------------------------------------------------------------
      inline static
      Derived<true> PipeImpl( ConcreteOperation<Derived, false, HdlrFactory,
          Args...> &me, Operation<false> &op )
      {
        me.handler.reset( new PipelineHandler() );
        me.AddOperation( op.ToHandled() );
        return me.template Transform<true>();
      }

      //------------------------------------------------------------------------
      //! Operation arguments
      //------------------------------------------------------------------------
      std::tuple<Args...> args;
    };
}

#endif // __XRD_CL_OPERATIONS_HH__
