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

namespace
{
  //----------------------------------------------------------------------------
  //! An exception type used to (force) stop a pipeline
  //----------------------------------------------------------------------------
  struct StopPipeline
  {
    StopPipeline( const XrdCl::XRootDStatus &status ) : status( status ) { }
    XrdCl::XRootDStatus status;
  };

  //----------------------------------------------------------------------------
  //! An exception type used to repeat an operation
  //----------------------------------------------------------------------------
  struct RepeatOpeation { };

  //----------------------------------------------------------------------------
  //! An exception type used to replace an operation
  //----------------------------------------------------------------------------
  struct ReplaceOperation
  {
    ReplaceOperation( XrdCl::Operation<false> &&opr ) : opr( opr.ToHandled() )
    {
    }

    std::unique_ptr<XrdCl::Operation<true>> opr;
  };

  //----------------------------------------------------------------------------
  //! An exception type used to replace whole pipeline
  //----------------------------------------------------------------------------
  struct ReplacePipeline
  {
    ReplacePipeline( XrdCl::Pipeline p ) : pipeline( std::move( p ) )
    {
    }

    XrdCl::Pipeline pipeline;
  };

  //----------------------------------------------------------------------------
  //! An exception type used to ignore a failed operation
  //----------------------------------------------------------------------------
  struct IgnoreError { };
}

namespace XrdCl
{

  //----------------------------------------------------------------------------
  // OperationHandler Constructor.
  //----------------------------------------------------------------------------
  PipelineHandler::PipelineHandler( ResponseHandler  *handler ) :
      responseHandler( handler )
  {
  }

  //----------------------------------------------------------------------------
  // OperationHandler::AddOperation
  //----------------------------------------------------------------------------
  void PipelineHandler::AddOperation( Operation<true> *operation )
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
  void PipelineHandler::HandleResponseImpl( XRootDStatus *status,
      AnyObject *response, HostList *hostList )
  {
    std::unique_ptr<PipelineHandler> myself( this );

    // We need to copy status as original status object is destroyed in
    // HandleResponse function
    XRootDStatus st( *status );
    if( responseHandler )
    {
      try
      {
        responseHandler->HandleResponseWithHosts( status, response, hostList );
      }
      catch( const StopPipeline &stop )
      {
        if( final ) final( stop.status );
        prms.set_value( stop.status );
        return;
      }
      catch( const RepeatOpeation &repeat )
      {
        Operation<true> *opr = currentOperation.release();
        opr->handler.reset( myself.release() );
        opr->Run( timeout, std::move( prms ), std::move( final ) );
        return;
      }
      catch( ReplaceOperation &replace )
      {
        Operation<true> *opr = replace.opr.release();
        opr->handler.reset( myself.release() );
        opr->Run( timeout, std::move( prms ), std::move( final ) );
        return;
      }
      catch( ReplacePipeline &replace )
      {
        Pipeline p = std::move( replace.pipeline );
        Operation<true> *opr = p.operation.release();
        opr->Run( timeout, std::move( prms ), std::move( final ) );
        return;
      }
      catch( const IgnoreError &ignore )
      {
        st = XRootDStatus();
      }
    }
    else
      dealloc( status, response, hostList );

    if( !st.IsOK() || !nextOperation )
    {
      if( final ) final( st );
      prms.set_value( st );
      return;
    }

    Operation<true> *opr = nextOperation.release();
    opr->Run( timeout, std::move( prms ), std::move( final ) );
  }

  //----------------------------------------------------------------------------
  // OperationHandler::HandleResponseWithHosts
  //----------------------------------------------------------------------------
  void PipelineHandler::HandleResponseWithHosts( XRootDStatus *status,
      AnyObject *response, HostList *hostList )
  {
    HandleResponseImpl( status, response, hostList );
  }

  //----------------------------------------------------------------------------
  // OperationHandler::HandleResponse
  //----------------------------------------------------------------------------
  void PipelineHandler::HandleResponse( XRootDStatus *status,
      AnyObject *response )
  {
    HandleResponseImpl( status, response );
  }

  //----------------------------------------------------------------------------
  // OperationHandler::AssignToWorkflow
  //----------------------------------------------------------------------------
  void PipelineHandler::Assign( const Timeout                            &t,
                                std::promise<XRootDStatus>                p,
                                std::function<void(const XRootDStatus&)>  f,
                                Operation<true>                          *opr )
  {
    timeout = t;
    prms    = std::move( p );
    if( !final ) final   = std::move( f );
    else if( f )
    {
      auto f1 = std::move( final );
      final = [f1, f]( const XRootDStatus &st ){ f1( st ); f( st ); };
    }
    currentOperation.reset( opr );
  }

  //------------------------------------------------------------------------
  // Assign the finalization routine
  //------------------------------------------------------------------------
  void PipelineHandler::Assign( std::function<void(const XRootDStatus&)>  f )
  {
    final = std::move( f );
  }

  //------------------------------------------------------------------------
  // Called by a pipeline on the handler of its first operation before Run
  //------------------------------------------------------------------------
  void PipelineHandler::PreparePipelineStart()
  {
    // Move any final-function from the handler of the last operaiton to the
    // first. It will be moved along the pipeline of handlers while the
    // pipeline is run.

    if( final || !nextOperation ) return;
    PipelineHandler *last = nextOperation->handler.get();
    while( last )
    {
      Operation<true> *nextop = last->nextOperation.get();
      if( !nextop ) break;
      last = nextop->handler.get();
    }
    if( last )
    {
      // swap-then-move rather than only move as we need to guarantee that
      // last->final is left without target.
      std::function<void(const XRootDStatus&)> f;
      f.swap( last->final );
      Assign( std::move( f ) );
    }
  }

  //------------------------------------------------------------------------
  // Stop the current pipeline
  //------------------------------------------------------------------------
  void Pipeline::Stop( const XRootDStatus &status )
  {
    throw StopPipeline( status );
  }

  //------------------------------------------------------------------------
  // Repeat current operation
  //------------------------------------------------------------------------
  void Pipeline::Repeat()
  {
    throw RepeatOpeation();
  }

  //------------------------------------------------------------------------
  // Replace current operation
  //------------------------------------------------------------------------
  void Pipeline::Replace( Operation<false> &&opr )
  {
    throw ReplaceOperation( std::move( opr ) );
  }

  //------------------------------------------------------------------------
  // Replace with pipeline
  //------------------------------------------------------------------------
  void Pipeline::Replace( Pipeline p )
  {
    throw ReplacePipeline( std::move( p ) );
  }

  //------------------------------------------------------------------------
  // Ignore error and proceed with the pipeline
  //------------------------------------------------------------------------
  void Pipeline::Ignore()
  {
    throw IgnoreError();
  }
}

