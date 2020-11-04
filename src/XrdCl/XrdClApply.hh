//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------

#ifndef SRC_XRDCL_XRDCLAPPLY_HH_
#define SRC_XRDCL_XRDCLAPPLY_HH_

#include <functional>
#include <tuple>

namespace XrdCl
{

  // This is the type which holds sequences
  template<int ... Is> struct sequence {};

  // First define the template signature
  template <int ... Ns> struct seq_gen;

  // Recursion case
  template <int I, int ... Ns>
  struct seq_gen<I, Ns...>
  {
    using type = typename seq_gen<I - 1, I - 1, Ns...>::type;
  };

  // Recursion abort
  template <int ... Ns>
  struct seq_gen<0, Ns...>
  {
    using type = sequence<Ns...>;
  };

  template <typename FUNC, typename TUPL, int ... INDICES>
  inline static auto tuple_call_impl( FUNC &func, TUPL &args, sequence<INDICES...> ) ->
    decltype( func( std::move( std::get<INDICES>( args ) )... ) )
  {
      return func( std::move( std::get<INDICES>( args ) )... );
  }

  //---------------------------------------------------------------------------
  // Applies tuple members as arguments to the function
  //
  // @param func : function to be called
  // @param tup  : tuple with the packed argumnets
  //
  // Note: Once we can use c++17 we can move to std::apply
  //---------------------------------------------------------------------------
  template <typename FUNC, typename ... ARGs>
  inline static auto Apply( FUNC &&func, std::tuple<ARGs...> &tup ) ->
    decltype( tuple_call_impl( func, tup, typename seq_gen<sizeof...(ARGs)>::type{} ) )
  {
      return tuple_call_impl( func, tup, typename seq_gen<sizeof...(ARGs)>::type{} );
  }

  //---------------------------------------------------------------------------
  // Applies tuple members as arguments to the function
  //
  // @param method : method to be called
  // @param obj    : the object to call the method on
  // @param tup    : tuple with the packed argumnets
  //
  // Note: Once we can use c++17 we can move to std::apply
  //---------------------------------------------------------------------------
  template <typename METH, typename OBJ, typename ... ARGs>
  inline static auto Apply( METH &&method, OBJ &obj, std::tuple<ARGs...> &tup ) ->
    decltype( Apply( std::bind( method, &obj, std::placeholders::_1, std::placeholders::_2 ), tup ) )
  {
    return Apply( std::bind( method, &obj, std::placeholders::_1, std::placeholders::_2 ), tup );
  }

}

#endif /* SRC_XRDCL_XRDCLAPPLY_HH_ */
