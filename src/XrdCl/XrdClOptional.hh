//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
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

#ifndef __XRD_CL_OPTIONAL_HH__
#define __XRD_CL_OPTIONAL_HH__

#include <utility>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! none object for initializing empty Optional
  //----------------------------------------------------------------------------
  static struct None{ } none;

  //----------------------------------------------------------------------------
  //! The Optional class
  //!
  //! @arg T : type of the optional parameter
  //----------------------------------------------------------------------------
  template<typename T>
  class Optional
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor for value
      //------------------------------------------------------------------------
      Optional( const T& t ) : optional( false )
      {
        new( &memory.value ) T( t );
      }

      //------------------------------------------------------------------------
      //! Default constructor
      //------------------------------------------------------------------------
      Optional( const None& n = none ) : optional( true )
      {
        (void)n;
      }

      //------------------------------------------------------------------------
      //! Copy constructor
      //------------------------------------------------------------------------
      Optional( const Optional& opt ) : optional( opt.optional )
      {
        if( !optional ) new( &memory.value ) T( opt.memory.value );
      }

      //------------------------------------------------------------------------
      //! Move constructor
      //------------------------------------------------------------------------
      Optional( Optional && opt ) : optional( opt.optional )
      {
        if( !optional ) new( &memory.value ) T( std::move( opt.memory.value ) );
      }

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      ~Optional()
      {
        if( optional ) memory.value.~T();
      }

      //------------------------------------------------------------------------
      //! Copy assignment operator
      //------------------------------------------------------------------------
      Optional& operator=( const Optional& opt )
      {
        if( this != &opt )
        {
          optional = opt.optional;
          if( !optional ) memory.value = opt.memory.value;
        }
        return *this;
      }

      //------------------------------------------------------------------------
      //! Move assignment operator
      //------------------------------------------------------------------------
      Optional& operator=( Optional&& opt )
      {
        if( this != &opt )
        {
          optional = opt.optional;
          if( !optional ) memory.value = std::move( opt.memory.value );
        }
        return *this;
      }

      //------------------------------------------------------------------------
      //! Conversion to boolean
      //------------------------------------------------------------------------
      operator bool() const
      {
        return optional;
      }

      //------------------------------------------------------------------------
      //! Dereference operator
      //------------------------------------------------------------------------
      T& operator*()
      {
        return memory.value;
      }

      //------------------------------------------------------------------------
      //! Dereference operator
      //------------------------------------------------------------------------
      const T& operator*() const
      {
        return memory.value;
      }

    private:

      //------------------------------------------------------------------------
      //! true if the value is optional, false otherwise
      //------------------------------------------------------------------------
      bool optional;

      //------------------------------------------------------------------------
      //! we use union as this is the only way to obtain memory with correct
      //! alignment and don't actually construct the object
      //------------------------------------------------------------------------
      union Storage
      {
        //----------------------------------------------------------------------
        //! value of the optional variable, if the variable is optional is
        //! remains uninitialized
        //----------------------------------------------------------------------
        T value;
        //----------------------------------------------------------------------
        //! Default constructor
        //----------------------------------------------------------------------
        inline Storage(){ }
        //----------------------------------------------------------------------
        // Destructor
        //----------------------------------------------------------------------
        inline ~Storage(){ };
      } memory; //> memory storage for the optional variable
  };
}

#endif // __XRD_CL_OPTIONAL_HH__
