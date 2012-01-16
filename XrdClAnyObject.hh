//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_ANY_OBJECT_HH__
#define __XRD_CL_ANY_OBJECT_HH__

#include <typeinfo>
#include <cstring>

namespace XrdClient
{
  //----------------------------------------------------------------------------
  //! Simple implementation of a type safe any object
  //! It would have been a better idea to use boost::any here but we don't
  //! want to depend on boost
  //----------------------------------------------------------------------------
  class AnyObject
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      AnyObject(): pPtr(0) {};

      //------------------------------------------------------------------------
      //! Grab an object
      //------------------------------------------------------------------------
      template <class Type> void Set( Type ptr )
      {
        if( !ptr )
          return;
        pPtr = ptr;
        pTypeInfo = &typeid( Type );
      }

      //------------------------------------------------------------------------
      //! Retrieve the object being held
      //------------------------------------------------------------------------
      template <class Type> void Get( Type &ptr )
      {
        if( !pPtr || (strcmp( pTypeInfo->name(), typeid( Type ).name() )) )
        {
          ptr = 0;
          return;
        }
        ptr = static_cast<Type>( pPtr );
      }

    private:
      void                 *pPtr;
      const std::type_info *pTypeInfo;
  };
}

#endif // __XRD_CL_ANY_OBJECT_HH__
