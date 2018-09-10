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

#include <iostream>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <tuple>
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClOperationArgs.hh"

namespace XrdCl
{

  enum State
  {
    Bare, Configured, Handled
  };
  template<State state> class Operation;

  //---------------------------------------------------------------------------
  //! Handler allowing forwarding parameters to the next operation in workflow
  //---------------------------------------------------------------------------
  class ForwardingHandler: public ResponseHandler
  {
      friend class OperationHandler;

    public:
      ForwardingHandler() :
          container( new ArgsContainer() ), responseHandler( NULL ), wrapper(
              false )
      {
      }

      ForwardingHandler( ResponseHandler *handler ) :
          container( new ArgsContainer() ), responseHandler( handler ), wrapper(
              true )
      {
      }

      virtual void HandleResponseWithHosts( XRootDStatus *status,
          AnyObject *response, HostList *hostList )
      {
        if( wrapper )
        {
          responseHandler->HandleResponseWithHosts( status, response,
              hostList );
          delete this;
        } else
        {
          delete hostList;
          HandleResponse( status, response );
        }
      }

      virtual void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        if( wrapper )
        {
          responseHandler->HandleResponse( status, response );
        } else
        {
          delete status;
          delete response;
        }
        delete this;
      }

      //------------------------------------------------------------------
      //! Saves value in param container so that in can be used in
      //! next operation
      //!
      //! @tparam T       type of the value which will be saved
      //! @param value    value to save
      //! @param bucket   bucket in which value will be saved
      //------------------------------------------------------------------
      template<typename T>
      void FwdArg( typename T::type value, int bucket = 1 )
      {
        container->SetArg<T>( value, bucket );
      }

    private:
      std::shared_ptr<ArgsContainer>& GetArgContainer()
      {
        return container;
      }

      std::shared_ptr<ArgsContainer> container;

    protected:
      std::unique_ptr<OperationContext> GetOperationContext()
      {
        return std::unique_ptr<OperationContext>(
            new OperationContext( container ) );
      }

      ResponseHandler* responseHandler;
      bool wrapper;
  };

  //---------------------------------------------------------------------------
  //! File operations workflow
  //---------------------------------------------------------------------------
  class Workflow
  {
      friend class OperationHandler;

    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param op first operation of the sequence
      //------------------------------------------------------------------------
      explicit Workflow( Operation<Handled>& op, bool enableLogging = true );

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param op first operation of the sequence
      //------------------------------------------------------------------------
      explicit Workflow( Operation<Handled> && op, bool enableLogging = true );

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param op first operation of the sequence
      //------------------------------------------------------------------------
      explicit Workflow( Operation<Handled>* op, bool enableLogging = true );

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param op first operation of the sequence
      //------------------------------------------------------------------------
      explicit Workflow( Operation<Configured>& op, bool enableLogging = true );

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param op first operation of the sequence
      //------------------------------------------------------------------------
      explicit Workflow( Operation<Configured> && op,
          bool enableLogging = true );

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param op first operation of the sequence
      //------------------------------------------------------------------------
      explicit Workflow( Operation<Configured>* op, bool enableLogging = true );

      ~Workflow();

      //------------------------------------------------------------------------
      //! Run first workflow operation
      //!
      //! @return original workflow object
      //------------------------------------------------------------------------
      Workflow& Run( std::shared_ptr<ArgsContainer> params = NULL, int bucket =
          1 );

      //------------------------------------------------------------------------
      //! Wait for workflow execution end
      //!
      //! @return original workflow object
      //------------------------------------------------------------------------
      Workflow& Wait();

      //------------------------------------------------------------------------
      //! Get workflow execution status
      //!
      //! @return workflow execution status if available, otherwise default
      //! XRootDStatus object
      //------------------------------------------------------------------------
      XRootDStatus GetStatus();

      //------------------------------------------------------------------
      //! Get workflow description
      //!
      //! @return description of the workflow
      //------------------------------------------------------------------
      std::string ToString();

      //------------------------------------------------------------------
      //! Add operation description to the descriptions list
      //!
      //! @param description description of assigned operation
      //------------------------------------------------------------------
      void AddOperationInfo( std::string description );

      //------------------------------------------------------------------
      //! Log operation descriptions
      //------------------------------------------------------------------
      void Print();

    private:
      //------------------------------------------------------------------------
      //! Release the semaphore and save status
      //!
      //! @param lastOperationStatus status of last executed operation.
      //!                    It is set as status of workflow execution.
      //------------------------------------------------------------------------
      void EndWorkflowExecution( const XRootDStatus &lastOperationStatus );

      Operation<Handled> *firstOperation;
      std::unique_ptr<XrdSysSemaphore> semaphore;
      XRootDStatus *status;
      std::list<std::string> operationDescriptions;
      bool logging;
  };

  //---------------------------------------------------------------------------
  //! Wrapper for ForwardingHandler, used only internally to run next operation
  //! after previous one is finished
  //---------------------------------------------------------------------------
  class OperationHandler: public ResponseHandler
  {
    public:
      OperationHandler( ForwardingHandler *handler, bool own );
      virtual void HandleResponseWithHosts( XRootDStatus *status,
          AnyObject *response, HostList *hostList );
      virtual void HandleResponse( XRootDStatus *status, AnyObject *response );
      virtual ~OperationHandler();

      //------------------------------------------------------------------------
      //! Add new operation to the sequence
      //!
      //! @param operation    operation to add
      //------------------------------------------------------------------------
      void AddOperation( Operation<Handled> *operation );

      //------------------------------------------------------------------------
      //! Set workflow to this and all next handlers. In the last handler
      //! it is used to finish workflow execution
      //!
      //! @param wf   workflow to set
      //------------------------------------------------------------------------
      void AssignToWorkflow( Workflow *wf );

    protected:
      //------------------------------------------------------------------------
      //! Run next operation if it is available, otherwise end workflow
      //------------------------------------------------------------------------
      XRootDStatus RunNextOperation();

    private:

      ForwardingHandler *responseHandler;
      bool ownHandler;
      Operation<Handled> *nextOperation;
      Workflow *workflow;
      std::shared_ptr<ArgsContainer> params;
  };

  //----------------------------------------------------------------------
  //! Operation template
  //!
  //! @tparam state   describes current operation configuration state
  //----------------------------------------------------------------------
  template<State state>
  class Operation
  {
      // Declare friendship between templates
      template<State> friend class Operation;

      friend class Workflow;
      friend class OperationHandler;

    public:
      //------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------
      Operation() :
          handler( nullptr )
      {

      }

      template<State from>
      Operation( Operation<from> && op ) :
          handler( std::move( op.handler ) )
      {

      }

      virtual ~Operation()
      {
      }

      virtual Operation<state>* Move() = 0;

      virtual Operation<Handled>* ToHandled() = 0;

      virtual std::string ToString() = 0;

    protected:

      //------------------------------------------------------------------
      //! Set workflow pointer in the handler
      //!
      //! @param wf   workflow to set
      //------------------------------------------------------------------
      void AssignToWorkflow( Workflow *wf )
      {
        static_assert(state == Handled, "Only Operation<Handled> can be assigned to workflow");
        wf->AddOperationInfo( ToString() );
        handler->AssignToWorkflow( wf );
      }

      //------------------------------------------------------------------
      //! Run operation
      //!
      //! @param params   container with parameters forwarded from
      //!                 previous operation
      //! @return         status of the operation
      //------------------------------------------------------------------
      virtual XRootDStatus Run( std::shared_ptr<ArgsContainer> &params,
          int bucket = 1 ) = 0;

      //------------------------------------------------------------------
      //! Handle error caused by missing parameter
      //!
      //! @param err  error object
      //! @return     default operation status (actual status containg
      //!             error information is passed to the handler)
      //------------------------------------------------------------------
      virtual XRootDStatus HandleError( const std::logic_error& err )
      {
        XRootDStatus *st = new XRootDStatus( stError, err.what() );
        handler->HandleResponse( st, 0 );
        return XRootDStatus();
      }

      //------------------------------------------------------------------
      //! Add next operation to the handler
      //!
      //! @param op  operation to add
      //------------------------------------------------------------------
      void AddOperation( Operation<Handled> *op )
      {
        if( handler )
        {
          handler->AddOperation( op );
        }
      }

      std::unique_ptr<OperationHandler> handler;
  };

  //----------------------------------------------------------------------
  //! ArgsOperation template
  //!
  //! @param Derived  the class that derives from this template
  //! @param state    describes current operation configuration state
  //! @param Args     operation arguments
  //----------------------------------------------------------------------
  template<template<State> class Derived, State state, typename ... Args>
  class OperationBase: public Operation<state>
  {
      template<template<State> class, State, typename ...> friend class OperationBase;

      public:

      OperationBase()
      {}

      template<State from>
      OperationBase( OperationBase<Derived, from, Args...> && op ) : Operation<state>( std::move( op ) ), args( std::move( op.args ) )
      {

      }

      Operation<state>* Move()
      {
        Derived<state> *me = static_cast<Derived<state>*>( this );
        return new Derived<state>( std::move( *me ) );
      }

      //------------------------------------------------------------------
      //! Transform operation to handled
      //!
      //! @return Operation<Handled>&
      //------------------------------------------------------------------
      Operation<Handled>* ToHandled()
      {
        this->handler.reset( new OperationHandler( new ForwardingHandler(), true ) );
        Derived<state> *me = static_cast<Derived<state>*>( this );
        return new Derived<Handled>( std::move( *me ) );
        return 0;
      }

      template<State to>
      Derived<to> Transform()
      {
        Derived<state> *me = static_cast<Derived<state>*>( this );
        return Derived<to>( std::move( *me ) );
      }

      Derived<Configured> operator()( Args... args )
      {
        static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
        this->TakeArgs( std::move( args )... );
        return Transform<Configured>();
      }

      //------------------------------------------------------------------
      //! Add handler which will be executed after operation ends
      //!
      //! @param h  handler to add
      //------------------------------------------------------------------
      Derived<Handled> operator>>(ForwardingHandler *h)
      {
        return StreamImpl( h, false );
      }

      //------------------------------------------------------------------
      //! Add handler which will be executed after operation ends
      //!
      //! @param h  handler to add
      //------------------------------------------------------------------
      Derived<Handled> operator>>(ResponseHandler *h)
      {
        return StreamImpl( new ForwardingHandler( h ) );
      }

      //------------------------------------------------------------------
      //! Add operation to handler
      //!
      //! @param op   operation to add
      //! @return     handled operation
      //------------------------------------------------------------------
      Derived<Handled> operator|( Operation<Handled> &op )
      {
        static_assert(state != Bare, "Operator | is available only for Operations that have been at least configured.");
        return PipeImpl( *this, op );
      }

      Derived<Handled> operator|( Operation<Handled> &&op )
      {
        static_assert(state != Bare, "Operator | is available only for Operations that have been at least configured.");
        return PipeImpl( *this, op );
      }

      Derived<Handled> operator|( Operation<Configured> &op )
      {
        static_assert(state != Bare, "Operator | is available only for Operations that have been at least configured.");
        return PipeImpl( *this, op );
      }

      Derived<Handled> operator|( Operation<Configured> &&op )
      {
        static_assert(state != Bare, "Operator | is available only for Operations that have been at least configured.");
        return PipeImpl( *this, op );
      }

      protected:

      inline void TakeArgs( Args&&... args )
      {
        this->args = std::tuple<Args...>( std::move( args )... );
      }

      //------------------------------------------------------------------
      //! implements operator>> functionality
      //!
      //! @param h    handler to be added
      //! @return     TODO
      //------------------------------------------------------------------
      inline Derived<Handled> StreamImpl( ForwardingHandler *handler, bool own = true )
      {
        static_assert(state == Configured, "Operator >> is available only for type Operation<Configured>");
        this->handler.reset( new OperationHandler( handler, own ) );
        return Transform<Handled>();
      }

      inline static
      Derived<Handled> PipeImpl( OperationBase<Derived, Handled, Args...> &me, Operation<Handled> &op )
      {
        me.AddOperation( op.Move() );
        return me.Transform<Handled>();
      }

      inline static
      Derived<Handled> PipeImpl( OperationBase<Derived, Handled, Args...> &me, Operation<Configured> &op )
      {
        me.AddOperation( op.ToHandled() );
        return me.Transform<Handled>();
      }

      inline static
      Derived<Handled> PipeImpl( OperationBase<Derived, Configured, Args...> &me, Operation<Handled> &op )
      {
        me.handler.reset( new OperationHandler( new ForwardingHandler(), true ) );
        me.AddOperation( op.Move() );
        return me.Transform<Handled>();
      }

      inline static
      Derived<Handled> PipeImpl( OperationBase<Derived, Configured, Args...> &me, Operation<Configured> &op )
      {
        me.handler.reset( new OperationHandler( new ForwardingHandler(), true ) );
        me.AddOperation( op.ToHandled() );
        return me.Transform<Handled>();
      }

      std::tuple<Args...> args;
    };

    //----------------------------------------------------------------------
    //! Get helper function for ArgsOperation template
    //!
    //! @param args     tuple with operation arguments
    //! @param params   params forwarded by previous operation
    //! @param bucket   bucket assigned to this operation
    //----------------------------------------------------------------------
  template<typename ArgDesc, typename ... Args>
  inline typename ArgDesc::type& Get( std::tuple<Args...> &args,
      std::shared_ptr<ArgsContainer> &params, int bucket )
  {
    auto &arg = std::get<ArgDesc::index>( args );
    return arg.IsEmpty() ? params->GetArg<ArgDesc>( bucket ) : arg.GetValue();
  }

  //-----------------------------------------------------------------------
  //! Parallel operations
  //!
  //! @tparam state   describes current operation configuration state
  //-----------------------------------------------------------------------
  template<State state = Bare>
  class ParallelOperation: public OperationBase<ParallelOperation, state>
  {
      template<State> friend class ParallelOperation;

    public:
      //------------------------------------------------------------------
      //! Constructor
      //!
      //! @param operations   list of operations to run in parallel
      //------------------------------------------------------------------
      ParallelOperation( std::initializer_list<Operation<Handled>*> operations )
      {
        static_assert(state == Configured, "Constructor is available only for type ParallelOperations<Configured>");
        std::initializer_list<Operation<Handled>*>::iterator it =
            operations.begin();
        while( it != operations.end() )
        {
          std::unique_ptr<Workflow> w( new Workflow( *it, false ) );
          workflows.push_back( std::move( w ) );
          ++it;
        }
      }

      template<State from>
      ParallelOperation( ParallelOperation<from> &&obj ) :
          OperationBase<ParallelOperation, state>( std::move( obj ) ), workflows(
              std::move( obj.workflows ) )
      {
      }

      template<typename Container>
      ParallelOperation( Container &container )
      {
        static_assert(state == Configured, "Constructor is available only for type ParallelOperations<Configured>");
        static_assert(std::is_same<typename Container::value_type, Operation<Handled>*>::value, "Invalid type in container");
        typename Container::iterator it = container.begin();
        while( it != container.end() )
        {
          std::unique_ptr<Workflow> w( new Workflow( *it, false ) );
          workflows.push_back( std::move( w ) );
          ++it;
        }
      }

      //------------------------------------------------------------------
      //! Get description of parallel operations flow
      //!
      //! @return std::string description
      //------------------------------------------------------------------
      std::string ToString()
      {
        std::ostringstream oss;
        oss << "Parallel(";
        for( int i = 0; i < workflows.size(); i++ )
        {
          oss << workflows[i]->ToString();
          if( i != workflows.size() - 1 )
          {
            oss << " && ";
          }
        }
        oss << ")";
        return oss.str();
      }

    private:
      //------------------------------------------------------------------------
      //! Run operations
      //!
      //! @param params           parameters container
      //! @param bucketDefault    bucket in parameters container
      //!                         (not used here, provided only for compatibility with the interface )
      //! @return XRootDStatus    status of the operations
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> &params,
          int bucketDefault = 0 )
      {
        for( int i = 0; i < workflows.size(); i++ )
        {
          int bucket = i + 1;
          workflows[i]->Run( params, bucket );
        }

        bool statusOK = true;
        std::string statusMessage = "";

        for( int i = 0; i < workflows.size(); i++ )
        {
          workflows[i]->Wait();
          auto result = workflows[i]->GetStatus();
          if( !result.IsOK() )
          {
            statusOK = false;
            statusMessage = result.ToStr();
            break;
          }
        }
        const uint16_t status = statusOK ? stOK : stError;

        XRootDStatus *st = new XRootDStatus( status, statusMessage );
        this->handler->HandleResponseWithHosts( st, NULL, NULL );

        return XRootDStatus();
      }

      std::vector<std::unique_ptr<Workflow>> workflows;
  };
  typedef ParallelOperation<Configured> Parallel;
}

#endif // __XRD_CL_OPERATIONS_HH__
