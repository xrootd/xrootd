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

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Parallel operations, allows to execute two or more pipelines in
  //! parallel.
  //!
  //! @arg state : describes current operation configuration state
  //!              (@see Operation)
  //----------------------------------------------------------------------------
  template<State state = Bare>
  class ParallelOperation: public ConcreteOperation<ParallelOperation, state>
  {
      template<State> friend class ParallelOperation;

    public:

      //------------------------------------------------------------------------
      //! Constructor: copy-move a ParallelOperation in different state
      //------------------------------------------------------------------------
      template<State from>
      ParallelOperation( ParallelOperation<from> &&obj ) :
          ConcreteOperation<ParallelOperation, state>( std::move( obj ) ), pipelines(
              std::move( obj.pipelines ) )
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
        static_assert(state == Configured, "Constructor is available only for type ParallelOperations<Configured>");

        pipelines.reserve( container.size() );
        auto begin = std::make_move_iterator( container.begin() );
        auto end   = std::make_move_iterator( container.end() );
        std::copy( begin, end, std::back_inserter( pipelines ) );
        container.clear(); // there's junk inside so we clear it
      }

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<ParallelOperation, state>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      ParallelOperation<Handled> operator>>(
          std::function<void( XRootDStatus& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : operation name
      //------------------------------------------------------------------------
      std::string ToString()
      {
        std::ostringstream oss;
        oss << "Parallel(";
        for( int i = 0; i < pipelines.size(); i++ )
        {
          oss << pipelines[i]->ToString();
          if( i != pipelines.size() - 1 )
          {
            oss << " && ";
          }
        }
        oss << ")";
        return oss.str();
      }

    private:

      //------------------------------------------------------------------------
      //! Run operation
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( const std::shared_ptr<ArgsContainer> &params,
                            int                                   bucketDefault = 0 ) // TODO that's wrong ! we sync in the handler !
      {
        for( int i = 0; i < pipelines.size(); i++ )
        {
          int bucket = i + 1;
          pipelines[i].Run( params, bucket );
        }

        for( auto &pipe : pipelines )
        {
          XRootDStatus st = pipe.ftr.get();
          if( !st.IsOK() )
          {
            this->handler->HandleResponse( new XRootDStatus( st ), nullptr );
            return st;
          }
        }

        this->handler->HandleResponse( new XRootDStatus(), nullptr );
        return XRootDStatus();
      }

      std::vector<Pipeline> pipelines;
  };

  //----------------------------------------------------------------------------
  //! Factory function for creating parallel operation from a vector
  //----------------------------------------------------------------------------
  template<class Container>
  ParallelOperation<Configured> Parallel( Container &container )
  {
    return ParallelOperation<Configured>( container );
  }

  //----------------------------------------------------------------------------
  //! Helper function for converting parameter pack into a vector
  //----------------------------------------------------------------------------
  void PipesToVec( std::vector<Pipeline>& )
  {
    // base case
  }

  //----------------------------------------------------------------------------
  // Declare PipesToVec (we need to do declare those functions ahead of
  // definitions, as they may call each other.
  //----------------------------------------------------------------------------
  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Operation<Configured> &operation,
      Others&... others );

  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Operation<Handled> &operation,
      Others&... others );

  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Pipeline &pipeline,
      Others&... others );

  //----------------------------------------------------------------------------
  // Define PipesToVec
  //----------------------------------------------------------------------------
  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Operation<Configured> &operation,
      Others&... others )
  {
    v.emplace_back( operation );
    PipesToVec( v, others... );
  }

  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Operation<Handled> &operation,
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
  //! (we use && reference since due to reference colapsing this will fit
  //! both r- and l-value references)
  //----------------------------------------------------------------------------
  template<typename ... Operations>
  ParallelOperation<Configured> Parallel( Operations&& ... operations )
  {
    constexpr size_t size = sizeof...( operations );
    std::vector<Pipeline> v;
    v.reserve( size );
    PipesToVec( v, operations... );
    return Parallel( v );
  }
}

#endif // __XRD_CL_OPERATIONS_HH__
