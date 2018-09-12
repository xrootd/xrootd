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

#ifndef __XRD_CL_FILE_OPERATIONS_HH__
#define __XRD_CL_FILE_OPERATIONS_HH__

#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClOperations.hh"
#include "XrdCl/XrdClOperationHandlers.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Base class for all file releated operations
  //!
  //! @arg Derived : the class that derives from this template (CRTP)
  //! @arg state   : describes current operation configuration state
  //! @arg Args    : operation arguments
  //----------------------------------------------------------------------------
  template<template<State> class Derived, State state, typename ... Arguments>
  class FileOperation: public ConcreteOperation<Derived, state, Arguments...>
  {

      template<template<State> class, State, typename ...> friend class FileOperation;

    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param f : file on which the operation will be performed
      //------------------------------------------------------------------------
      FileOperation(File *f): file(f)
      {
        static_assert(state == Bare, "Constructor is available only for type Operation<Bare>");
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      FileOperation( FileOperation<Derived, from, Arguments...> && op ) : ConcreteOperation<Derived, state, Arguments...>( std::move( op ) ), file( op.file )
      {

      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~FileOperation()
      {

      }

    protected:

      //------------------------------------------------------------------------
      //! The file object itself
      //------------------------------------------------------------------------
      File *file;
    };

  //----------------------------------------------------------------------------
  //! Open operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class OpenImpl: public FileOperation<OpenImpl, state, Arg<std::string>,
      Arg<OpenFlags::Flags>, Arg<Access::Mode>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      OpenImpl( File *f ) :
          FileOperation<OpenImpl, state, Arg<std::string>,
              Arg<OpenFlags::Flags>, Arg<Access::Mode>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      OpenImpl( File &f ) :
          FileOperation<OpenImpl, state, Arg<std::string>,
              Arg<OpenFlags::Flags>, Arg<Access::Mode>>( &f )
      {

      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      OpenImpl( OpenImpl<from> && open ) :
          FileOperation<OpenImpl, state, Arg<std::string>,
              Arg<OpenFlags::Flags>, Arg<Access::Mode>>( std::move( open ) )
      {

      }

      //------------------------------------------------------------------------
      //! URL Argument Descriptors
      //------------------------------------------------------------------------
      struct UrlArg
      {
          static const int index = 0;
          static const std::string key;
          typedef std::string type;
      };

      //------------------------------------------------------------------------
      //! Flags Argument Descriptors
      //------------------------------------------------------------------------
      struct FlagsArg
      {
          static const int index = 1;
          static const std::string key;
          typedef OpenFlags::Flags type;
      };

      //------------------------------------------------------------------------
      //! Mode Argument Descriptors
      //------------------------------------------------------------------------
      struct ModeArg
      {
          static const int index = 2;
          static const std::string key;
          typedef Access::Mode type;
      };

      //------------------------------------------------------------------------
      //! Overloaded operator() (in order to provide default value for mode)
      //------------------------------------------------------------------------
      OpenImpl<Configured> operator()( Arg<std::string> url,
          Arg<OpenFlags::Flags> flags, Arg<Access::Mode> mode = Access::None )
      {
        return this->ConcreteOperation<OpenImpl, state, Arg<std::string>,
            Arg<OpenFlags::Flags>, Arg<Access::Mode>>::operator ()(
            std::move( url ), std::move( flags ), std::move( mode ) );
      }

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<OpenImpl, state, Arg<std::string>,
          Arg<OpenFlags::Flags>, Arg<Access::Mode>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      OpenImpl<Handled> operator>>(
          std::function<void( XRootDStatus& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      OpenImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, StatInfo& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new ExOpenFuncWrapper(
            *this->file, handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      OpenImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler =
            new SimpleForwardingFunctionWrapper( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      OpenImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, StatInfo&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new ForwardingExOpenFuncWrapper(
            *this->file, handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Open";
      }

    protected:

      //------------------------------------------------------------------------
      //! Run operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> &params, int bucket = 1 )
      {
        try
        {
          std::string &url = Get<UrlArg>( this->args, params, bucket );
          OpenFlags::Flags &flags = Get<FlagsArg>( this->args, params, bucket );
          Access::Mode &mode = Get<ModeArg>( this->args, params, bucket );
          return this->file->Open( url, flags, mode, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef OpenImpl<Bare> Open;
  template<State state> const std::string OpenImpl<state>::UrlArg::key   = "url";
  template<State state> const std::string OpenImpl<state>::FlagsArg::key = "flags";
  template<State state> const std::string OpenImpl<state>::ModeArg::key  = "mode";

  //----------------------------------------------------------------------------
  //! Read operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class ReadImpl: public FileOperation<ReadImpl, state, Arg<uint64_t>,
      Arg<uint32_t>, Arg<void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      ReadImpl( File *f ) :
          FileOperation<ReadImpl, state, Arg<uint64_t>, Arg<uint32_t>,
              Arg<void*>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      ReadImpl( File &f ) :
          FileOperation<ReadImpl, state, Arg<uint64_t>, Arg<uint32_t>,
              Arg<void*>>( &f )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      ReadImpl( ReadImpl<from> && read ) :
          FileOperation<ReadImpl, state, Arg<uint64_t>, Arg<uint32_t>,
              Arg<void*>>( std::move( read ) )
      {
      }

      struct OffsetArg
      {
          static const int index = 0;
          static const std::string key;
          typedef uint64_t type;
      };

      struct SizeArg
      {
          static const int index = 1;
          static const std::string key;
          typedef uint32_t type;
      };

      struct BufferArg
      {
          static const int index = 2;
          static const std::string key;
          typedef void* type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<ReadImpl, state, Arg<uint64_t>, Arg<uint32_t>,
          Arg<void*>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      ReadImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, ChunkInfo& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new FunctionWrapper<ChunkInfo>(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      ReadImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, ChunkInfo&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<
            ChunkInfo>( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Read";
      }

    protected:

      //------------------------------------------------------------------------
      //! Run operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> &params, int bucket = 1 )
      {
        try
        {
          uint64_t &offset = Get<OffsetArg>( this->args, params, bucket );
          uint32_t &size = Get<SizeArg>( this->args, params, bucket );
          void *buffer = Get<BufferArg>( this->args, params, bucket );
          return this->file->Read( offset, size, buffer, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef ReadImpl<Bare> Read;
  template<State state> const std::string ReadImpl<state>::OffsetArg::key = "offset";
  template<State state> const std::string ReadImpl<state>::SizeArg::key   = "size";
  template<State state> const std::string ReadImpl<state>::BufferArg::key = "buffer";

  //----------------------------------------------------------------------------
  //! Close operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state = Bare>
  class CloseImpl: public FileOperation<CloseImpl, state>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      CloseImpl( File *f ) :
          FileOperation<CloseImpl, state>( f )
      {

      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      CloseImpl( File &f ) :
          FileOperation<CloseImpl, state>( &f )
      {

      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      CloseImpl( CloseImpl<from> && close ) :
          FileOperation<CloseImpl, state>( std::move( close ) )
      {

      }

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<CloseImpl, state>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      CloseImpl<Handled> operator>>(
          std::function<void( XRootDStatus& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      CloseImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler =
            new SimpleForwardingFunctionWrapper( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Close";
      }

    protected:

      //------------------------------------------------------------------------
      //! Run operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> &params, int bucket = 1 )
      {
        return this->file->Close( this->handler.get() );
      }
  };
  typedef CloseImpl<Bare> Close;

  //----------------------------------------------------------------------------
  //! Stat operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state = Bare>
  class StatImpl: public FileOperation<StatImpl, state, Arg<bool>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      StatImpl( File *f ) :
          FileOperation<StatImpl, state, Arg<bool>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      StatImpl( File &f ) :
          FileOperation<StatImpl, state, Arg<bool>>( &f )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      StatImpl( StatImpl<from> && stat ) :
          FileOperation<StatImpl, state, Arg<bool>>( std::move( stat ) )
      {

      }

      struct ForceArg
      {
          static const int index = 0;
          static const std::string key;
          typedef bool type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<StatImpl, state, Arg<bool>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      StatImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, StatInfo& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new FunctionWrapper<StatInfo>(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      StatImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, StatInfo&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<
            StatInfo>( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Stat";
      }

    protected:

      //------------------------------------------------------------------------
      //! Run operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> &params, int bucket = 1 )
      {
        try
        {
          bool force = Get<ForceArg>( this->args, params, bucket );
          return this->file->Stat( force, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  template<State state> const std::string StatImpl<state>::ForceArg::key = "force";

  //----------------------------------------------------------------------------
  //! Factory for creating StatImpl objects (as there is another Stat in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  StatImpl<Bare> Stat( File *file )
  {
    return StatImpl<Bare>( file );
  }

  //----------------------------------------------------------------------------
  //! Factory for creating StatImpl objects (as there is another Stat in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  StatImpl<Bare> Stat( File &file )
  {
    return StatImpl<Bare>( file );
  }

  //----------------------------------------------------------------------------
  //! Write operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class WriteImpl: public FileOperation<WriteImpl, state, Arg<uint64_t>,
      Arg<uint32_t>, Arg<void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      WriteImpl( File *f ) :
          FileOperation<WriteImpl, state, Arg<uint64_t>, Arg<uint32_t>,
              Arg<void*>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      WriteImpl( File &f ) :
          FileOperation<WriteImpl, state, Arg<uint64_t>, Arg<uint32_t>,
              Arg<void*>>( &f )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      WriteImpl( WriteImpl<from> && write ) :
          FileOperation<WriteImpl, state, Arg<uint64_t>, Arg<uint32_t>,
              Arg<void*>>( std::move( write ) )
      {
      }

      struct OffsetArg
      {
          static const int index = 0;
          static const std::string key;
          typedef uint64_t type;
      };

      struct SizeArg
      {
          static const int index = 1;
          static const std::string key;
          typedef uint32_t type;
      };

      struct BufferArg
      {
          static const int index = 2;
          static const std::string key;
          typedef void* type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<WriteImpl, state, Arg<uint64_t>, Arg<uint32_t>,
          Arg<void*>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      WriteImpl<Handled> operator>>(
          std::function<void( XRootDStatus& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      WriteImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler =
            new SimpleForwardingFunctionWrapper( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Write";
      }

    protected:

      //------------------------------------------------------------------------
      //! Run operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> &params, int bucket = 1 )
      {
        try
        {
          uint64_t &offset = Get<OffsetArg>( this->args, params, bucket );
          uint32_t &size = Get<SizeArg>( this->args, params, bucket );
          void *buffer = Get<BufferArg>( this->args, params, bucket );
          return this->file->Write( offset, size, buffer, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef WriteImpl<Bare> Write;
  template<State state> const std::string WriteImpl<state>::OffsetArg::key = "offset";
  template<State state> const std::string WriteImpl<state>::SizeArg::key   = "size";
  template<State state> const std::string WriteImpl<state>::BufferArg::key = "buffer";

  //----------------------------------------------------------------------------
  //! Sync operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state = Bare>
  class SyncImpl: public FileOperation<SyncImpl, state>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      SyncImpl( File *f ) :
          FileOperation<SyncImpl, state>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      SyncImpl( File &f ) :
          FileOperation<SyncImpl, state>( &f )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      SyncImpl( SyncImpl<from> && sync ) :
          FileOperation<SyncImpl, state>( std::move( sync ) )
      {
      }

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<SyncImpl, state>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      SyncImpl<Handled> operator>>(
          std::function<void( XRootDStatus& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      SyncImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler =
            new SimpleForwardingFunctionWrapper( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Sync";
      }

    protected:

      //------------------------------------------------------------------------
      //! Run operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> &params, int bucket = 1 )
      {
        return this->file->Sync( this->handler.get() );
      }
  };
  typedef SyncImpl<Bare> Sync;

  //----------------------------------------------------------------------------
  //! Truncate operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class TruncateImpl: public FileOperation<TruncateImpl, state, Arg<uint64_t>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      TruncateImpl( File *f ) :
          FileOperation<TruncateImpl, state, Arg<uint64_t>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      TruncateImpl( File &f ) :
          FileOperation<TruncateImpl, state, Arg<uint64_t>>( &f )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      TruncateImpl( TruncateImpl<from> && trunc ) :
          FileOperation<TruncateImpl, state, Arg<uint64_t>>(
              std::move( trunc ) )
      {
      }

      struct SizeArg
      {
          static const int index = 0;
          static const std::string key;
          typedef uint64_t type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<TruncateImpl, state, Arg<uint64_t>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      TruncateImpl<Handled> operator>>(
          std::function<void( XRootDStatus& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      TruncateImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler =
            new SimpleForwardingFunctionWrapper( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Truncate";
      }

    protected:

      //------------------------------------------------------------------------
      //! Run operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> &params, int bucket = 1 )
      {
        try
        {
          uint64_t &size = Get<SizeArg>( this->args, params, bucket );
          return this->file->Truncate( size, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  template<State state> const std::string TruncateImpl<state>::SizeArg::key = "size";

  //----------------------------------------------------------------------------
  //! Factory for creating TruncateImpl objects (as there is another Stat in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  TruncateImpl<Bare> Truncate( File *file )
  {
    return TruncateImpl<Bare>( file );
  }

  //----------------------------------------------------------------------------
  //! Factory for creating TruncateImpl objects (as there is another Stat in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  TruncateImpl<Bare> Truncate( File &file )
  {
    return TruncateImpl<Bare>( file );
  }

  //----------------------------------------------------------------------------
  //! VectorRead operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class VectorReadImpl: public FileOperation<VectorReadImpl, state,
      Arg<ChunkList>, Arg<void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      VectorReadImpl( File *f ) :
          FileOperation<VectorReadImpl, state, Arg<ChunkList>, Arg<void*>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      VectorReadImpl( File &f ) :
          FileOperation<VectorReadImpl, state, Arg<ChunkList>, Arg<void*>>( &f )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      VectorReadImpl( VectorReadImpl<from> && vread ) :
          FileOperation<VectorReadImpl, state, Arg<ChunkList>, Arg<void*>>(
              std::move( vread ) )
      {
      }

      struct ChunksArg
      {
          static const int index = 0;
          static const std::string key;
          typedef ChunkList type;
      };

      struct BufferArg
      {
          static const int index = 1;
          static const std::string key;
          typedef char* type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<VectorReadImpl, state, Arg<ChunkList>, Arg<void*>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      VectorReadImpl<Handled> operator>>(
          std::function<void( XRootDStatus& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      VectorReadImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler =
            new SimpleForwardingFunctionWrapper( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "VectorRead";
      }

    protected:

      //------------------------------------------------------------------------
      //! Run operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> &params, int bucket = 1 )
      {
        try
        {
          const ChunkList &chunks = Get<ChunksArg>( this->args, params,
              bucket );
          void *buffer = Get<BufferArg>( this->args, params, bucket );
          return this->file->VectorRead( chunks, buffer, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef VectorReadImpl<Bare> VectorRead;
  template<State state> const std::string VectorReadImpl<state>::ChunksArg::key = "chunks";
  template<State state> const std::string VectorReadImpl<state>::BufferArg::key = "buffer";

  //----------------------------------------------------------------------------
  //! VectorWrite operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class VectorWriteImpl: public FileOperation<VectorWriteImpl, state,
      Arg<ChunkList>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      VectorWriteImpl( File *f ) :
          FileOperation<VectorWriteImpl, state, Arg<ChunkList>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      VectorWriteImpl( File &f ) :
          FileOperation<VectorWriteImpl, state, Arg<ChunkList>>( &f )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      VectorWriteImpl( VectorWriteImpl<from> && vwrite ) :
          FileOperation<VectorWriteImpl, state, Arg<ChunkList>>(
              std::move( vwrite ) )
      {
      }

      struct ChunksArg
      {
          static const int index = 0;
          static const std::string key;
          typedef ChunkList type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<VectorWriteImpl, state, Arg<ChunkList>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      VectorWriteImpl<Handled> operator>>(
          std::function<void( XRootDStatus& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      VectorWriteImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler =
            new SimpleForwardingFunctionWrapper( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "VectorWrite";
      }

    protected:

      //------------------------------------------------------------------------
      //! Run operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> &params, int bucket = 1 )
      {
        try
        {
          const ChunkList& chunks = Get<ChunksArg>( this->args, params,
              bucket );
          return this->file->VectorWrite( chunks, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef VectorWriteImpl<Bare> VectorWrite;
  template<State state> const std::string VectorWriteImpl<state>::ChunksArg::key = "chunks";

  //----------------------------------------------------------------------------
  //! WriteV operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class WriteVImpl: public FileOperation<WriteVImpl, state, Arg<uint64_t>,
      Arg<struct iovec*>, Arg<int>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      WriteVImpl( File *f ) :
          FileOperation<WriteVImpl, state, Arg<uint64_t>, Arg<struct iovec*>,
              Arg<int>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      WriteVImpl( File &f ) :
          FileOperation<WriteVImpl, state, Arg<uint64_t>, Arg<struct iovec*>,
              Arg<int>>( &f )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      WriteVImpl( WriteVImpl<from> && writev ) :
          FileOperation<WriteVImpl, state, Arg<uint64_t>, Arg<struct iovec*>,
              Arg<int>>( std::move( writev ) )
      {
      }

      struct OffsetArg
      {
          static const int index = 0;
          static const std::string key;
          typedef uint64_t type;
      };

      struct IovArg
      {
          static const int index = 1;
          static const std::string key;
          typedef struct iovec* type;
      };

      struct IovcntArg
      {
          static const int index = 2;
          static const std::string key;
          typedef int type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<WriteVImpl, state, Arg<uint64_t>,
          Arg<struct iovec*>, Arg<int>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      WriteVImpl<Handled> operator>>(
          std::function<void( XRootDStatus& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      WriteVImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler =
            new SimpleForwardingFunctionWrapper( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "WriteV";
      }

    protected:

      //------------------------------------------------------------------------
      //! Run operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> &params, int bucket = 1 )
      {
        try
        {
          uint64_t &offset = Get<OffsetArg>( this->args, params, bucket );
          const struct iovec *iov = Get<IovArg>( this->args, params, bucket );
          int &iovcnt = Get<IovcntArg>( this->args, params, bucket );
          return this->file->WriteV( offset, iov, iovcnt, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }

      }
  };
  typedef WriteVImpl<Bare> WriteV;
  template<State state> const std::string WriteVImpl<state>::OffsetArg::key = "offset";
  template<State state> const std::string WriteVImpl<state>::IovArg::key    = "iov";
  template<State state> const std::string WriteVImpl<state>::IovcntArg::key = "iovcnt";

  //----------------------------------------------------------------------------
  //! Fcntl operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class FcntlImpl: public FileOperation<FcntlImpl, state, Arg<Buffer>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      FcntlImpl( File *f ) :
          FileOperation<FcntlImpl, state, Arg<Buffer>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      FcntlImpl( File &f ) :
          FileOperation<FcntlImpl, state, Arg<Buffer>>( &f )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      FcntlImpl( FcntlImpl<from> && fcntl ) :
          FileOperation<FcntlImpl, state, Arg<Buffer>>( std::move( fcntl ) )
      {
      }

      struct BufferArg
      {
          static const int index = 0;
          static const std::string key;
          typedef Buffer type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<FcntlImpl, state, Arg<Buffer>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      FcntlImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, Buffer& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new FunctionWrapper<Buffer>(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      FcntlImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, Buffer&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<
            Buffer>( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Fcntl";
      }

    protected:

      //------------------------------------------------------------------------
      //! Run operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> &params, int bucket = 1 )
      {
        try
        {
          const Buffer& arg = Get<BufferArg>( this->args, params, bucket );
          return this->file->Fcntl( arg, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef FcntlImpl<Bare> Fcntl;
  template<State state> const std::string FcntlImpl<state>::BufferArg::key = "arg";

  //----------------------------------------------------------------------------
  //! Visa operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state = Bare>
  class VisaImpl: public FileOperation<VisaImpl, state>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      VisaImpl( File *f ) :
          FileOperation<VisaImpl, state>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      VisaImpl( File &f ) :
          FileOperation<VisaImpl, state>( &f )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      VisaImpl( VisaImpl<from> && visa ) :
          FileOperation<VisaImpl, state>( std::move( visa ) )
      {
      }

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<VisaImpl, state>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      VisaImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, Buffer& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new FunctionWrapper<Buffer>(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      VisaImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, Buffer&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<
            Buffer>( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Visa";
      }

    protected:

      //------------------------------------------------------------------------
      //! Run operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus Run( std::shared_ptr<ArgsContainer> &params, int bucket = 1 )
      {
        return this->file->Visa( this->handler.get() );
      }
  };
  typedef VisaImpl<Bare> Visa;
}

#endif // __XRD_CL_FILE_OPERATIONS_HH__

