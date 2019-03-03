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

#ifndef __XRD_CL_PARALLELOPERATION_HH__
#define __XRD_CL_PARALLELOPERATION_HH__

#include "XrdCl/XrdClOperations.hh"
#include "XrdCl/XrdClOperationHandlers.hh"

#include <atomic>

namespace XrdCl
{

  class ParallelHandler: public PipelineHandler
  {

  };

  //----------------------------------------------------------------------------
  //! Parallel operations, allows to execute two or more pipelines in
  //! parallel.
  //!
  //! @arg state : describes current operation configuration state
  //!              (@see Operation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class ParallelOperation: public ConcreteOperation<ParallelOperation, HasHndl, Resp<void>>
  {
      template<bool> friend class ParallelOperation;

    public:

      //------------------------------------------------------------------------
      //! Constructor: copy-move a ParallelOperation in different state
      //------------------------------------------------------------------------
      template<bool from>
      ParallelOperation( ParallelOperation<from> &&obj ) :
          ConcreteOperation<ParallelOperation, HasHndl, Resp<void>>( std::move( obj ) ),
            pipelines( std::move( obj.pipelines ) )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @arg   Container : iterable container type
      //!
      //! @param container : iterable container with pipelines
      //------------------------------------------------------------------------
      template<class Container>
      ParallelOperation( Container &&container )
      {
        static_assert( !HasHndl, "Constructor is available only operation without handler");

        pipelines.reserve( container.size() );
        auto begin = std::make_move_iterator( container.begin() );
        auto end   = std::make_move_iterator( container.end() );
        std::copy( begin, end, std::back_inserter( pipelines ) );
        container.clear(); // there's junk inside so we clear it
      }

      //------------------------------------------------------------------------
      //! @return : operation name
      //------------------------------------------------------------------------
      std::string ToString()
      {
        std::ostringstream oss;
        oss << "Parallel(";
        for( size_t i = 0; i < pipelines.size(); i++ )
        {
          oss << pipelines[i]->ToString();
          if( i + 1  != pipelines.size() )
          {
            oss << " && ";
          }
        }
        oss << ")";
        return oss.str();
      }

    private:

      //------------------------------------------------------------------------
      //! Helper class for handling the PipelineHandler of the
      //! ParallelOperation (RAII).
      //!
      //! Guarantees that the handler will be executed exactly once.
      //------------------------------------------------------------------------
      struct Ctx
      {
        //----------------------------------------------------------------------
        //! Constructor.
        //!
        //! @param handler : the PipelineHandler of the Parallel operation
        //----------------------------------------------------------------------
        Ctx( PipelineHandler *handler ): handler( handler )
        {

        }

        //----------------------------------------------------------------------
        //! Destructor.
        //----------------------------------------------------------------------
        ~Ctx()
        {
          Handle( XRootDStatus() );
        }

        //----------------------------------------------------------------------
        //! Forwards the status to the PipelineHandler if the handler haven't
        //! been called yet.
        //!
        //! @param st : status
        //----------------------------------------------------------------------
        void Handle( const XRootDStatus &st )
        {
          PipelineHandler* hdlr = handler.exchange( nullptr );
          if( hdlr )
            hdlr->HandleResponse( new XRootDStatus( st ), nullptr );
        }

        //----------------------------------------------------------------------
        //! PipelineHandler of the ParallelOperation
        //----------------------------------------------------------------------
        std::atomic<PipelineHandler*> handler;
      };

      //------------------------------------------------------------------------
      //! Run operation
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        std::shared_ptr<Ctx> ctx( new Ctx( this->handler.release() ) );

        try
        {
          for( size_t i = 0; i < pipelines.size(); ++i )
          {
            pipelines[i].Run( [ctx]( const XRootDStatus &st ){ if( !st.IsOK() ) ctx->Handle( st ); } );
          }
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }

        return XRootDStatus();
      }

      std::vector<Pipeline> pipelines;
  };

  //----------------------------------------------------------------------------
  //! Factory function for creating parallel operation from a vector
  //----------------------------------------------------------------------------
  template<class Container>
  ParallelOperation<false> Parallel( Container &container )
  {
    return ParallelOperation<false>( container );
  }

  //----------------------------------------------------------------------------
  //! Helper function for converting parameter pack into a vector
  //----------------------------------------------------------------------------
  inline void PipesToVec( std::vector<Pipeline>& )
  {
    // base case
  }

  //----------------------------------------------------------------------------
  // Declare PipesToVec (we need to do declare those functions ahead of
  // definitions, as they may call each other.
  //----------------------------------------------------------------------------
  template<typename ... Others>
  inline void PipesToVec( std::vector<Pipeline> &v, Operation<false> &operation,
      Others&... others );

  template<typename ... Others>
  inline void PipesToVec( std::vector<Pipeline> &v, Operation<true> &operation,
      Others&... others );

  template<typename ... Others>
  inline void PipesToVec( std::vector<Pipeline> &v, Pipeline &pipeline,
      Others&... others );

  //----------------------------------------------------------------------------
  // Define PipesToVec
  //----------------------------------------------------------------------------
  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Operation<false> &operation,
      Others&... others )
  {
    v.emplace_back( operation );
    PipesToVec( v, others... );
  }

  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Operation<true> &operation,
      Others&... others )
  {
    v.emplace_back( operation );
    PipesToVec( v, others... );
  }

  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Pipeline &pipeline,
      Others&... others )
  {
    v.emplace_back( std::move( pipeline ) );
    PipesToVec( v, others... );
  }

  //----------------------------------------------------------------------------
  //! Factory function for creating parallel operation from
  //! a given number of operations
  //! (we use && reference since due to reference collapsing this will fit
  //! both r- and l-value references)
  //----------------------------------------------------------------------------
  template<typename ... Operations>
  ParallelOperation<false> Parallel( Operations&& ... operations )
  {
    constexpr size_t size = sizeof...( operations );
    std::vector<Pipeline> v;
    v.reserve( size );
    PipesToVec( v, operations... );
    return Parallel( v );
  }
}

#endif // __XRD_CL_OPERATIONS_HH__
