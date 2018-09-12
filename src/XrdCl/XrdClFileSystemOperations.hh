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

#ifndef __XRD_CL_FILE_SYSTEM_OPERATIONS_HH__
#define __XRD_CL_FILE_SYSTEM_OPERATIONS_HH__

#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClOperations.hh"
#include "XrdCl/XrdClOperationHandlers.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Base class for all file system releated operations
  //!
  //! @arg Derived : the class that derives from this template (CRTP)
  //! @arg state   : describes current operation configuration state
  //! @arg Args    : operation arguments
  //----------------------------------------------------------------------------
  template<template<State> class Derived, State state, typename ... Args>
  class FileSystemOperation: public ConcreteOperation<Derived, state, Args...>
  {

      template<template<State> class, State, typename ...> friend class FileSystemOperation;

    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param fs : file system on which the operation will be performed
      //------------------------------------------------------------------------
      explicit FileSystemOperation(FileSystem *fs): filesystem(fs)
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
      FileSystemOperation( FileSystemOperation<Derived, from, Args...> && op ): ConcreteOperation<Derived, state, Args...>( std::move( op ) ), filesystem( op.filesystem )
      {

      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~FileSystemOperation()
      {

      }

    protected:

      //------------------------------------------------------------------------
      //! The file system object itself.
      //------------------------------------------------------------------------
      FileSystem *filesystem;
  };

  //----------------------------------------------------------------------------
  //! Locate operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class LocateImpl: public FileSystemOperation<LocateImpl, state,
      Arg<std::string>, Arg<OpenFlags::Flags>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      LocateImpl( FileSystem *fs ) :
          FileSystemOperation<LocateImpl, state, Arg<std::string>,
              Arg<OpenFlags::Flags>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      LocateImpl( FileSystem &fs ) :
          FileSystemOperation<LocateImpl, state, Arg<std::string>,
              Arg<OpenFlags::Flags>>( &fs )
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
      LocateImpl( LocateImpl<from> && locate ) :
          FileSystemOperation<LocateImpl, state, Arg<std::string>,
              Arg<OpenFlags::Flags>>( std::move( locate ) )
      {
      }

      //------------------------------------------------------------------------
      //! Path Argument Descriptors
      //------------------------------------------------------------------------
      struct PathArg
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
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<LocateImpl, state, Arg<std::string>,
          Arg<OpenFlags::Flags>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      LocateImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, LocationInfo& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler =
            new FunctionWrapper<LocationInfo>( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      LocateImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, LocationInfo&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<
            LocationInfo>( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Locate";
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
          std::string &path = Get<PathArg>( this->args, params, bucket );
          OpenFlags::Flags &flags = Get<FlagsArg>( this->args, params, bucket );
          return this->filesystem->Locate( path, flags, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef LocateImpl<Bare> Locate;
  template<State state> const std::string LocateImpl<state>::PathArg::key  = "path";
  template<State state> const std::string LocateImpl<state>::FlagsArg::key = "flags";

  //----------------------------------------------------------------------------
  //! DeepLocate operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class DeepLocateImpl: public FileSystemOperation<DeepLocateImpl, state,
      Arg<std::string>, Arg<OpenFlags::Flags>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      DeepLocateImpl( FileSystem *fs ) :
          FileSystemOperation<DeepLocateImpl, state, Arg<std::string>,
              Arg<OpenFlags::Flags>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      DeepLocateImpl( FileSystem &fs ) :
          FileSystemOperation<DeepLocateImpl, state, Arg<std::string>,
              Arg<OpenFlags::Flags>>( &fs )
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
      DeepLocateImpl( DeepLocateImpl<from> && locate ) :
          FileSystemOperation<DeepLocateImpl, state, Arg<std::string>,
              Arg<OpenFlags::Flags>>( std::move( locate ) )
      {
      }

      //------------------------------------------------------------------------
      //! Path Argument Descriptors
      //------------------------------------------------------------------------
      struct PathArg
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
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<DeepLocateImpl, state, Arg<std::string>,
          Arg<OpenFlags::Flags>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      DeepLocateImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, LocationInfo& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler =
            new FunctionWrapper<LocationInfo>( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      DeepLocateImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, LocationInfo&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<
            LocationInfo>( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "DeepLocate";
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
          std::string &path = Get<PathArg>( this->args, params, bucket );
          OpenFlags::Flags &flags = Get<FlagsArg>( this->args, params, bucket );
          return this->filesystem->DeepLocate( path, flags,
              this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef DeepLocateImpl<Bare> DeepLocate;
  template<State state> const std::string DeepLocateImpl<state>::PathArg::key  = "path";
  template<State state> const std::string DeepLocateImpl<state>::FlagsArg::key = "flags";

  //----------------------------------------------------------------------------
  //! Mv operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class MvImpl: public FileSystemOperation<MvImpl, state, Arg<std::string>,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      MvImpl( FileSystem *fs ) :
          FileSystemOperation<MvImpl, state, Arg<std::string>, Arg<std::string>>(
              fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      MvImpl( FileSystem &fs ) :
          FileSystemOperation<MvImpl, state, Arg<std::string>, Arg<std::string>>(
              &fs )
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
      MvImpl( MvImpl<from> && mv ) :
          FileSystemOperation<MvImpl, state, Arg<std::string>, Arg<std::string>>(
              std::move( mv ) )
      {
      }

      //------------------------------------------------------------------------
      //! Source Argument Descriptors
      //------------------------------------------------------------------------
      struct SourceArg
      {
          static const int index = 0;
          static const std::string key;
          typedef std::string type;
      };

      //------------------------------------------------------------------------
      //! Dest Argument Descriptors
      //------------------------------------------------------------------------
      struct DestArg
      {
          static const int index = 1;
          static const std::string key;
          typedef std::string type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<MvImpl, state, Arg<std::string>, Arg<std::string>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      MvImpl<Handled> operator>>(
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
      MvImpl<Handled> operator>>(
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
        return "Mv";
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
          std::string &source = Get<SourceArg>( this->args, params, bucket );
          std::string &dest = Get<DestArg>( this->args, params, bucket );
          return this->filesystem->Mv( source, dest, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef MvImpl<Bare> Mv;
  template<State state> const std::string MvImpl<state>::SourceArg::key = "source";
  template<State state> const std::string MvImpl<state>::DestArg::key   = "dest";

  //----------------------------------------------------------------------------
  //! Query operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class QueryImpl: public FileSystemOperation<QueryImpl, state,
      Arg<QueryCode::Code>, Arg<Buffer>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      QueryImpl( FileSystem *fs ) :
          FileSystemOperation<QueryImpl, state, Arg<QueryCode::Code>,
              Arg<Buffer>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      QueryImpl( FileSystem &fs ) :
          FileSystemOperation<QueryImpl, state, Arg<QueryCode::Code>,
              Arg<Buffer>>( &fs )
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
      QueryImpl( QueryImpl<from> && query ) :
          FileSystemOperation<QueryImpl, state, Arg<QueryCode::Code>,
              Arg<Buffer>>( std::move( query ) )
      {
      }

      //------------------------------------------------------------------------
      //! QueryCode Argument Descriptors
      //------------------------------------------------------------------------
      struct QueryCodeArg
      {
          static const int index = 0;
          static const std::string key;
          typedef QueryCode::Code type;
      };

      //------------------------------------------------------------------------
      //! Buffer Argument Descriptors
      //------------------------------------------------------------------------
      struct BufferArg
      {
          static const int index = 1;
          static const std::string key;
          typedef Buffer type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<QueryImpl, state, Arg<QueryCode::Code>,
          Arg<Buffer>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      QueryImpl<Handled> operator>>(
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
      QueryImpl<Handled> operator>>(
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
        return "Query";
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
          QueryCode::Code &queryCode = Get<QueryCodeArg>( this->args, params,
              bucket );
          const Buffer &arg = Get<BufferArg>( this->args, params, bucket );
          return this->filesystem->Query( queryCode, arg, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef QueryImpl<Bare> Query;
  template<State state> const std::string QueryImpl<state>::QueryCodeArg::key = "queryCode";
  template<State state> const std::string QueryImpl<state>::BufferArg::key    = "arg";

  //----------------------------------------------------------------------------
  //! Truncate operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class TruncateFsImpl: public FileSystemOperation<TruncateFsImpl, state,
      Arg<std::string>, Arg<uint64_t>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      TruncateFsImpl( FileSystem *fs ) :
          FileSystemOperation<TruncateFsImpl, state, Arg<std::string>,
              Arg<uint64_t>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      TruncateFsImpl( FileSystem &fs ) :
          FileSystemOperation<TruncateFsImpl, state, Arg<std::string>,
              Arg<uint64_t>>( &fs )
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
      TruncateFsImpl( TruncateFsImpl<from> && trunc ) :
          FileSystemOperation<TruncateFsImpl, state, Arg<std::string>,
              Arg<uint64_t>>( std::move( trunc ) )
      {
      }

      //------------------------------------------------------------------------
      //! Path Argument Descriptors
      //------------------------------------------------------------------------
      struct PathArg
      {
          static const int index = 0;
          static const std::string key;
          typedef std::string type;
      };

      //------------------------------------------------------------------------
      //! Size Argument Descriptors
      //------------------------------------------------------------------------
      struct SizeArg
      {
          static const int index = 1;
          static const std::string key;
          typedef uint64_t type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<TruncateFsImpl, state, Arg<std::string>,
          Arg<uint64_t>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      TruncateFsImpl<Handled> operator>>(
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
      TruncateFsImpl<Handled> operator>>(
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
          std::string &path = Get<PathArg>( this->args, params, bucket );
          uint64_t &size = Get<SizeArg>( this->args, params, bucket );
          return this->filesystem->Truncate( path, size, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  template<State state> const std::string TruncateFsImpl<state>::PathArg::key = "path";
  template<State state> const std::string TruncateFsImpl<state>::SizeArg::key = "size";

  TruncateFsImpl<Bare> Truncate( FileSystem *fs )
  {
    return TruncateFsImpl<Bare>( fs );
  }

  TruncateFsImpl<Bare> Truncate( FileSystem &fs )
  {
    return TruncateFsImpl<Bare>( fs );
  }

  //----------------------------------------------------------------------------
  //! Rm operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class RmImpl: public FileSystemOperation<RmImpl, state, Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      RmImpl( FileSystem *fs ) :
          FileSystemOperation<RmImpl, state, Arg<std::string>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      RmImpl( FileSystem &fs ) :
          FileSystemOperation<RmImpl, state, Arg<std::string>>( &fs )
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
      RmImpl( RmImpl<from> && rm ) :
          FileSystemOperation<RmImpl, state, Arg<std::string>>(
              std::move( rm ) )
      {
      }

      //------------------------------------------------------------------------
      //! Path Argument Descriptors
      //------------------------------------------------------------------------
      struct PathArg
      {
          static const int index = 0;
          static const std::string key;
          typedef std::string type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<RmImpl, state, Arg<std::string>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      RmImpl<Handled> operator>>(
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
      RmImpl<Handled> operator>>(
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
        return "Rm";
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
          std::string& path = Get<PathArg>( this->args, params, bucket );
          return this->filesystem->Rm( path, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef RmImpl<Bare> Rm;
  template<State state> const std::string RmImpl<state>::PathArg::key = "path";

  //----------------------------------------------------------------------------
  //! MkDir operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class MkDirImpl: public FileSystemOperation<MkDirImpl, state,
      Arg<std::string>, Arg<MkDirFlags::Flags>, Arg<Access::Mode>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      MkDirImpl( FileSystem *fs ) :
          FileSystemOperation<MkDirImpl, state, Arg<std::string>,
              Arg<MkDirFlags::Flags>, Arg<Access::Mode>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      MkDirImpl( FileSystem &fs ) :
          FileSystemOperation<MkDirImpl, state, Arg<std::string>,
              Arg<MkDirFlags::Flags>, Arg<Access::Mode>>( &fs )
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
      MkDirImpl( MkDirImpl<from> && mkdir ) :
          FileSystemOperation<MkDirImpl, state, Arg<std::string>,
              Arg<MkDirFlags::Flags>, Arg<Access::Mode>>( std::move( mkdir ) )
      {
      }

      //------------------------------------------------------------------------
      //! Path Argument Descriptors
      //------------------------------------------------------------------------
      struct PathArg
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
          typedef MkDirFlags::Flags type;
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
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<MkDirImpl, state, Arg<std::string>,
          Arg<MkDirFlags::Flags>, Arg<Access::Mode>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      MkDirImpl<Handled> operator>>(
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
      MkDirImpl<Handled> operator>>(
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
        return "MkDir";
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
          std::string &path = Get<PathArg>( this->args, params, bucket );
          MkDirFlags::Flags &flags = Get<FlagsArg>( this->args, params,
              bucket );
          Access::Mode &mode = Get<ModeArg>( this->args, params, bucket );
          return this->filesystem->MkDir( path, flags, mode,
              this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef MkDirImpl<Bare> MkDir;
  template<State state> const std::string MkDirImpl<state>::PathArg::key  = "path";
  template<State state> const std::string MkDirImpl<state>::FlagsArg::key = "flags";
  template<State state> const std::string MkDirImpl<state>::ModeArg::key  = "mode";

  //----------------------------------------------------------------------------
  //! RmDir operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class RmDirImpl: public FileSystemOperation<RmDirImpl, state, Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      RmDirImpl( FileSystem *fs ) :
          FileSystemOperation<RmDirImpl, state, Arg<std::string>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      RmDirImpl( FileSystem &fs ) :
          FileSystemOperation<RmDirImpl, state, Arg<std::string>>( &fs )
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
      RmDirImpl( RmDirImpl<from> && rmdir ) :
          FileSystemOperation<RmDirImpl, state, Arg<std::string>>(
              std::move( rmdir ) )
      {
      }

      //------------------------------------------------------------------------
      //! Path Argument Descriptors
      //------------------------------------------------------------------------
      struct PathArg
      {
          static const int index = 0;
          static const std::string key;
          typedef std::string type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<RmDirImpl, state, Arg<std::string>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      RmDirImpl<Handled> operator>>(
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
      RmDirImpl<Handled> operator>>(
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
        return "RmDir";
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
          std::string &path = Get<PathArg>( this->args, params, bucket );
          return this->filesystem->RmDir( path, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef RmDirImpl<Bare> RmDir;
  template<State state> const std::string RmDirImpl<state>::PathArg::key = "path";

  //----------------------------------------------------------------------------
  //! ChMod operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class ChModImpl: public FileSystemOperation<ChModImpl, state,
      Arg<std::string>, Arg<Access::Mode>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      ChModImpl( FileSystem *fs ) :
          FileSystemOperation<ChModImpl, state, Arg<std::string>,
              Arg<Access::Mode>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      ChModImpl( FileSystem &fs ) :
          FileSystemOperation<ChModImpl, state, Arg<std::string>,
              Arg<Access::Mode>>( &fs )
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
      ChModImpl( ChModImpl<from> && chmod ) :
          FileSystemOperation<ChModImpl, state, Arg<std::string>,
              Arg<Access::Mode>>( std::move( chmod ) )
      {
      }

      //------------------------------------------------------------------------
      //! Path Argument Descriptors
      //------------------------------------------------------------------------
      struct PathArg
      {
          static const int index = 0;
          static const std::string key;
          typedef std::string type;
      };

      //------------------------------------------------------------------------
      //! Mode Argument Descriptors
      //------------------------------------------------------------------------
      struct ModeArg
      {
          static const int index = 1;
          static const std::string key;
          typedef Access::Mode type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<ChModImpl, state, Arg<std::string>,
          Arg<Access::Mode>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      ChModImpl<Handled> operator>>(
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
      ChModImpl<Handled> operator>>(
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
        return "ChMod";
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
          std::string &path = Get<PathArg>( this->args, params, bucket );
          Access::Mode &mode = Get<ModeArg>( this->args, params, bucket );
          return this->filesystem->ChMod( path, mode, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef ChModImpl<Bare> ChMod;
  template<State state> const std::string ChModImpl<state>::PathArg::key = "path";
  template<State state> const std::string ChModImpl<state>::ModeArg::key = "mode";

  //----------------------------------------------------------------------------
  //! Ping operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class PingImpl: public FileSystemOperation<PingImpl, state>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      PingImpl( FileSystem *fs ) :
          FileSystemOperation<PingImpl, state>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      PingImpl( FileSystem &fs ) :
          FileSystemOperation<PingImpl, state>( &fs )
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
      PingImpl( PingImpl<from> && ping ) :
          FileSystemOperation<PingImpl, state>( std::move( ping ) )
      {
      }

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<PingImpl, state>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      PingImpl<Handled> operator>>(
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
      PingImpl<Handled> operator>>(
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
        return "Ping";
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
          return this->filesystem->Ping( this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef PingImpl<Bare> Ping;

  //----------------------------------------------------------------------------
  //! Stat operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class StatFsImpl: public FileSystemOperation<StatFsImpl, state,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      StatFsImpl( FileSystem *fs ) :
          FileSystemOperation<StatFsImpl, state, Arg<std::string>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      StatFsImpl( FileSystem &fs ) :
          FileSystemOperation<StatFsImpl, state, Arg<std::string>>( &fs )
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
      StatFsImpl( StatFsImpl<from> && statfs ) :
          FileSystemOperation<StatFsImpl, state, Arg<std::string>>(
              std::move( statfs ) )
      {
      }

      //------------------------------------------------------------------------
      //! Path Argument Descriptors
      //------------------------------------------------------------------------
      struct PathArg
      {
          static const int index = 0;
          static const std::string key;
          typedef std::string type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<StatFsImpl, state, Arg<std::string>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      StatFsImpl<Handled> operator>>(
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
      StatFsImpl<Handled> operator>>(
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
          std::string &path = Get<PathArg>( this->args, params, bucket );
          return this->filesystem->RmDir( path, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  template<State state> const std::string StatFsImpl<state>::PathArg::key = "path";

  StatFsImpl<Bare> Stat( FileSystem *fs )
  {
    return StatFsImpl<Bare>( fs );
  }

  StatFsImpl<Bare> Stat( FileSystem &fs )
  {
    return StatFsImpl<Bare>( fs );
  }

  //----------------------------------------------------------------------------
  //! StatVS operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class StatVFSImpl: public FileSystemOperation<StatVFSImpl, state,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      StatVFSImpl( FileSystem *fs ) :
          FileSystemOperation<StatVFSImpl, state, Arg<std::string>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      StatVFSImpl( FileSystem &fs ) :
          FileSystemOperation<StatVFSImpl, state, Arg<std::string>>( &fs )
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
      StatVFSImpl( StatVFSImpl<state> && statvfs ) :
          FileSystemOperation<StatVFSImpl, state, Arg<std::string>>(
              std::move( statvfs ) )
      {
      }

      //------------------------------------------------------------------------
      //! Path Argument Descriptors
      //------------------------------------------------------------------------
      struct PathArg
      {
          static const int index = 0;
          static const std::string key;
          typedef std::string type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<StatVFSImpl, state, Arg<std::string>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      StatVFSImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, StatInfoVFS& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new FunctionWrapper<StatInfoVFS>(
            handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      StatVFSImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, StatInfoVFS&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<
            StatInfoVFS>( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "StatVFS";
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
          std::string &path = Get<PathArg>( this->args, params, bucket );
          return this->filesystem->StatVFS( path, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef StatVFSImpl<Bare> StatVFS;
  template<State state> const std::string StatVFSImpl<state>::PathArg::key = "path";

  //----------------------------------------------------------------------------
  //! Protocol operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class ProtocolImpl: public FileSystemOperation<ProtocolImpl, state>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      ProtocolImpl( FileSystem *fs ) :
          FileSystemOperation<ProtocolImpl, state>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      ProtocolImpl( FileSystem &fs ) :
          FileSystemOperation<ProtocolImpl, state>( &fs )
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
      ProtocolImpl( ProtocolImpl<from> && prot ) :
          FileSystemOperation<ProtocolImpl, state>( std::move( prot ) )
      {
      }

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<ProtocolImpl, state>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      ProtocolImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, ProtocolInfo& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler =
            new FunctionWrapper<ProtocolInfo>( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      ProtocolImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, ProtocolInfo&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<
            ProtocolInfo>( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Protocol";
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
          return this->filesystem->Protocol( this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef ProtocolImpl<Bare> Protocol;

  //----------------------------------------------------------------------------
  //! DirList operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class DirListImpl: public FileSystemOperation<DirListImpl, state,
      Arg<std::string>, Arg<DirListFlags::Flags>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      DirListImpl( FileSystem *fs ) :
          FileSystemOperation<DirListImpl, state, Arg<std::string>,
              Arg<DirListFlags::Flags>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      DirListImpl( FileSystem &fs ) :
          FileSystemOperation<DirListImpl, state, Arg<std::string>,
              Arg<DirListFlags::Flags>>( &fs )
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
      DirListImpl( DirListImpl<from> && dirls ) :
          FileSystemOperation<DirListImpl, state, Arg<std::string>,
              Arg<DirListFlags::Flags>>( std::move( dirls ) )
      {
      }

      //------------------------------------------------------------------------
      //! Path Argument Descriptors
      //------------------------------------------------------------------------
      struct PathArg
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
          typedef DirListFlags::Flags type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<DirListImpl, state, Arg<std::string>,
          Arg<DirListFlags::Flags>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      DirListImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, DirectoryList& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler =
            new FunctionWrapper<DirectoryList>( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      DirListImpl<Handled> operator>>(
          std::function<void( XRootDStatus&, DirectoryList&, OperationContext& )> handleFunction )
      {
        ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<
            DirectoryList>( handleFunction );
        return this->StreamImpl( forwardingHandler );
      }

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "DirList";
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
          std::string &path = Get<PathArg>( this->args, params, bucket );
          DirListFlags::Flags &flags = Get<FlagsArg>( this->args, params,
              bucket );
          return this->filesystem->DirList( path, flags, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef DirListImpl<Bare> DirList;
  template<State state> const std::string DirListImpl<state>::PathArg::key  = "path";
  template<State state> const std::string DirListImpl<state>::FlagsArg::key = "flags";

  //----------------------------------------------------------------------------
  //! SendInfo operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class SendInfoImpl: public FileSystemOperation<SendInfoImpl, state,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      SendInfoImpl( FileSystem *fs ) :
          FileSystemOperation<SendInfoImpl, state, Arg<std::string>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      SendInfoImpl( FileSystem &fs ) :
          FileSystemOperation<SendInfoImpl, state, Arg<std::string>>( &fs )
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
      SendInfoImpl( SendInfoImpl<from> && sendinfo ) :
          FileSystemOperation<SendInfoImpl, state, Arg<std::string>>(
              std::move( sendinfo ) )
      {
      }

      //------------------------------------------------------------------------
      //! Info Argument Descriptors
      //------------------------------------------------------------------------
      struct InfoArg
      {
          static const int index = 0;
          static const std::string key;
          typedef std::string type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<SendInfoImpl, state, Arg<std::string>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      SendInfoImpl<Handled> operator>>(
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
      SendInfoImpl<Handled> operator>>(
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
        return "SendInfo";
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
          std::string &info = Get<InfoArg>( this->args, params, bucket );
          return this->filesystem->SendInfo( info, this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef SendInfoImpl<Bare> SendInfo;
  template<State state> const std::string SendInfoImpl<state>::InfoArg::key = "info";

  //----------------------------------------------------------------------------
  //! Prepare operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class PrepareImpl: public FileSystemOperation<PrepareImpl, state,
      Arg<std::vector<std::string>>, Arg<PrepareFlags::Flags>, Arg<uint8_t>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      PrepareImpl( FileSystem *fs ) :
          FileSystemOperation<PrepareImpl, state, Arg<std::vector<std::string>>,
              Arg<PrepareFlags::Flags>, Arg<uint8_t>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      PrepareImpl( FileSystem &fs ) :
          FileSystemOperation<PrepareImpl, state, Arg<std::vector<std::string>>,
              Arg<PrepareFlags::Flags>, Arg<uint8_t>>( &fs )
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
      PrepareImpl( PrepareImpl<from> && prep ) :
          FileSystemOperation<PrepareImpl, state, Arg<std::vector<std::string>>,
              Arg<PrepareFlags::Flags>, Arg<uint8_t>>( std::move( prep ) )
      {
      }

      //------------------------------------------------------------------------
      //! FileList Argument Descriptors
      //------------------------------------------------------------------------
      struct FileListArg
      {
          static const int index = 0;
          static const std::string key;
          typedef std::vector<std::string> type;
      };

      //------------------------------------------------------------------------
      //! Flags Argument Descriptors
      //------------------------------------------------------------------------
      struct FlagsArg
      {
          static const int index = 1;
          static const std::string key;
          typedef PrepareFlags::Flags type;
      };

      //------------------------------------------------------------------------
      //! Priority Argument Descriptors
      //------------------------------------------------------------------------
      struct PriorityArg
      {
          static const int index = 2;
          static const std::string key;
          typedef uint8_t type;
      };

      //------------------------------------------------------------------------
      //! make visible the >> inherited from ConcreteOperation
      //------------------------------------------------------------------------
      using ConcreteOperation<PrepareImpl, state, Arg<std::vector<std::string>>,
          Arg<PrepareFlags::Flags>, Arg<uint8_t>>::operator>>;

      //------------------------------------------------------------------------
      //! Adds handler for the operation
      //!
      //! @param handleFunction : callback (function, functor or lambda)
      //------------------------------------------------------------------------
      PrepareImpl<Handled> operator>>(
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
      PrepareImpl<Handled> operator>>(
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
        return "Prepare";
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
          std::vector<std::string> &fileList = Get<FileListArg>( this->args,
              params, bucket );
          PrepareFlags::Flags &flags = Get<FlagsArg>( this->args, params,
              bucket );
          uint8_t &priority = Get<PriorityArg>( this->args, params, bucket );
          return this->filesystem->Prepare( fileList, flags, priority,
              this->handler.get() );
        }
        catch( const std::logic_error& err )
        {
          return this->HandleError( err );
        }
      }
  };
  typedef PrepareImpl<Bare> Prepare;
  template<State state> const std::string PrepareImpl<state>::FileListArg::key = "fileList";
  template<State state> const std::string PrepareImpl<state>::FlagsArg::key    = "flags";
  template<State state> const std::string PrepareImpl<state>::PriorityArg::key = "priority";

}

#endif // __XRD_CL_FILE_SYSTEM_OPERATIONS_HH__
