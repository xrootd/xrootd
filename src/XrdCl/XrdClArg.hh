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

#ifndef __XRD_CL_OPERATION_PARAMS_HH__
#define __XRD_CL_OPERATION_PARAMS_HH__

#include "XrdCl/XrdClFwd.hh"

#include <string>
#include <sstream>
#include <unordered_map>

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Base class for Arg.
  //----------------------------------------------------------------------------
  template<typename T>
  class ArgBase
  {
    public:

      //------------------------------------------------------------------------
      //! Default Constructor.
      //------------------------------------------------------------------------
      ArgBase()
      {
      }

      //------------------------------------------------------------------------
      //! Destructor.
      //------------------------------------------------------------------------
      virtual ~ArgBase()
      {
      }

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param value : the value of the argument
      //------------------------------------------------------------------------
      ArgBase( T value ) : holder( new PlainValue( std::move( value ) ) )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param ftr : future value of the argument
      //------------------------------------------------------------------------
      ArgBase( std::future<T> &&ftr ) : holder( new FutureValue( std::move( ftr ) ) )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param fwd : forwarded value of the argument
      //------------------------------------------------------------------------
      ArgBase( const Fwd<T> &fwd ) : holder( new FwdValue( fwd ) )
      {
      }

      //------------------------------------------------------------------------
      //! Get Constructor.
      //------------------------------------------------------------------------
      ArgBase( ArgBase &&arg ) : holder( std::move( arg.holder ) )
      {
      }

      //------------------------------------------------------------------------
      //! @return : value of the argument
      //------------------------------------------------------------------------
      T Get()
      {
        if( !holder ) throw std::logic_error( "XrdCl::ArgBase::Get(): value not set." );
        return holder->Get();
      }

    protected:

      //------------------------------------------------------------------------
      //! Abstract class for holding a value
      //------------------------------------------------------------------------
      struct ValueHolder
      {
        //----------------------------------------------------------------------
        //! Virtual Destructor (important ;-).
        //----------------------------------------------------------------------
        virtual ~ValueHolder()
        {
        }

        //----------------------------------------------------------------------
        //! @return : the value
        //----------------------------------------------------------------------
        virtual T Get() = 0;
      };

      //------------------------------------------------------------------------
      //! A helper class for holding plain value
      //------------------------------------------------------------------------
      struct PlainValue : public ValueHolder
      {
          //--------------------------------------------------------------------
          //! Constructor
          //!
          //! @param value : the value to be hold by us
          //--------------------------------------------------------------------
          PlainValue( T &&value ) : value( std::move( value ) )
          {
          }

          //--------------------------------------------------------------------
          //! @return : the value
          //--------------------------------------------------------------------
          T Get()
          {
            return std::move( value );
          }

        private:
          //--------------------------------------------------------------------
          //! the value
          //--------------------------------------------------------------------
          T value;
      };

      //------------------------------------------------------------------------
      //! A helper class for holding future value
      //------------------------------------------------------------------------
      struct FutureValue : public ValueHolder
      {
          //--------------------------------------------------------------------
          //! Constructor
          //!
          //! @param value : the future value to be hold by us
          //--------------------------------------------------------------------
          FutureValue( std::future<T> &&ftr ) : ftr( std::move( ftr ) )
          {
          }

          //--------------------------------------------------------------------
          //! @return : the value
          //--------------------------------------------------------------------
          T Get()
          {
            return ftr.get();
          }

        private:
          //--------------------------------------------------------------------
          //! the future value
          //--------------------------------------------------------------------
          std::future<T> ftr;
      };

      //------------------------------------------------------------------------
      //! A helper class for holding forwarded value
      //------------------------------------------------------------------------
      struct FwdValue : public ValueHolder
      {
          //--------------------------------------------------------------------
          //! Constructor
          //!
          //! @param value : the forwarded value to be hold by us
          //--------------------------------------------------------------------
          FwdValue( const Fwd<T> &fwd ) : fwd( fwd )
          {
          }

          //--------------------------------------------------------------------
          //! @return : the value
          //--------------------------------------------------------------------
          T Get()
          {
            return std::move( *fwd );
          }

        private:
          //--------------------------------------------------------------------
          //! the forwarded value
          //--------------------------------------------------------------------
          Fwd<T> fwd;
      };

      //------------------------------------------------------------------------
      //! Holds the value of the argument
      //------------------------------------------------------------------------
      std::unique_ptr<ValueHolder> holder;
  };

  //----------------------------------------------------------------------------
  //! Operation argument.
  //! The argument is optional, user may initialize it with 'notdef'
  //!
  //! @arg T : real type of the argument
  //----------------------------------------------------------------------------
  template<typename T>
  class Arg : public ArgBase<T>
  {
    public:

      //------------------------------------------------------------------------
      //! Default Constructor.
      //------------------------------------------------------------------------
      Arg()
      {
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param value : value of the argument (will be std::moved)
      //------------------------------------------------------------------------
      Arg( T value ) : ArgBase<T>( std::move( value ) )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param ftr : future value of the argument (will be std::moved)
      //------------------------------------------------------------------------
      Arg( std::future<T> &&ftr ) : ArgBase<T>( std::move( ftr ) )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param fwd : forwarded value of the argument (will be std::moved)
      //------------------------------------------------------------------------
      Arg( const Fwd<T> &fwd ) : ArgBase<T>( fwd )
      {
      }

      //------------------------------------------------------------------------
      //! Get Constructor.
      //------------------------------------------------------------------------
      Arg( Arg &&arg ) : ArgBase<T>( std::move( arg ) )
      {
      }

      //------------------------------------------------------------------------
      //! Get-Assignment.
      //------------------------------------------------------------------------
      Arg& operator=( Arg &&arg )
      {
        if( &arg == this ) return *this;
        this->holder = std::move( arg.holder );
        return *this;
      }
  };

  //----------------------------------------------------------------------------
  //! Operation argument.
  //! Specialized for 'std::string', might be constructed in addition from c-like
  //! string (const char*)
  //----------------------------------------------------------------------------
  template<>
  class Arg<std::string> : public ArgBase<std::string>
  {
    public:

      //------------------------------------------------------------------------
      //! Default Constructor.
      //------------------------------------------------------------------------
      Arg()
      {
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param value : value of the argument
      //------------------------------------------------------------------------
      Arg( std::string str ) : ArgBase<std::string>( str )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param val : value of the argument
      //------------------------------------------------------------------------
      Arg( const char *cstr ) : ArgBase<std::string>( cstr )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //------------------------------------------------------------------------
      Arg( std::future<std::string> &&ftr ) : ArgBase<std::string>( std::move( ftr ) )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //------------------------------------------------------------------------
      Arg( const Fwd<std::string> &fwd ) : ArgBase<std::string>( fwd )
      {
      }


      //------------------------------------------------------------------------
      //! Get Constructor.
      //-----------------------------------------------------------------------
      Arg( Arg &&arg ) : ArgBase<std::string>( std::move( arg ) )
      {
      }

      //------------------------------------------------------------------------
      //! Get-Assignment.
      //------------------------------------------------------------------------
      Arg& operator=( Arg &&arg )
      {
        if( &arg == this ) return *this;
        this->holder = std::move( arg.holder );
        return *this;
      }
  };
}

#endif // __XRD_CL_OPERATION_PARAMS_HH__
