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

#ifndef SRC_XRDCL_XRDCLCTX_HH_
#define SRC_XRDCL_XRDCLCTX_HH_

#include <memory>

namespace XrdCl
{
  //---------------------------------------------------------------------------
  //! Utility class for storing a pointer to operation context
  //---------------------------------------------------------------------------
  template<typename T>
  struct Ctx : protected std::shared_ptr<T*>
  {
    //-------------------------------------------------------------------------
    //! Default constructor
    //-------------------------------------------------------------------------
    Ctx() : std::shared_ptr<T*>( std::make_shared<T*>() )
    {
    }

    //-------------------------------------------------------------------------
    //! Constructor (from pointer)
    //-------------------------------------------------------------------------
    Ctx( T *ctx ) : std::shared_ptr<T*>( std::make_shared<T*>( ctx ) )
    {
    }

    //-------------------------------------------------------------------------
    //! Constructor (from reference)
    //-------------------------------------------------------------------------
    Ctx( T &ctx ) : std::shared_ptr<T*>( std::make_shared<T*>( &ctx ) )
    {
    }

    //-------------------------------------------------------------------------
    //! Copy constructor
    //-------------------------------------------------------------------------
    Ctx( const Ctx &ctx ) : std::shared_ptr<T*>( ctx )
    {
    }

    //-------------------------------------------------------------------------
    //! Move constructor
    //-------------------------------------------------------------------------
    Ctx( Ctx &&ctx ) : std::shared_ptr<T*>( std::move( ctx ) )
    {
    }

    //-------------------------------------------------------------------------
    //! Assignment operator (from pointer)
    //-------------------------------------------------------------------------
    Ctx& operator=( T *ctx )
    {
      *this->get() = ctx;
      return *this;
    }

    //-------------------------------------------------------------------------
    //! Assignment operator (from reference)
    //-------------------------------------------------------------------------
    Ctx& operator=( T &ctx )
    {
      *this->get() = &ctx;
      return *this;
    }

    //------------------------------------------------------------------------
    //! Dereferencing operator. Note if Ctx is a null-reference this will
    //! trigger an exception
    //!
    //! @return : reference to the underlying value
    //! @throws : std::logic_error
    //------------------------------------------------------------------------
    T& operator*() const
    {
      if( !bool( *this->get() ) ) throw std::logic_error( "XrdCl::Ctx contains no value!" );
      return **this->get();
    }

    //------------------------------------------------------------------------
    //! Dereferencing member operator. Note if Ctx is a null-reference
    //! this will trigger an exception
    //!
    //! @return : pointer to the underlying value
    //! @throws : std::logic_error
    //------------------------------------------------------------------------
    T* operator->() const
    {
      if( !bool( *this->get() ) ) throw std::logic_error( "XrdCl::Ctx contains no value!" );
      return *this->get();
    }
  };
}


#endif /* SRC_XRDCL_XRDCLCTX_HH_ */
