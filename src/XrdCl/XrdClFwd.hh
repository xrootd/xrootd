/*
 * XrdClFwd.hh
 *
 *  Created on: Oct 19, 2018
 *      Author: simonm
 */

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
      if( ptr ) throw std::logic_error( "XrdCl::Fwd already contains value." );
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
      if( ptr ) throw std::logic_error( "XrdCl::Fwd already contains value." );
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
      //! Make sure the default constructor of T wont be called
      //------------------------------------------------------------------------
      Memory() { }

      //------------------------------------------------------------------------
      //! Make sure the destrutor of T wont be called
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
  class Fwd : protected std::shared_ptr<FwdStorage<T>>
  {
    public:

      //------------------------------------------------------------------------
      //! Default constructor.
      //!
      //! Allocates memory for the underlying value object without callying
      //! its constructor.
      //------------------------------------------------------------------------
      Fwd() : std::shared_ptr<FwdStorage<T>>( new FwdStorage<T>() )
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
      //! Assignment operator. Note: the object can be assigned only once.
      //! Reassignment will trigger an exception
      //!
      //! @param value : forwarded value
      //! @throws      : std::logic_error
      //------------------------------------------------------------------------
      const Fwd& operator=( const T &value ) const
      {
        *this->get() = value;
        return *this;
      }

      //------------------------------------------------------------------------
      //! Move assignment operator. Note: the object can be assigned only once.
      //! Reassignment will trigger an exception
      //!
      //! @param value : forwarded value
      //! @throws      : std::logic_error
      //------------------------------------------------------------------------
      const Fwd& operator=( T && value ) const
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

  };
}


#endif /* SRC_XRDCL_XRDCLFWD_HH_ */
