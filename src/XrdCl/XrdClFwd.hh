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

#ifndef SRC_XRDCL_XRDCLFWD_HH_
#define SRC_XRDCL_XRDCLFWD_HH_

#include <memory>
#include <stdexcept>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Helper class for storing forwarded values
  //! Allocates memory respectively aligned for T but constructs the object
  //! only on assignment.
  //!
  //! @arg T : type of the value
  //----------------------------------------------------------------------------
  template<typename T>
  struct FwdStorage
  {
    //--------------------------------------------------------------------------
    //! Default constructor
    //--------------------------------------------------------------------------
    FwdStorage() : ptr( nullptr ) { }

    //--------------------------------------------------------------------------
    //! Constructor from T.
    //! @param value : value for forwarding
    //--------------------------------------------------------------------------
    FwdStorage( const T &value ) : ptr( new( &storage.memory ) T( value ) )
    {
    }

    //--------------------------------------------------------------------------
    //! Assignment operator from T
    //! @param value : value for forwarding
    //--------------------------------------------------------------------------
    FwdStorage& operator=( const T &value )
    {
      ptr = new( &storage.memory ) T( value );
      return *this;
    }

    //--------------------------------------------------------------------------
    //! Move constructor from T.
    //! @param value : value for forwarding
    //--------------------------------------------------------------------------
    FwdStorage( T && value ) : ptr( new( &storage.memory ) T( std::move( value ) ) )
    {
    }

    //--------------------------------------------------------------------------
    //! Move assignment operator from T
    //! @param value : value for forwarding
    //--------------------------------------------------------------------------
    FwdStorage& operator=( T && value )
    {
      ptr = new( &storage.memory ) T( std::move( value ) );
      return *this;
    }

    //--------------------------------------------------------------------------
    //! Destructor
    //--------------------------------------------------------------------------
    ~FwdStorage()
    {
      if( ptr ) ptr->~T();
    }

    //--------------------------------------------------------------------------
    //! Memory for the value
    //--------------------------------------------------------------------------
    union Memory
    {
      //------------------------------------------------------------------------
      //! Make sure the default constructor of T won't be called
      //------------------------------------------------------------------------
      Memory() { }

      //------------------------------------------------------------------------
      //! Make sure the destrutor of T won't be called
      //------------------------------------------------------------------------
      ~Memory() { }

      //------------------------------------------------------------------------
      //! The memory for storing forwarded value
      //------------------------------------------------------------------------
      T memory;
    };

    //--------------------------------------------------------------------------
    //! The memory for storying forwarded value
    //--------------------------------------------------------------------------
    Memory storage;

    //--------------------------------------------------------------------------
    //! Pointer to the forwarded value
    //--------------------------------------------------------------------------
    T *ptr;
  };

  //----------------------------------------------------------------------------
  //! A helper class for forwarding arguments between operations.
  //! In practice it's a wrapper around std::shared_ptr using FwdStorage as
  //! underlying memory.
  //!
  //! @arg T : type of forwarded value
  //----------------------------------------------------------------------------
  template<typename T>
  struct Fwd : protected std::shared_ptr<FwdStorage<T>>
  {
    //------------------------------------------------------------------------
    //! Default constructor.
    //!
    //! Allocates memory for the underlying value object without callying
    //! its constructor.
    //------------------------------------------------------------------------
    Fwd() : std::shared_ptr<FwdStorage<T>>( std::make_shared<FwdStorage<T>>() )
    {
    }

    //------------------------------------------------------------------------
    //! Copy constructor.
    //------------------------------------------------------------------------
    Fwd( const Fwd &fwd ) : std::shared_ptr<FwdStorage<T>>( fwd )
    {
    }

    //------------------------------------------------------------------------
    //! Move constructor.
    //------------------------------------------------------------------------
    Fwd( Fwd && fwd ) : std::shared_ptr<FwdStorage<T>>( std::move( fwd ) )
    {
    }

    //------------------------------------------------------------------------
    //! Initialize from shared_ptr
    //------------------------------------------------------------------------
    Fwd( std::shared_ptr<FwdStorage<T>> && ptr ) : std::shared_ptr<FwdStorage<T>>( std::move( ptr ) )
    {
    }

    //------------------------------------------------------------------------
    //! Constructor from value
    //------------------------------------------------------------------------
    explicit Fwd( const T &value )
    {
      *this->get() = value;
    }

    //------------------------------------------------------------------------
    //! Move construct from value
    //------------------------------------------------------------------------
    explicit Fwd( T &&value )
    {
      *this->get() = std::move( value );
    }

    //------------------------------------------------------------------------
    //! Assignment operator.
    //!
    //! @param value : forwarded value
    //! @throws      : std::logic_error
    //------------------------------------------------------------------------
    Fwd& operator=( const T &value )
    {
      *this->get() = value;
      return *this;
    }

    //------------------------------------------------------------------------
    //! Move assignment operator.
    //!
    //! @param value : forwarded value
    //! @throws      : std::logic_error
    //------------------------------------------------------------------------
    Fwd& operator=( T && value )
    {
      *this->get() = std::move( value );
      return *this;
    }

    //------------------------------------------------------------------------
    //! Dereferencing operator. Note if Fwd has not been assigned with
    //! a value this will trigger an exception
    //!
    //! @return : reference to the underlying value
    //! @throws : std::logic_error
    //------------------------------------------------------------------------
    T& operator*() const
    {
      if( !bool( this->get()->ptr ) ) throw std::logic_error( "XrdCl::Fwd contains no value!" );
      return *this->get()->ptr;
    }

    //------------------------------------------------------------------------
    //! Dereferencing member operator. Note if Fwd has not been assigned with
    //! a value this will trigger an exception
    //!
    //! @return : pointer to the underlying value
    //! @throws : std::logic_error
    //------------------------------------------------------------------------
    T* operator->() const
    {
      if( !bool( this->get()->ptr ) ) throw std::logic_error( "XrdCl::Fwd contains no value!" );
      return this->get()->ptr;
    }

    //------------------------------------------------------------------------
    //! Check if it contains a valid value
    //------------------------------------------------------------------------
    bool Valid() const
    {
      return bool( this->get()->ptr );
    }
  };

  //--------------------------------------------------------------------------
  // Utility function for creating forwardable objects
  //--------------------------------------------------------------------------
  template<typename T, typename... Args>
  inline std::shared_ptr<FwdStorage<T>> make_fwd( Args&&... args )
  {
    return std::make_shared<FwdStorage<T>>( std::forward<Args>( args )... );
  }
}


#endif /* SRC_XRDCL_XRDCLFWD_HH_ */
