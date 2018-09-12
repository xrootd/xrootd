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

#include <stdexcept>
#include <string>
#include "XrdCl/XrdClOperations.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  // OperationHandler Constructor.
  //----------------------------------------------------------------------------
  OperationHandler::OperationHandler( ForwardingHandler *handler, bool own ) :
      responseHandler( handler ), ownHandler( own ), workflow( nullptr )
  {
    params = handler->GetArgContainer();
  }

  //----------------------------------------------------------------------------
  // OperationHandler::AddOperation
  //----------------------------------------------------------------------------
  void OperationHandler::AddOperation( Operation<Handled> *operation )
  {
    if( nextOperation )
    {
      nextOperation->AddOperation( operation );
    }
    else
    {
      nextOperation.reset( operation );
    }
  }

  //----------------------------------------------------------------------------
  // OperationHandler::HandleResponseImpl
  //----------------------------------------------------------------------------
  void OperationHandler::HandleResponseImpl( XRootDStatus *status,
      AnyObject *response, HostList *hostList )
  {
    // We need to copy status as original status object is destroyed in HandleResponse function
    auto statusCopy = XRootDStatus { *status };
    responseHandler->HandleResponseWithHosts( status, response, hostList );
    ownHandler = false;
    if( !statusCopy.IsOK() || !nextOperation )
    {
      workflow->EndWorkflowExecution( statusCopy ); // TODO check policy
      return;
    }
    if( !( statusCopy = RunNextOperation() ).IsOK() )
      workflow->EndWorkflowExecution( statusCopy );
  }

  //----------------------------------------------------------------------------
  // OperationHandler::HandleResponseWithHosts
  //----------------------------------------------------------------------------
  void OperationHandler::HandleResponseWithHosts( XRootDStatus *status,
      AnyObject *response, HostList *hostList )
  {
    HandleResponseImpl( status, response, hostList );
  }

  //----------------------------------------------------------------------------
  // OperationHandler::HandleResponse
  //----------------------------------------------------------------------------
  void OperationHandler::HandleResponse( XRootDStatus *status,
      AnyObject *response )
  {
    HandleResponseImpl( status, response );
  }

  //----------------------------------------------------------------------------
  // OperationHandler::RunNextOperation
  //----------------------------------------------------------------------------
  XRootDStatus OperationHandler::RunNextOperation()
  {
    return nextOperation->Run( params );
  }

  //----------------------------------------------------------------------------
  // OperationHandler Destructor
  //----------------------------------------------------------------------------
  OperationHandler::~OperationHandler()
  {
    if( ownHandler ) delete responseHandler;
  }

  //----------------------------------------------------------------------------
  // OperationHandler::AssignToWorkflow
  //----------------------------------------------------------------------------
  void OperationHandler::AssignToWorkflow( Workflow *wf )
  {
    if( workflow )
    {
      throw std::logic_error( "Workflow assignment has already been made" );
    }
    workflow = wf;
    if( nextOperation )
    {
      nextOperation->AssignToWorkflow( wf );
    }
  }


  //----------------------------------------------------------------------------
  // Workflow Constructor.
  //----------------------------------------------------------------------------
  Workflow::Workflow( Operation<Handled> &op, bool enableLogging ) :
      firstOperation( op.Move() ), status( nullptr ), logging( enableLogging )
  {
    firstOperation->AssignToWorkflow( this );
  }

  //----------------------------------------------------------------------------
  // Workflow Constructor.
  //----------------------------------------------------------------------------
  Workflow::Workflow( Operation<Handled> &&op, bool enableLogging ) :
      firstOperation( op.Move() ), status( nullptr ), logging( enableLogging )
  {
    firstOperation->AssignToWorkflow( this );
  }

  //----------------------------------------------------------------------------
  // Workflow Constructor.
  //----------------------------------------------------------------------------
  Workflow::Workflow( Operation<Handled> *op, bool enableLogging ) :
      firstOperation( op->Move() ), status( nullptr ), logging( enableLogging )
  {
    firstOperation->AssignToWorkflow( this );
  }

  //----------------------------------------------------------------------------
  // Workflow Constructor.
  //----------------------------------------------------------------------------
  Workflow::Workflow( Operation<Configured> &op, bool enableLogging ) :
      status( nullptr ), logging( enableLogging )
  {
    firstOperation = op.ToHandled();
    firstOperation->AssignToWorkflow( this );
  }

  //----------------------------------------------------------------------------
  // Workflow Constructor.
  //----------------------------------------------------------------------------
  Workflow::Workflow( Operation<Configured> &&op, bool enableLogging ) :
      status( nullptr ), logging( enableLogging )
  {
    firstOperation = op.ToHandled();
    firstOperation->AssignToWorkflow( this );
  }

  //----------------------------------------------------------------------------
  // Workflow Constructor.
  //----------------------------------------------------------------------------
  Workflow::Workflow( Operation<Configured> *op, bool enableLogging ) :
      status( nullptr ), logging( enableLogging )
  {
    firstOperation = op->ToHandled();
    firstOperation->AssignToWorkflow( this );
  }

  //----------------------------------------------------------------------------
  // Workflow Constructor.
  //----------------------------------------------------------------------------
  Workflow::Workflow( Pipeline &&pipeline, bool enableLogging ) :
      status( nullptr ), logging( enableLogging )
  {
    if( !pipeline.operation )
      throw std::invalid_argument( "Pipeline already has been executed." );
    firstOperation = pipeline.operation.release();
  }

  //----------------------------------------------------------------------------
  // Workflow Destructor.
  //----------------------------------------------------------------------------
  Workflow::~Workflow()
  {
    delete firstOperation;
    delete status;
  }

  //----------------------------------------------------------------------------
  // Workflow::Run
  //----------------------------------------------------------------------------
  XRootDStatus Workflow::Run( std::shared_ptr<ArgsContainer> params,
      int bucket )
  {
    if( semaphore )
    {
      throw std::logic_error( "Workflow is already running" );
    }
    semaphore = std::unique_ptr<XrdSysSemaphore>( new XrdSysSemaphore( 0 ) );
    if( logging )
    {
      Print();
    }
    if( params ) return firstOperation->Run( params, bucket );

    std::shared_ptr<ArgsContainer> firstOperationParams = std::shared_ptr<
        ArgsContainer>( new ArgsContainer() );
    return firstOperation->Run( firstOperationParams );
  }

  //----------------------------------------------------------------------------
  // Workflow::EndWorkflowExecution
  //----------------------------------------------------------------------------
  void Workflow::EndWorkflowExecution( const XRootDStatus &lastOperationStatus )
  {
    if( semaphore )
    {
      status = new XRootDStatus( lastOperationStatus );
      semaphore->Post();
    }
  }

  //----------------------------------------------------------------------------
  // Workflow::GetStatus
  //----------------------------------------------------------------------------
  XRootDStatus Workflow::GetStatus()
  {
    return status ? *status : XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Workflow::Wait
  //----------------------------------------------------------------------------
  void Workflow::Wait()
  {
    if( semaphore )
    {
      semaphore->Wait();
    }
  }

  //----------------------------------------------------------------------------
  // Workflow::AddOperationInfo
  //----------------------------------------------------------------------------
  void Workflow::AddOperationInfo( std::string description )
  {
    operationDescriptions.push_back( description );
  }

  //----------------------------------------------------------------------------
  // Workflow::ToString
  //----------------------------------------------------------------------------
  std::string Workflow::ToString()
  {
    std::ostringstream oss;
    auto lastButOne = ( --operationDescriptions.end() );
    for( auto it = operationDescriptions.begin();
        it != operationDescriptions.end(); it++ )
    {
      oss << ( *it );
      if( it != lastButOne )
      {
        oss << " --> ";
      }
    }
    return oss.str();
  }

  //----------------------------------------------------------------------------
  // Workflow::Print
  //----------------------------------------------------------------------------
  void Workflow::Print()
  {
    std::ostringstream oss;
    oss << "Running workflow: " << ToString();
    XrdCl::Log* log = DefaultEnv::GetLog();
    log->Info( TaskMgrMsg, oss.str().c_str() );
  }

}
;
