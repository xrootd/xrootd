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
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClOperationArgs.hh"
#include "XrdSys/XrdSysPthread.hh"

namespace XrdCl
{

  enum State
  {
    Bare, Configured, Handled
  };

  template<State state> class Operation;

  class Pipeline;

  //----------------------------------------------------------------------------
  //! Handler allowing forwarding parameters to the next operation in pipeline
  //----------------------------------------------------------------------------
  class ForwardingHandler: public ResponseHandler
  {
      friend class OperationHandler;

    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //------------------------------------------------------------------------
      ForwardingHandler() :
          container( new ArgsContainer() )
      {
      }

      //------------------------------------------------------------------------
      //! Callback function.
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XRootDStatus *status,
          AnyObject *response, HostList *hostList )
      {
        delete hostList;
        HandleResponse( status, response );
      }

      //------------------------------------------------------------------------
      //! Callback function.
      //------------------------------------------------------------------------
      virtual void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        delete status;
        delete response;
        delete this;
      }

      //------------------------------------------------------------------------
      //! Forward an argument to next operation in pipeline
      //!
      //! @arg    T       :  type of the value which will be saved
      //!
      //! @param  value   :  value to save
      //! @param  bucket  :  bucket in which value will be saved
      //------------------------------------------------------------------------
      template<typename T>
      void FwdArg( typename T::type value, int bucket = 1 )
      {
        container->SetArg<T>( value, bucket );
      }

    private:

      //------------------------------------------------------------------------
      //! @return : container with arguments for forwarding
      //------------------------------------------------------------------------
      std::shared_ptr<ArgsContainer>& GetArgContainer()
      {
        return container;
      }

      //------------------------------------------------------------------------
      //! container with arguments for forwarding
      //------------------------------------------------------------------------
      std::shared_ptr<ArgsContainer> container;

    protected:

      //------------------------------------------------------------------------
      //! @return  :  operation context
      //------------------------------------------------------------------------
      std::unique_ptr<OperationContext> GetOperationContext()
      {
        return std::unique_ptr<OperationContext>(
            new OperationContext( container ) );
      }
  };

  //----------------------------------------------------------------------------
  //! Handler allowing wrapping a normal ResponseHandler into
  //! a ForwaridngHandler.
  //----------------------------------------------------------------------------
  class WrappingHandler: public ForwardingHandler
  {
      friend class OperationHandler;

    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param handler : the handler to be wrapped up
      //------------------------------------------------------------------------
      WrappingHandler( ResponseHandler *handler ) :
          handler( handler )
      {

      }

      //------------------------------------------------------------------------
      //! Callback function.
      //------------------------------------------------------------------------
      void HandleResponseWithHosts( XRootDStatus *status, AnyObject *response,
          HostList *hostList )
      {
        handler->HandleResponseWithHosts( status, response, hostList );
        delete this;
      }

      //------------------------------------------------------------------------
      //! Callback function.
      //------------------------------------------------------------------------
      void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        handler->HandleResponse( status, response );
        delete this;
      }

    private:

      //------------------------------------------------------------------------
      //! The wrapped handler
      //------------------------------------------------------------------------
      ResponseHandler *handler;
  };

  //----------------------------------------------------------------------------
  //! An operations workflow
  //----------------------------------------------------------------------------
  class Workflow
  {
      friend class OperationHandler;

    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param op : first operation of the sequence
      //! @param enableLogging : true if logging should be enabled,
      //!                        false otherwise
      //------------------------------------------------------------------------
      explicit Workflow( Operation<Handled>& op, bool enableLogging = true );

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param op : first operation of the sequence
      //! @param enableLogging : true if logging should be enabled,
      //!                        false otherwise
      //------------------------------------------------------------------------
      explicit Workflow( Operation<Handled> && op, bool enableLogging = true );

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param op : first operation of the sequence
      //! @param enableLogging : true if logging should be enabled,
      //!                        false otherwise
      //------------------------------------------------------------------------
      explicit Workflow( Operation<Handled>* op, bool enableLogging = true );

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param op : first operation of the sequence
      //! @param enableLogging : true if logging should be enabled,
      //!                        false otherwise
      //------------------------------------------------------------------------
      explicit Workflow( Operation<Configured>& op, bool enableLogging = true );

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param op : first operation of the sequence
      //! @param enableLogging : true if logging should be enabled,
      //!                        false otherwise
      //------------------------------------------------------------------------
      explicit Workflow( Operation<Configured> && op,
          bool enableLogging = true );

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param op : first operation of the sequence
      //! @param enableLogging : true if logging should be enabled,
      //!                        false otherwise
      //------------------------------------------------------------------------
      explicit Workflow( Operation<Configured>* op, bool enableLogging = true );

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param pipeline : a pipeline object
      //! @param enableLogging : true if logging should be enabled,
      //!                        false otherwise
      //------------------------------------------------------------------------
      Workflow( Pipeline &&pipeline, bool enableLogging = true );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~Workflow();

      //------------------------------------------------------------------------
      //! Start the pipeline
      //!
      //! @return : status of the operation
      //!
      //! @throws logic_error if the workflow is already running
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> params = nullptr,
          int bucket = 1 );

      //------------------------------------------------------------------------
      //! Wait for the pipeline execution to end
      //------------------------------------------------------------------------
      void Wait();

      //------------------------------------------------------------------------
      //! Get workflow execution status
      //!
      //! @return : workflow execution status
      //------------------------------------------------------------------------
      XRootDStatus GetStatus();

       //-----------------------------------------------------------------------
      //! Get workflow description
      //!
      //! @return : description of the workflow
       //-----------------------------------------------------------------------
      std::string ToString();

       //-----------------------------------------------------------------------
      //! Add operation description to the descriptions list
      //!
      //! @param description : description of assigned operation
       //-----------------------------------------------------------------------
      void AddOperationInfo( std::string description );

       //-----------------------------------------------------------------------
      //! Log operation descriptions
       //-----------------------------------------------------------------------
      void Print();

    private:
      //------------------------------------------------------------------------
      //! Release the semaphore and save status
      //!
      //! @param lastOperationStatus : status of last executed operation,
      //!                              it is set as status of workflow execution
      //------------------------------------------------------------------------
      void EndWorkflowExecution( const XRootDStatus &lastOperationStatus );

      //------------------------------------------------------------------------
      //! first operation in the pipeline
      //------------------------------------------------------------------------
      Operation<Handled> *firstOperation;

      //------------------------------------------------------------------------
      //! Synchronization semaphore, will be released once the pipeline
      //! execution came to an end
      //------------------------------------------------------------------------
      std::unique_ptr<XrdSysSemaphore> semaphore;

      //------------------------------------------------------------------------
      //! Status of the workflow
      //------------------------------------------------------------------------
      XRootDStatus *status;

      //------------------------------------------------------------------------
      //! Description of the workflow
      //------------------------------------------------------------------------
      std::list<std::string> operationDescriptions;

      //------------------------------------------------------------------------
      //! true if logging is enabled, false otherwise
      //------------------------------------------------------------------------
      bool logging;
  };

  //----------------------------------------------------------------------------
  //! Wrapper for ForwardingHandler, used only internally to run next operation
  //! after previous one is finished
  //----------------------------------------------------------------------------
  class OperationHandler: public ResponseHandler
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param handler : the forwarding handler of our operation
      //! @param own     : if true we have the ownership of handler (it's
      //!                  memory), and it is our responsibility to deallocate it
      //------------------------------------------------------------------------
      OperationHandler( ForwardingHandler *handler, bool own );

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
      ~OperationHandler();

      //------------------------------------------------------------------------
      //! Add new operation to the pipeline
      //!
      //! @param operation  :  operation to add
      //------------------------------------------------------------------------
      void AddOperation( Operation<Handled> *operation );

      //------------------------------------------------------------------------
      //! Set workflow to this and all next handlers. In the last handler
      //! it is used to finish workflow execution
      //!
      //! @param  wf           :  workflow to set
      //! @throws logic_error  :  if a workflow has been already assigned
      //------------------------------------------------------------------------
      void AssignToWorkflow( Workflow *wf );

    protected:
      //------------------------------------------------------------------------
      //! Run next operation if it is available, otherwise end workflow
      //------------------------------------------------------------------------
      XRootDStatus RunNextOperation();

    private:

      //------------------------------------------------------------------------
      //! Callback function implementation;
      //------------------------------------------------------------------------
      void HandleResponseImpl( XRootDStatus *status, AnyObject *response,
          HostList *hostList = nullptr );

      //------------------------------------------------------------------------
      //! The forwarding handler of our operation
      //------------------------------------------------------------------------
      ForwardingHandler *responseHandler;

      //------------------------------------------------------------------------
      //! true, if we own the handler
      //------------------------------------------------------------------------
      bool ownHandler;

      //------------------------------------------------------------------------
      //! Next operation in the pipeline
      //------------------------------------------------------------------------
      std::unique_ptr<Operation<Handled>> nextOperation;

      //------------------------------------------------------------------------
      //! Workflow object.
      //------------------------------------------------------------------------
      Workflow *workflow;

      //------------------------------------------------------------------------
      //! Arguments for forwarding
      //------------------------------------------------------------------------
      std::shared_ptr<ArgsContainer> params;
  };

  //----------------------------------------------------------------------------
  //! Operation template. An Operation is a once-use-only object - once executed
  //! by a Workflow engine it is invalidated. Also if used as an argument for
  //! >> or | the original object gets invalidated.
  //!
  //! @arg state :  describes current operation state:
  //!                 - Bare       : a bare operation
  //!                 - Configured : operation with its arguments
  //!                 - Handled    : operation with its arguments and handler
  //----------------------------------------------------------------------------
  template<State state>
  class Operation
  {
      // Declare friendship between templates
      template<State> friend class Operation;

      friend class Workflow;
      friend class OperationHandler;

    public:

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Operation()
      {

      }

      //------------------------------------------------------------------------
      //! Move constructor between template instances.
      //------------------------------------------------------------------------
      template<State from>
      Operation( Operation<from> && op ) :
          handler( std::move( op.handler ) )
      {

      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~Operation()
      {
      }

      //------------------------------------------------------------------------
      //! Move current object into newly allocated instance
      //!
      //! @return : the new instance
      //------------------------------------------------------------------------
      virtual Operation<state>* Move() = 0;

       //------------------------------------------------------------------------
      //! Move current object into newly allocated instance, and convert
      //! it into 'Handled' state.
      //!
      //! @return : the new instance
      //------------------------------------------------------------------------
      virtual Operation<Handled>* ToHandled() = 0;

      //------------------------------------------------------------------------
      //! Name of the operation.
      //------------------------------------------------------------------------
      virtual std::string ToString() = 0;

    protected:

      //------------------------------------------------------------------------
      //! Assign workflow to OperationHandler
      //!
      //! @param wf : the workflow
      //------------------------------------------------------------------------
      void AssignToWorkflow( Workflow *wf )
      {
        static_assert(state == Handled, "Only Operation<Handled> can be assigned to workflow");
        wf->AddOperationInfo( ToString() );
        handler->AssignToWorkflow( wf );
      }

      //------------------------------------------------------------------------
      //! Run operation
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      virtual XRootDStatus Run( std::shared_ptr<ArgsContainer> &params,
          int bucket = 1 ) = 0;

      //------------------------------------------------------------------------
      //! Handle error caused by missing parameter
      //!
      //! @param err : error object
      //!
      //! @return    : default operation status (actual status containg
      //!              error information is passed to the handler)
      //------------------------------------------------------------------------
      virtual XRootDStatus HandleError( const std::logic_error& err )
      {
        XRootDStatus *st = new XRootDStatus( stError, err.what() );
        handler->HandleResponseWithHosts( st, nullptr, nullptr );
        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Add next operation in the pipeline
      //!
      //! @param op : operation to add
      //------------------------------------------------------------------------
      void AddOperation( Operation<Handled> *op )
      {
        if( handler )
        {
          handler->AddOperation( op );
        }
      }

      //------------------------------------------------------------------------
      //! Operation handler
      //------------------------------------------------------------------------
      std::unique_ptr<OperationHandler> handler;
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
      friend class Workflow;

    public:

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Pipeline( Operation<Handled> *op ) :
          operation( op->Move() )
      {

      }

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Pipeline( Operation<Handled> &op ) :
          operation( op.Move() )
      {

      }

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Pipeline( Operation<Handled> &&op ) :
          operation( op.Move() )
      {

      }

      Pipeline( Operation<Configured> *op ) :
          operation( op->ToHandled() )
      {

      }

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Pipeline( Operation<Configured> &op ) :
          operation( op.ToHandled() )
      {

      }

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Pipeline( Operation<Configured> &&op ) :
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
      //! Conversion to Operation<Handled>
      //------------------------------------------------------------------------
      operator Operation<Handled>&()
      {
        return *operation.get();
      }

    private:

      //------------------------------------------------------------------------
      //! First iteam in the pipeline
      //------------------------------------------------------------------------
      std::unique_ptr<Operation<Handled>> operation;
  };

  //----------------------------------------------------------------------------
  //! Concrete Operation template
  //! Defines | and >> operator as well as operation arguments.
  //!
  //! @arg Derived : the class that derives from this template (CRTP)
  //! @arg state   : describes current operation configuration state
  //! @arg Args    : operation arguments
  //----------------------------------------------------------------------------
  template<template<State> class Derived, State state, typename ... Args>
  class ConcreteOperation: public Operation<state>
  {
      template<template<State> class, State, typename ...> friend class ConcreteOperation;

    public:

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      ConcreteOperation()
      {

      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      ConcreteOperation( ConcreteOperation<Derived, from, Args...> && op ) :
        Operation<state>( std::move( op ) ), args( std::move( op.args ) )
      {

      }

      //------------------------------------------------------------------------
      //! Move current object into newly allocated instance
      //!
      //! @return : the new instance
      //------------------------------------------------------------------------
      Operation<state>* Move()
      {
        Derived<state> *me = static_cast<Derived<state>*>( this );
        return new Derived<state>( std::move( *me ) );
      }

      //------------------------------------------------------------------------
      //! Transform operation to handled
      //!
      //! @return Operation<Handled>&
      //------------------------------------------------------------------------
      Operation<Handled>* ToHandled()
      {
        this->handler.reset( new OperationHandler( new ForwardingHandler(), true ) );
        Derived<state> *me = static_cast<Derived<state>*>( this );
        return new Derived<Handled>( std::move( *me ) );
      }

      //------------------------------------------------------------------------
      //! Transform into a new instance with desired state
      //!
      //! @return : new instance in the desired state
      //------------------------------------------------------------------------
      template<State to>
      Derived<to> Transform()
      {
        Derived<state> *me = static_cast<Derived<state>*>( this );
        return Derived<to>( std::move( *me ) );
      }

      //------------------------------------------------------------------------
      //! Generic function call operator. Sets the arguments for the
      //! given operation.
      //!
      //! @param args : parameter pack with operation arguments
      //!
      //! @return     : move-copy of myself in 'Configured' state
      //------------------------------------------------------------------------
      Derived<Configured> operator()( Args... args )
      {
        static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
        this->args = std::tuple<Args...>( std::move( args )... );
        return Transform<Configured>();
      }

      //------------------------------------------------------------------------
      //! Adds handler to the operation
      //!
      //! @param h : handler to add
      //------------------------------------------------------------------------
      Derived<Handled> operator>>(ForwardingHandler *h)
      {
        return StreamImpl( h, false );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param h : handler to add
      //------------------------------------------------------------------------
      Derived<Handled> operator>>(ForwardingHandler &h)
      {
        return StreamImpl( &h, false );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param h : handler to add
      //------------------------------------------------------------------------
      Derived<Handled> operator>>(ResponseHandler *h)
      {
        return StreamImpl( new WrappingHandler( h ) );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param h : handler to add
      //------------------------------------------------------------------------
      Derived<Handled> operator>>(ResponseHandler &h)
      {
        return StreamImpl( new WrappingHandler( &h ) );
      }

      //------------------------------------------------------------------------
      //! Creates a pipeline of 2 or more operations
      //!
      //! @param op  : operation to add
      //!
      //! @return    : handled operation
      //------------------------------------------------------------------------
      Derived<Handled> operator|( Operation<Handled> &op )
      {
        static_assert(state != Bare, "Operator | is available only for Operations that have been at least configured.");
        return PipeImpl( *this, op );
      }

      //------------------------------------------------------------------------
      //! Creates a pipeline of 2 or more operations
      //!
      //! @param op :  operation to add
      //!
      //! @return   :  handled operation
      //------------------------------------------------------------------------
      Derived<Handled> operator|( Operation<Handled> &&op )
      {
        static_assert(state != Bare, "Operator | is available only for Operations that have been at least configured.");
        return PipeImpl( *this, op );
      }

      //------------------------------------------------------------------------
      //! Creates a pipeline of 2 or more operations
      //!
      //! @param op   operation to add
      //!
      //! @return     handled operation
      //------------------------------------------------------------------------
      Derived<Handled> operator|( Operation<Configured> &op )
      {
        static_assert(state != Bare, "Operator | is available only for Operations that have been at least configured.");
        return PipeImpl( *this, op );
      }

      //------------------------------------------------------------------------
      //! Creates a pipeline of 2 or more operations
      //!
      //! @param op  : operation to add
      //!
      //! @return    : handled operation
      //------------------------------------------------------------------------
      Derived<Handled> operator|( Operation<Configured> &&op )
      {
        static_assert(state != Bare, "Operator | is available only for Operations that have been at least configured.");
        return PipeImpl( *this, op );
      }

  protected:

      //------------------------------------------------------------------------
      //! Implements operator>> functionality
      //!
      //! @param h  :  handler to be added
      //! @
      //! @return   :  return an instance of Derived<Handled>
      //------------------------------------------------------------------------
      inline Derived<Handled> StreamImpl( ForwardingHandler *handler, bool own = true )
      {
        static_assert(state == Configured, "Operator >> is available only for type Operation<Configured>");
        this->handler.reset( new OperationHandler( handler, own ) );
        return Transform<Handled>();
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
      Derived<Handled> PipeImpl( ConcreteOperation<Derived, Handled, Args...> &me, Operation<Handled> &op )
      {
        me.AddOperation( op.Move() );
        return me.template Transform<Handled>();
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
      Derived<Handled> PipeImpl( ConcreteOperation<Derived, Handled, Args...> &me, Operation<Configured> &op )
      {
        me.AddOperation( op.ToHandled() );
        return me.template Transform<Handled>();
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
      Derived<Handled> PipeImpl( ConcreteOperation<Derived, Configured, Args...> &me, Operation<Handled> &op )
      {
        me.handler.reset( new OperationHandler( new ForwardingHandler(), true ) );
        me.AddOperation( op.Move() );
        return me.template Transform<Handled>();
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
      Derived<Handled> PipeImpl( ConcreteOperation<Derived, Configured, Args...> &me, Operation<Configured> &op )
      {
        me.handler.reset( new OperationHandler( new ForwardingHandler(), true ) );
        me.AddOperation( op.ToHandled() );
        return me.template Transform<Handled>();
      }

      //------------------------------------------------------------------------
      //! Operation arguments
      //------------------------------------------------------------------------
      std::tuple<Args...> args;
    };

  //----------------------------------------------------------------------------
  //! Helper function for extracting arguments from a argument tuple and
  //! argument container. Priority goes to arguments specified in the
  //! tuple, only if not defined there an arguments is being looked up
  //! in the argument container.
  //!
  //! @arg   ArgDesc : descryptor of the argument type
  //! @arg   Args    : types of operation arguments
  //!
  //! @param args    : tuple with operation arguments
  //! @param params  : container with forwarded arguments
  //! @param bucket  : bucket number in the container
  //!
  //! @return        : requested argument
  //!
  //! @throws        : logic_error if the argument has not been specified
  //!                 neither in the tuple nor in the container
  //----------------------------------------------------------------------------
  template<typename ArgDesc, typename ... Args>
  inline typename ArgDesc::type& Get( std::tuple<Args...> &args,
      std::shared_ptr<ArgsContainer> &params, int bucket )
  {
    auto &arg = std::get<ArgDesc::index>( args );
    return arg.IsEmpty() ? params->GetArg<ArgDesc>( bucket ) : arg.GetValue();
  }
}

#endif // __XRD_CL_OPERATIONS_HH__
