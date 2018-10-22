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
  template<template<State> class Derived, State state, typename Response, typename ... Args>
  class FileSystemOperation: public ConcreteOperation<Derived, state, Response, Args...>
  {

      template<template<State> class, State, typename, typename ...> friend class FileSystemOperation;

    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param fs : file system on which the operation will be performed
      //------------------------------------------------------------------------
      explicit FileSystemOperation( FileSystem *fs ): filesystem(fs)
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
      FileSystemOperation( FileSystemOperation<Derived, from, Response, Args...> && op ):
        ConcreteOperation<Derived, state, Response, Args...>( std::move( op ) ), filesystem( op.filesystem )
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
  class LocateImpl: public FileSystemOperation<LocateImpl, state, Resp<LocationInfo>,
      Arg<std::string>, Arg<OpenFlags::Flags>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      LocateImpl( FileSystem *fs ) :
          FileSystemOperation<LocateImpl, state, Resp<LocationInfo>, Arg<std::string>,
              Arg<OpenFlags::Flags>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      LocateImpl( FileSystem &fs ) :
          FileSystemOperation<LocateImpl, state, Resp<LocationInfo>, Arg<std::string>,
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
          FileSystemOperation<LocateImpl, state, Resp<LocationInfo>, Arg<std::string>,
              Arg<OpenFlags::Flags>>( std::move( locate ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { PathArg, FlagsArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Locate";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          std::string      path  = std::get<PathArg>( this->args ).Get();
          OpenFlags::Flags flags = std::get<FlagsArg>( this->args ).Get();
          return this->filesystem->Locate( path, flags, this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };
  typedef LocateImpl<Bare> Locate;

  //----------------------------------------------------------------------------
  //! DeepLocate operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class DeepLocateImpl: public FileSystemOperation<DeepLocateImpl, state,
      Resp<LocationInfo>, Arg<std::string>, Arg<OpenFlags::Flags>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      DeepLocateImpl( FileSystem *fs ) :
          FileSystemOperation<DeepLocateImpl, state, Resp<LocationInfo>,
              Arg<std::string>, Arg<OpenFlags::Flags>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      DeepLocateImpl( FileSystem &fs ) :
          FileSystemOperation<DeepLocateImpl, state, Resp<LocationInfo>,
              Arg<std::string>, Arg<OpenFlags::Flags>>( &fs )
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
          FileSystemOperation<DeepLocateImpl, state, Resp<LocationInfo>,
              Arg<std::string>, Arg<OpenFlags::Flags>>( std::move( locate ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { PathArg, FlagsArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "DeepLocate";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          std::string      path  = std::get<PathArg>( this->args ).Get();
          OpenFlags::Flags flags = std::get<FlagsArg>( this->args ).Get();
          return this->filesystem->DeepLocate( path, flags, this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };
  typedef DeepLocateImpl<Bare> DeepLocate;

  //----------------------------------------------------------------------------
  //! Mv operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class MvImpl: public FileSystemOperation<MvImpl, state, Resp<void>, Arg<std::string>,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      MvImpl( FileSystem *fs ) :
          FileSystemOperation<MvImpl, state, Resp<void>, Arg<std::string>, Arg<std::string>>(
              fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      MvImpl( FileSystem &fs ) :
          FileSystemOperation<MvImpl, state, Resp<void>, Arg<std::string>, Arg<std::string>>(
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
          FileSystemOperation<MvImpl, state, Resp<void>, Arg<std::string>, Arg<std::string>>(
              std::move( mv ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { SourceArg, DestArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Mv";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          std::string source = std::get<SourceArg>( this->args ).Get();
          std::string dest   = std::get<DestArg>( this->args ).Get();
          return this->filesystem->Mv( source, dest, this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };
  typedef MvImpl<Bare> Mv;

  //----------------------------------------------------------------------------
  //! Query operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class QueryImpl: public FileSystemOperation<QueryImpl, state, Resp<Buffer>,
      Arg<QueryCode::Code>, Arg<Buffer>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      QueryImpl( FileSystem *fs ) :
          FileSystemOperation<QueryImpl, state, Resp<Buffer>, Arg<QueryCode::Code>,
              Arg<Buffer>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      QueryImpl( FileSystem &fs ) :
          FileSystemOperation<QueryImpl, state, Resp<Buffer>, Arg<QueryCode::Code>,
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
          FileSystemOperation<QueryImpl, state, Resp<Buffer>, Arg<QueryCode::Code>,
              Arg<Buffer>>( std::move( query ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { QueryCodeArg, BufferArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Query";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          QueryCode::Code queryCode = std::get<QueryCodeArg>( this->args ).Get();
          const Buffer    buffer( std::get<BufferArg>( this->args ).Get() );
          return this->filesystem->Query( queryCode, buffer, this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };
  typedef QueryImpl<Bare> Query;

  //----------------------------------------------------------------------------
  //! Truncate operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class TruncateFsImpl: public FileSystemOperation<TruncateFsImpl, state, Resp<void>,
      Arg<std::string>, Arg<uint64_t>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      TruncateFsImpl( FileSystem *fs ) :
          FileSystemOperation<TruncateFsImpl, state, Resp<void>, Arg<std::string>,
              Arg<uint64_t>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      TruncateFsImpl( FileSystem &fs ) :
          FileSystemOperation<TruncateFsImpl, state, Resp<void>, Arg<std::string>,
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
          FileSystemOperation<TruncateFsImpl, state, Resp<void>, Arg<std::string>,
              Arg<uint64_t>>( std::move( trunc ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { PathArg, SizeArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Truncate";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          std::string path = std::get<PathArg>( this->args ).Get();
          uint64_t    size = std::get<SizeArg>( this->args ).Get();
          return this->filesystem->Truncate( path, size, this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };

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
  class RmImpl: public FileSystemOperation<RmImpl, state, Resp<void>, Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      RmImpl( FileSystem *fs ) :
          FileSystemOperation<RmImpl, state, Resp<void>, Arg<std::string>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      RmImpl( FileSystem &fs ) :
          FileSystemOperation<RmImpl, state, Resp<void>, Arg<std::string>>( &fs )
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
          FileSystemOperation<RmImpl, state, Resp<void>, Arg<std::string>>(
              std::move( rm ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { PathArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Rm";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          std::string path = std::get<PathArg>( this->args ).Get();
          return this->filesystem->Rm( path, this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };
  typedef RmImpl<Bare> Rm;

  //----------------------------------------------------------------------------
  //! MkDir operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class MkDirImpl: public FileSystemOperation<MkDirImpl, state, Resp<void>,
      Arg<std::string>, Arg<MkDirFlags::Flags>, Arg<Access::Mode>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      MkDirImpl( FileSystem *fs ) :
          FileSystemOperation<MkDirImpl, state, Resp<void>, Arg<std::string>,
              Arg<MkDirFlags::Flags>, Arg<Access::Mode>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      MkDirImpl( FileSystem &fs ) :
          FileSystemOperation<MkDirImpl, state, Resp<void>, Arg<std::string>,
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
          FileSystemOperation<MkDirImpl, state, Resp<void>, Arg<std::string>,
              Arg<MkDirFlags::Flags>, Arg<Access::Mode>>( std::move( mkdir ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { PathArg, FlagsArg, ModeArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "MkDir";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          std::string       path  = std::get<PathArg>( this->args ).Get();
          MkDirFlags::Flags flags = std::get<FlagsArg>( this->args ).Get();
          Access::Mode      mode  = std::get<ModeArg>( this->args ).Get();
          return this->filesystem->MkDir( path, flags, mode, this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };
  typedef MkDirImpl<Bare> MkDir;

  //----------------------------------------------------------------------------
  //! RmDir operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class RmDirImpl: public FileSystemOperation<RmDirImpl, state, Resp<void>,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      RmDirImpl( FileSystem *fs ) :
          FileSystemOperation<RmDirImpl, state, Resp<void>, Arg<std::string>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      RmDirImpl( FileSystem &fs ) :
          FileSystemOperation<RmDirImpl, state, Resp<void>, Arg<std::string>>( &fs )
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
          FileSystemOperation<RmDirImpl, state, Resp<void>, Arg<std::string>>(
              std::move( rmdir ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { PathArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "RmDir";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          std::string path = std::get<PathArg>( this->args ).Get();
          return this->filesystem->RmDir( path, this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };
  typedef RmDirImpl<Bare> RmDir;

  //----------------------------------------------------------------------------
  //! ChMod operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class ChModImpl: public FileSystemOperation<ChModImpl, state, Resp<void>,
      Arg<std::string>, Arg<Access::Mode>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      ChModImpl( FileSystem *fs ) :
          FileSystemOperation<ChModImpl, state, Resp<void>, Arg<std::string>,
              Arg<Access::Mode>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      ChModImpl( FileSystem &fs ) :
          FileSystemOperation<ChModImpl, state, Resp<void>, Arg<std::string>,
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
          FileSystemOperation<ChModImpl, state, Resp<void>, Arg<std::string>,
              Arg<Access::Mode>>( std::move( chmod ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { PathArg, ModeArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "ChMod";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          std::string  path = std::get<PathArg>( this->args ).Get();
          Access::Mode mode = std::get<ModeArg>( this->args ).Get();
          return this->filesystem->ChMod( path, mode, this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };
  typedef ChModImpl<Bare> ChMod;

  //----------------------------------------------------------------------------
  //! Ping operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class PingImpl: public FileSystemOperation<PingImpl, state, Resp<void>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      PingImpl( FileSystem *fs ) :
          FileSystemOperation<PingImpl, state, Resp<void>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      PingImpl( FileSystem &fs ) :
          FileSystemOperation<PingImpl, state, Resp<void>>( &fs )
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
          FileSystemOperation<PingImpl, state, Resp<void>>( std::move( ping ) )
      {
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
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        return this->filesystem->Ping( this->handler.get() );
      }
  };
  typedef PingImpl<Bare> Ping;

  //----------------------------------------------------------------------------
  //! Stat operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class StatFsImpl: public FileSystemOperation<StatFsImpl, state, Resp<StatInfo>,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      StatFsImpl( FileSystem *fs ) :
          FileSystemOperation<StatFsImpl, state, Resp<StatInfo>,
              Arg<std::string>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      StatFsImpl( FileSystem &fs ) :
          FileSystemOperation<StatFsImpl, state, Resp<StatInfo>,
              Arg<std::string>>( &fs )
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
          FileSystemOperation<StatFsImpl, state, Resp<StatInfo>, Arg<std::string>>(
              std::move( statfs ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { PathArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Stat";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          std::string path = std::get<PathArg>( this->args ).Get();
          return this->filesystem->RmDir( path, this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };

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
      Resp<StatInfoVFS>, Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      StatVFSImpl( FileSystem *fs ) : FileSystemOperation<StatVFSImpl, state,
          Resp<StatInfoVFS>, Arg<std::string>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      StatVFSImpl( FileSystem &fs ) : FileSystemOperation<StatVFSImpl, state,
          Resp<StatInfoVFS>, Arg<std::string>>( &fs )
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
      StatVFSImpl( StatVFSImpl<state> && statvfs ) : FileSystemOperation<StatVFSImpl,
          state, Resp<StatInfoVFS>, Arg<std::string>>( std::move( statvfs ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { PathArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "StatVFS";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          std::string path = std::get<PathArg>( this->args ).Get();
          return this->filesystem->StatVFS( path, this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };
  typedef StatVFSImpl<Bare> StatVFS;

  //----------------------------------------------------------------------------
  //! Protocol operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class ProtocolImpl: public FileSystemOperation<ProtocolImpl, state,
      Resp<ProtocolInfo>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      ProtocolImpl( FileSystem *fs ) :
          FileSystemOperation<ProtocolImpl, state, Resp<ProtocolInfo>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      ProtocolImpl( FileSystem &fs ) :
          FileSystemOperation<ProtocolImpl, state, Resp<ProtocolInfo>>( &fs )
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
      ProtocolImpl( ProtocolImpl<from> && prot ) : FileSystemOperation<ProtocolImpl,
          state, Resp<ProtocolInfo>>( std::move( prot ) )
      {
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
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        return this->filesystem->Protocol( this->handler.get() );
      }
  };
  typedef ProtocolImpl<Bare> Protocol;

  //----------------------------------------------------------------------------
  //! DirList operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class DirListImpl: public FileSystemOperation<DirListImpl, state, Resp<DirectoryList>,
      Arg<std::string>, Arg<DirListFlags::Flags>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      DirListImpl( FileSystem *fs ) : FileSystemOperation<DirListImpl, state,
          Resp<DirectoryList>, Arg<std::string>, Arg<DirListFlags::Flags>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      DirListImpl( FileSystem &fs ) : FileSystemOperation<DirListImpl, state,
          Resp<DirectoryList>, Arg<std::string>, Arg<DirListFlags::Flags>>( &fs )
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
      DirListImpl( DirListImpl<from> && dirls ) : FileSystemOperation<DirListImpl,
          state, Resp<DirectoryList>, Arg<std::string>,
          Arg<DirListFlags::Flags>>( std::move( dirls ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { PathArg, FlagsArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "DirList";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          std::string         path  = std::get<PathArg>( this->args ).Get();
          DirListFlags::Flags flags = std::get<FlagsArg>( this->args ).Get();
          return this->filesystem->DirList( path, flags, this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };
  typedef DirListImpl<Bare> DirList;

  //----------------------------------------------------------------------------
  //! SendInfo operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class SendInfoImpl: public FileSystemOperation<SendInfoImpl, state, Resp<Buffer>,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      SendInfoImpl( FileSystem *fs ) : FileSystemOperation<SendInfoImpl, state,
          Resp<Buffer>, Arg<std::string>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      SendInfoImpl( FileSystem &fs ) : FileSystemOperation<SendInfoImpl, state,
          Resp<Buffer>, Arg<std::string>>( &fs )
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
      SendInfoImpl( SendInfoImpl<from> && sendinfo ) : FileSystemOperation<SendInfoImpl,
          state, Resp<Buffer>, Arg<std::string>>( std::move( sendinfo ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { InfoArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "SendInfo";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          std::string info = std::get<InfoArg>( this->args ).Get();
          return this->filesystem->SendInfo( info, this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };
  typedef SendInfoImpl<Bare> SendInfo;

  //----------------------------------------------------------------------------
  //! Prepare operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class PrepareImpl: public FileSystemOperation<PrepareImpl, state, Resp<Buffer>,
      Arg<std::vector<std::string>>, Arg<PrepareFlags::Flags>, Arg<uint8_t>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      PrepareImpl( FileSystem *fs ) : FileSystemOperation<PrepareImpl, state,
          Resp<Buffer>, Arg<std::vector<std::string>>, Arg<PrepareFlags::Flags>,
          Arg<uint8_t>>( fs )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileSystemOperation)
      //------------------------------------------------------------------------
      PrepareImpl( FileSystem &fs ) : FileSystemOperation<PrepareImpl, state,
          Resp<Buffer>, Arg<std::vector<std::string>>, Arg<PrepareFlags::Flags>,
          Arg<uint8_t>>( &fs )
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
      PrepareImpl( PrepareImpl<from> && prep ) : FileSystemOperation<PrepareImpl,
          state, Resp<Buffer>, Arg<std::vector<std::string>>,
          Arg<PrepareFlags::Flags>, Arg<uint8_t>>( std::move( prep ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { FileListArg, FlagsArg, PriorityArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Prepare";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        try
        {
          std::vector<std::string> fileList = std::get<FileListArg>( this->args ).Get();
          PrepareFlags::Flags      flags    = std::get<FlagsArg>( this->args ).Get();
          uint8_t                  priority = std::get<PriorityArg>( this->args ).Get();
          return this->filesystem->Prepare( fileList, flags, priority,
              this->handler.get() );
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }
      }
  };
  typedef PrepareImpl<Bare> Prepare;
}

#endif // __XRD_CL_FILE_SYSTEM_OPERATIONS_HH__
