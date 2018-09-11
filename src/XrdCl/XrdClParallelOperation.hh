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
  //-----------------------------------------------------------------------
  //! Parallel operations
  //!
  //! @tparam state   describes current operation configuration state
  //-----------------------------------------------------------------------
  template<State state = Bare>
  class ParallelOperation: public ConcreteOperation<ParallelOperation, state>
  {
      template<State> friend class ParallelOperation;

    public:

      template<State from>
      ParallelOperation( ParallelOperation<from> &&obj ) :
          ConcreteOperation<ParallelOperation, state>( std::move( obj ) ), workflows(
              std::move( obj.workflows ) )
      {
      }

      template<class Container>
      ParallelOperation( Container &container )
      {
        static_assert(state == Configured, "Constructor is available only for type ParallelOperations<Configured>");
        auto itr = container.begin();
        for( ; itr != container.end(); ++itr )
        {
          std::unique_ptr<Workflow> w( new Workflow( *itr, false ) );
          workflows.push_back( std::move( w ) );
        }
      }

      using ConcreteOperation<ParallelOperation, state>::operator>>;

      ParallelOperation<Handled> operator>>( std::function<void( XRootDStatus& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
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

  //-----------------------------------------------------------------------
  //! Factory function for creating parallel operation from a vector
  //-----------------------------------------------------------------------
  template<class Container>
  ParallelOperation<Configured> Parallel( Container &container )
  {
    return ParallelOperation<Configured>( container );
  }

  //-----------------------------------------------------------------------
  //! Helper function for converting parameter pack into a vector
  //-----------------------------------------------------------------------
  void PipesToVec( std::vector<Pipeline>& )
  {
    // base case
  }

  //-----------------------------------------------------------------------
  // Declare PipesToVec (we need to do declare those functions ahead of
  // definitions, as they may call each other.
  //-----------------------------------------------------------------------
  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Operation<Configured> &operation, Others&... others );

  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Operation<Handled>    &operation, Others&... others );

  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Pipeline              &pipeline,  Others&... others );

  //-----------------------------------------------------------------------
  // Define PipesToVec
  //-----------------------------------------------------------------------
  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Operation<Configured> &operation, Others&... others )
  {
    v.emplace_back( operation );
    PipesToVec( v, others... );
  }

  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Operation<Handled> &operation, Others&... others )
  {
    v.emplace_back( operation );
    PipesToVec( v, others... );
  }

  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Pipeline &pipeline, Others&... others )
  {
    v.emplace_back( std::move( pipeline ) );
    PipesToVec( v, others... );
  }

  //-----------------------------------------------------------------------
  //! Factory function for creating parallel operation from
  //! a given number of operations
  //! (we use && reference since due to reference colapsing this will fit
  //! both r- and l-value references)
  //-----------------------------------------------------------------------
  template<typename ... Operations>
  ParallelOperation<Configured> Parallel( Operations&& ... operations )
  {
    constexpr size_t size = sizeof...( operations );
    std::vector<Pipeline> v; v.reserve( size );
    PipesToVec( v, operations... );
    return Parallel( v );
  }
}

#endif // __XRD_CL_OPERATIONS_HH__
