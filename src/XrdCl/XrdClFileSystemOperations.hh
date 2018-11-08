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
  //! @arg HasHndl : true if operation has a handler, false otherwise
  //! @arg Args    : operation arguments
  //----------------------------------------------------------------------------
  template<template<bool> class Derived, bool HasHndl, typename Response, typename ... Args>
  class FileSystemOperation: public ConcreteOperation<Derived, HasHndl, Response, Args...>
  {

      template<template<bool> class, bool, typename, typename ...> friend class FileSystemOperation;

    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param fs   : file system on which the operation will be performed
      //! @param args : file operation arguments
      //------------------------------------------------------------------------
      FileSystemOperation( FileSystem *fs, Args... args): ConcreteOperation<Derived,
          false, Response, Args...>( std::move( args )... ), filesystem(fs)
      {
      }

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param fs   : file system on which the operation will be performed
      //! @param args : file operation arguments
      //------------------------------------------------------------------------
      FileSystemOperation( FileSystem &fs, Args... args): FileSystemOperation( &fs, std::move( args )... )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : the object that is being converted
      //------------------------------------------------------------------------
      template<bool from>
      FileSystemOperation( FileSystemOperation<Derived, from, Response, Args...> && op ):
        ConcreteOperation<Derived, HasHndl, Response, Args...>( std::move( op ) ), filesystem( op.filesystem )
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
  template<bool HasHndl>
  class LocateImpl: public FileSystemOperation<LocateImpl, HasHndl, Resp<LocationInfo>,
      Arg<std::string>, Arg<OpenFlags::Flags>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<LocateImpl, HasHndl, Resp<LocationInfo>, Arg<std::string>,
                                Arg<OpenFlags::Flags>>::FileSystemOperation;

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
  typedef LocateImpl<false> Locate;

  //----------------------------------------------------------------------------
  //! DeepLocate operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class DeepLocateImpl: public FileSystemOperation<DeepLocateImpl, HasHndl,
      Resp<LocationInfo>, Arg<std::string>, Arg<OpenFlags::Flags>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<DeepLocateImpl, HasHndl, Resp<LocationInfo>, Arg<std::string>,
                                Arg<OpenFlags::Flags>>::FileSystemOperation;

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
  typedef DeepLocateImpl<false> DeepLocate;

  //----------------------------------------------------------------------------
  //! Mv operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class MvImpl: public FileSystemOperation<MvImpl, HasHndl, Resp<void>, Arg<std::string>,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<MvImpl, HasHndl, Resp<void>, Arg<std::string>,
                                Arg<std::string>>::FileSystemOperation;

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
  typedef MvImpl<false> Mv;

  //----------------------------------------------------------------------------
  //! Query operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class QueryImpl: public FileSystemOperation<QueryImpl, HasHndl, Resp<Buffer>,
      Arg<QueryCode::Code>, Arg<Buffer>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<QueryImpl, HasHndl, Resp<Buffer>, Arg<QueryCode::Code>,
                                Arg<Buffer>>::FileSystemOperation;

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
  typedef QueryImpl<false> Query;

  //----------------------------------------------------------------------------
  //! Truncate operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class TruncateFsImpl: public FileSystemOperation<TruncateFsImpl, HasHndl, Resp<void>,
      Arg<std::string>, Arg<uint64_t>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<TruncateFsImpl, HasHndl, Resp<void>, Arg<std::string>,
                                Arg<uint64_t>>::FileSystemOperation;

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

  TruncateFsImpl<false> Truncate( FileSystem *fs, Arg<std::string> path, Arg<uint64_t> size )
  {
    return TruncateFsImpl<false>( fs, std::move( path ), std::move( size ) );
  }

  TruncateFsImpl<false> Truncate( FileSystem &fs, Arg<std::string> path, Arg<uint64_t> size )
  {
    return TruncateFsImpl<false>( fs, std::move( path ), std::move( size ) );
  }

  //----------------------------------------------------------------------------
  //! Rm operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class RmImpl: public FileSystemOperation<RmImpl, HasHndl, Resp<void>, Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<RmImpl, HasHndl, Resp<void>, Arg<std::string>>::FileSystemOperation;

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
  typedef RmImpl<false> Rm;

  //----------------------------------------------------------------------------
  //! MkDir operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class MkDirImpl: public FileSystemOperation<MkDirImpl, HasHndl, Resp<void>,
      Arg<std::string>, Arg<MkDirFlags::Flags>, Arg<Access::Mode>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<MkDirImpl, HasHndl, Resp<void>, Arg<std::string>,
                                Arg<MkDirFlags::Flags>, Arg<Access::Mode>>::FileSystemOperation;

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
  typedef MkDirImpl<false> MkDir;

  //----------------------------------------------------------------------------
  //! RmDir operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class RmDirImpl: public FileSystemOperation<RmDirImpl, HasHndl, Resp<void>,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<RmDirImpl, HasHndl, Resp<void>, Arg<std::string>>::FileSystemOperation;

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
  typedef RmDirImpl<false> RmDir;

  //----------------------------------------------------------------------------
  //! ChMod operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class ChModImpl: public FileSystemOperation<ChModImpl, HasHndl, Resp<void>,
      Arg<std::string>, Arg<Access::Mode>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<ChModImpl, HasHndl, Resp<void>, Arg<std::string>,
                                Arg<Access::Mode>>::FileSystemOperation;

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
  typedef ChModImpl<false> ChMod;

  //----------------------------------------------------------------------------
  //! Ping operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class PingImpl: public FileSystemOperation<PingImpl, HasHndl, Resp<void>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<PingImpl, HasHndl, Resp<void>>::FileSystemOperation;

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
  typedef PingImpl<false> Ping;

  //----------------------------------------------------------------------------
  //! Stat operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class StatFsImpl: public FileSystemOperation<StatFsImpl, HasHndl, Resp<StatInfo>,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<StatFsImpl, HasHndl, Resp<StatInfo>,
                                Arg<std::string>>::FileSystemOperation;

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

  StatFsImpl<false> Stat( FileSystem *fs, Arg<std::string> path )
  {
    return StatFsImpl<false>( fs, std::move( path ) );
  }

  StatFsImpl<false> Stat( FileSystem &fs, Arg<std::string> path )
  {
    return StatFsImpl<false>( fs, std::move( path ) );
  }

  //----------------------------------------------------------------------------
  //! StatVS operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class StatVFSImpl: public FileSystemOperation<StatVFSImpl, HasHndl,
      Resp<StatInfoVFS>, Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<StatVFSImpl, HasHndl, Resp<StatInfoVFS>,
                                Arg<std::string>>::FileSystemOperation;

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
  typedef StatVFSImpl<false> StatVFS;

  //----------------------------------------------------------------------------
  //! Protocol operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class ProtocolImpl: public FileSystemOperation<ProtocolImpl, HasHndl,
      Resp<ProtocolInfo>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<ProtocolImpl, HasHndl, Resp<ProtocolInfo>>::FileSystemOperation;

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
  typedef ProtocolImpl<false> Protocol;

  //----------------------------------------------------------------------------
  //! DirList operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class DirListImpl: public FileSystemOperation<DirListImpl, HasHndl, Resp<DirectoryList>,
      Arg<std::string>, Arg<DirListFlags::Flags>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<DirListImpl, HasHndl, Resp<DirectoryList>, Arg<std::string>,
          Arg<DirListFlags::Flags>>::FileSystemOperation;

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
  typedef DirListImpl<false> DirList;

  //----------------------------------------------------------------------------
  //! SendInfo operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class SendInfoImpl: public FileSystemOperation<SendInfoImpl, HasHndl, Resp<Buffer>,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<SendInfoImpl, HasHndl, Resp<Buffer>,
                                Arg<std::string>>::FileSystemOperation;

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
  typedef SendInfoImpl<false> SendInfo;

  //----------------------------------------------------------------------------
  //! Prepare operation (@see FileSystemOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class PrepareImpl: public FileSystemOperation<PrepareImpl, HasHndl, Resp<Buffer>,
      Arg<std::vector<std::string>>, Arg<PrepareFlags::Flags>, Arg<uint8_t>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileSystemOperation (@see FileSystemOperation)
      //------------------------------------------------------------------------
      using FileSystemOperation<PrepareImpl, HasHndl, Resp<Buffer>, Arg<std::vector<std::string>>,
                                Arg<PrepareFlags::Flags>, Arg<uint8_t>>::FileSystemOperation;

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
  typedef PrepareImpl<false> Prepare;
}

#endif // __XRD_CL_FILE_SYSTEM_OPERATIONS_HH__
