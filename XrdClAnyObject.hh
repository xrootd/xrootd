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
  //! Simple implementation of a type safe holder for any object pointer
  //! It would have been a better idea to use boost::any here but we don't
  //! want to depend on boost
  //----------------------------------------------------------------------------
  class AnyObject
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      AnyObject(): pHolder(0), pTypeInfo(0), pOwn( true ) {};

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~AnyObject()
      {
        if( pHolder && pOwn )
          pHolder->Delete();
        delete pHolder;
      }

      //------------------------------------------------------------------------
      //! Grab an object
      //! By default the ownership of the object is taken as well, ie.
      //! the object will be deleted when the AnyObject holding it is deleted.
      //! To release an object grab a zero pointer, ie. (int *)0
      //!
      //! @param object object pointer
      //! @param own    take the ownership or not
      //------------------------------------------------------------------------
      template <class Type> void Set( Type object, bool own = true )
      {
        if( !object )
        {
          delete pHolder;
          pHolder   = 0;
          pTypeInfo = 0;
          return;
        }

        delete pHolder;
        pHolder   = new ConcreteHolder<Type>( object );
        pOwn      = own;
        pTypeInfo = &typeid( Type );
      }

      //------------------------------------------------------------------------
      //! Retrieve the object being held
      //------------------------------------------------------------------------
      template <class Type> void Get( Type &object )
      {
        if( !pHolder || (strcmp( pTypeInfo->name(), typeid( Type ).name() )) )
        {
          object = 0;
          return;
        }
        object = static_cast<Type>( pHolder->Get() );
      }

      //------------------------------------------------------------------------
      //! Check if we own the object being stored
      //------------------------------------------------------------------------
      bool HasOwnership() const
      {
        return pOwn;
      }

    private:
      //------------------------------------------------------------------------
      // Abstract holder object
      //------------------------------------------------------------------------
      class Holder
      {
        public:
          virtual void Delete() = 0;
          virtual void *Get()   = 0;
      };

      //------------------------------------------------------------------------
      // Concrete holder
      //------------------------------------------------------------------------
      template<class Type>
      class ConcreteHolder: public Holder
      {
        public:
          ConcreteHolder( Type object  ): pObject( object ) {}
          virtual void Delete()
          {
            delete pObject;
          }

          virtual void *Get()
          {
            return (void *)pObject;
          }

        private:
          Type pObject;
      };

      Holder               *pHolder;
      const std::type_info *pTypeInfo;
      bool                  pOwn;
  };
}

#endif // __XRD_CL_ANY_OBJECT_HH__
