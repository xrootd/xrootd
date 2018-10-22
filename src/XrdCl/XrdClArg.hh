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

#include <string>
#include <sstream>
#include <unordered_map>

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! A argument that has not been defined
  //----------------------------------------------------------------------------
  class NotDefArg
  {
  } notdef; //> a global for specifying not defined arguments

  //----------------------------------------------------------------------------
  //! Operation argument.
  //! The argument is optional, user may initialize it with 'notdef'
  //!
  //! @arg T : real type of the argument
  //----------------------------------------------------------------------------
  template<typename T>
  class Arg
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param val : value of the argument
      //------------------------------------------------------------------------
      Arg( T val ) :
          empty( false )
      {
        value = val;
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //------------------------------------------------------------------------
      Arg() : empty( true )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param notdef : place holder meaning the argument is not defined yet
      //------------------------------------------------------------------------
      Arg( NotDefArg notdef ) :
          empty( true )
      {
      }

      //------------------------------------------------------------------------
      //! Move Constructor.
      //------------------------------------------------------------------------
      Arg( Arg && opt ) :
          value( std::move( opt.value ) )
      {
        empty = opt.empty;
        opt.empty = true;
      }

      //------------------------------------------------------------------------
      //! Move-Assignment.
      //------------------------------------------------------------------------
      Arg& operator=( Arg &&arg )
      {
        value = std::move( arg.value );
        empty = arg.empty;
        arg.empty = true;
        return *this;
      }

      //------------------------------------------------------------------------
      //! @return : true if the argument has not been defined yet,
      //!           false otherwise
      //------------------------------------------------------------------------
      bool IsEmpty()
      {
        return empty;
      }

      //------------------------------------------------------------------------
      //! @return : value of the argument
      //------------------------------------------------------------------------
      T& GetValue()
      {
        if( IsEmpty() )
        {
          throw std::logic_error(
              "Cannot get parameter: value has not been specified" );
        }
        return value;
      }

    private:

      //------------------------------------------------------------------------
      //! value of the argument
      //------------------------------------------------------------------------
      T value;

      //------------------------------------------------------------------------
      //! true if the argument has not been defined yet, false otherwise
      //------------------------------------------------------------------------
      bool empty;
  };

  //----------------------------------------------------------------------------
  //! Operation argument.
  //! Specialized for 'std::string', migh be constructed in addition from c-like
  //! string (const char*)
  //!
  //! @arg T : real type of the argument
  //----------------------------------------------------------------------------
  template<>
  class Arg<std::string>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param val : value of the argument
      //------------------------------------------------------------------------
      Arg( const std::string& str ) :
          empty( false )
      {
        value = str;
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param val : value of the argument
      //------------------------------------------------------------------------
      Arg( const char *val ) :
          empty( false )
      {
        value = std::string( val );
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //------------------------------------------------------------------------
      Arg() : empty( true )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor.
      //!
      //! @param notdef : place holder meaning the argument is not defined yet
      //------------------------------------------------------------------------
      Arg( NotDefArg notdef ) :
          empty( true )
      {
      }

      //------------------------------------------------------------------------
      //! Move Constructor.
      //-----------------------------------------------------------------------
      Arg( Arg &&arg ) :
          value( std::move( arg.value ) )
      {
        empty = arg.empty;
        arg.empty = true;
      }

      //------------------------------------------------------------------------
      //! Move-Assignment.
      //------------------------------------------------------------------------
      Arg& operator=( Arg &&opt )
      {
        value = std::move( opt.value );
        empty = opt.empty;
        opt.empty = true;
        return *this;
      }

      //------------------------------------------------------------------------
      //! @return : true if the argument has not been defined yet,
      //!           false otherwise
      //------------------------------------------------------------------------
      bool IsEmpty()
      {
        return empty;
      }

      //------------------------------------------------------------------------
      //! @return : value of the argument
      //------------------------------------------------------------------------
      std::string& GetValue()
      {
        if( IsEmpty() )
        {
          throw std::logic_error(
              "Cannot get parameter: value has not been specified" );
        }
        return value;
      }

    private:

      //------------------------------------------------------------------------
      //! value of the argument
      //------------------------------------------------------------------------
      std::string value;

      //------------------------------------------------------------------------
      //! true if the argument has not been defined yet, false otherwise
      //------------------------------------------------------------------------
      bool empty;
  };

  //----------------------------------------------------------------------------
  //! Container with argument for forwarding
  //----------------------------------------------------------------------------
  class ArgsContainer
  {
    public:

      //------------------------------------------------------------------------
      //! Get an argument from container
      //!
      //! @arg ArgDesc  : descryptor of the argument type
      //!
      //! @param bucket : bucket number of the desired argument
      //------------------------------------------------------------------------
      template<typename ArgDesc>
      typename ArgDesc::type& GetArg( int bucket = 1 )
      {
        if( !Exists( ArgDesc::key, bucket ) )
        {
          std::ostringstream oss;
          oss << "Parameter " << ArgDesc::key << " has not been specified in bucket "
              << bucket;
          throw std::logic_error( oss.str() );
        }
        AnyObject *obj = paramsMap[bucket][ArgDesc::key];
        typename ArgDesc::type *valuePtr = nullptr;
        obj->Get( valuePtr );
        return *valuePtr;
      }

      //------------------------------------------------------------------------
      //! Set an argument in container
      //!
      //! @arg ArgDesc  : descryptor of the argument type
      //!
      //! @param bucket : bucket number of the desired argument
      //------------------------------------------------------------------------
      template<typename ArgDesc>
      void SetArg( typename ArgDesc::type value, int bucket = 1 )
      {
        if( !BucketExists( bucket ) )
        {
          paramsMap[bucket];
        }
        if( paramsMap[bucket].find( ArgDesc::key ) != paramsMap[bucket].end() )
        {
          std::ostringstream oss;
          oss << "Parameter " << ArgDesc::key << " has already been set in bucket "
              << bucket;
          throw std::logic_error( oss.str() );
        }
        typename ArgDesc::type *valuePtr = new typename ArgDesc::type( value );
        AnyObject *obj = new AnyObject();
        obj->Set( valuePtr, true );
        paramsMap[bucket][ArgDesc::key] = obj;
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~ArgsContainer()
      {
        auto buckets = paramsMap.begin();
        // Destroy dynamically allocated objects stored in map
        while( buckets != paramsMap.end() )
        {
          auto& objectsMap = buckets->second;
          auto it = objectsMap.begin();
          while( it != objectsMap.end() )
          {
            AnyObject* obj = it->second;
            it++;
            delete obj;
          }
          buckets++;
        }
      }

    private:

      //------------------------------------------------------------------------
      //! Check if given key exists in given bucket
      //!
      //! @param key    : key of the argument
      //! @param bucket : the bucket we want to check
      //!
      //! @return       : true if the argument exists, false otherwise
      //------------------------------------------------------------------------
      bool Exists( const std::string &key, int bucket = 1 )
      {
        return paramsMap.find( bucket ) != paramsMap.end()
            && paramsMap[bucket].find( key ) != paramsMap[bucket].end();
      }

      //------------------------------------------------------------------------
      //! Check if given bucket exist
      //!
      //! @param bucket : bucket number
      //!
      //! @return       : true if the bucket exists, false otherwise
      //------------------------------------------------------------------------
      bool BucketExists( int bucket )
      {
        return paramsMap.find( bucket ) != paramsMap.end();
      }

      //------------------------------------------------------------------------
      //! map of buckets with arguments
      //------------------------------------------------------------------------
      std::unordered_map<int, std::unordered_map<std::string, AnyObject*>> paramsMap;
  };

  //----------------------------------------------------------------------------
  //! Operation Context.
  //!
  //! Acts as an additional parameter to a lambda so it can forward arguments.
  //----------------------------------------------------------------------------
  class OperationContext
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param paramsContainer : container with argument
      //------------------------------------------------------------------------
      OperationContext( std::shared_ptr<ArgsContainer> paramsContainer ) :
          container( paramsContainer )
      {
      }

      //------------------------------------------------------------------------
      //! Forward argument to next operation in pipeline
      //!
      //! @arg ArgDesc  : descryptor of the argument type
      //!
      //! @param value  : value of the argument
      //! @param bucket : the bucket where the argument should be stored
      //------------------------------------------------------------------------
      template<typename ArgDesc>
      void FwdArg( typename ArgDesc::type value, int bucket = 1 )
      {
        container->SetArg <ArgDesc> ( value, bucket );
      }

    private:

      //------------------------------------------------------------------------
      //! Container with arguments for forwarding
      //------------------------------------------------------------------------
      std::shared_ptr<ArgsContainer> container;
  };

}

#endif // __XRD_CL_OPERATION_PARAMS_HH__
