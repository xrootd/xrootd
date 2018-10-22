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
  template<template<State> class Derived, State state, typename Response, typename ... Arguments>
  class FileOperation: public ConcreteOperation<Derived, state, Response, Arguments...>
  {

      template<template<State> class, State, typename, typename ...> friend class FileOperation;

    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param f : file on which the operation will be performed
      //------------------------------------------------------------------------
      FileOperation( File *f ): file(f)
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
      FileOperation( FileOperation<Derived, from, Response, Arguments...> && op ) :
        ConcreteOperation<Derived, state, Response, Arguments...>( std::move( op ) ), file( op.file )
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
  class OpenImpl: public FileOperation<OpenImpl, state, Resp<void>, Arg<std::string>,
      Arg<OpenFlags::Flags>, Arg<Access::Mode>>
  {
      //------------------------------------------------------------------------
      //! Helper for extending the operator>> capabilities.
      //!
      //! In addition to standard overloads for std::function adds:
      //! - void( XRootDStatus&, StatInfo& )
      //! - void( XRootDStatus&, StatInfo&, OperationContext& )
      //------------------------------------------------------------------------
      struct ExResp : public Resp<void>
      {
          //--------------------------------------------------------------------
          //! Constructor
          //!
          //! @param file : the underlying XrdCl::File object
          //--------------------------------------------------------------------
          ExResp( XrdCl::File &file ): file( file )
          {
          }

          //--------------------------------------------------------------------
          //! A factory method
          //!
          //! @param func : the function/functor/lambda that should be wrapped
          //! @return     : ResponseHandler instance
          //--------------------------------------------------------------------
          inline ResponseHandler* Create( std::function<void( XRootDStatus&,
              StatInfo& )> func )
          {
            return new ExOpenFuncWrapper( this->file, func );
          }

          //--------------------------------------------------------------------
          //! Make other overloads of Create visible
          //--------------------------------------------------------------------
          using Resp<void>::Create;

          //--------------------------------------------------------------------
          //! The underlying XrdCl::File object
          //--------------------------------------------------------------------
          XrdCl::File &file;
      };

    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      OpenImpl( File *f ) :
          FileOperation<OpenImpl, state, Resp<void>, Arg<std::string>,
              Arg<OpenFlags::Flags>, Arg<Access::Mode>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      OpenImpl( File &f ) :
          FileOperation<OpenImpl, state, Resp<void>, Arg<std::string>,
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
          FileOperation<OpenImpl, state, Resp<void>, Arg<std::string>,
              Arg<OpenFlags::Flags>, Arg<Access::Mode>>( std::move( open ) )
      {

      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { UrlArg, FlagsArg, ModeArg };

      //------------------------------------------------------------------------
      //! Overloaded operator() (in order to provide default value for mode)
      //------------------------------------------------------------------------
      OpenImpl<Configured> operator()( Arg<std::string> url,
          Arg<OpenFlags::Flags> flags, Arg<Access::Mode> mode = Access::None )
      {
        return this->ConcreteOperation<OpenImpl, state, Resp<void>, Arg<std::string>,
            Arg<OpenFlags::Flags>, Arg<Access::Mode>>::
            operator ()( std::move( url ), std::move( flags ), std::move( mode ) );
      }

      //------------------------------------------------------------------------
      //! Overload of operator>> defined in ConcreteOperation, we're adding
      //! additional capabilities by using ExResp factory (@see ExResp).
      //!
      //! @param func : function/functor/lambda
      //------------------------------------------------------------------------
      template<typename Hdlr>
      OpenImpl<Handled> operator>>( Hdlr hdlr )
      {
        // check if the resulting handler should be owned by us or by the user,
        // if the user passed us directly a ResponseHandler it's owned by the
        // user, otherwise we need to wrap the argument in a handler and in this
        // case the resulting handler will be owned by us
        constexpr bool own = !IsResponseHandler<Hdlr>::value;
        ExResp factory( *this->file );
        return this->StreamImpl( factory.Create( hdlr ), own );
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
          std::string      url   = std::get<UrlArg>( this->args ).Get();
          OpenFlags::Flags flags = std::get<FlagsArg>( this->args ).Get();
          Access::Mode     mode  = std::get<ModeArg>( this->args ).Get();
          return this->file->Open( url, flags, mode, this->handler.get() );
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
  typedef OpenImpl<Bare> Open;

  //----------------------------------------------------------------------------
  //! Read operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class ReadImpl: public FileOperation<ReadImpl, state, Resp<ChunkInfo>,
      Arg<uint64_t>, Arg<uint32_t>, Arg<void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      ReadImpl( File *f ) :
          FileOperation<ReadImpl, state, Resp<ChunkInfo>, Arg<uint64_t>,
              Arg<uint32_t>, Arg<void*>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      ReadImpl( File &f ) :
          FileOperation<ReadImpl, state, Resp<ChunkInfo>, Arg<uint64_t>,
              Arg<uint32_t>, Arg<void*>>( &f )
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
          FileOperation<ReadImpl, state, Resp<ChunkInfo>, Arg<uint64_t>,
              Arg<uint32_t>, Arg<void*>>( std::move( read ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { OffsetArg, SizeArg, BufferArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Read";
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
          uint64_t  offset = std::get<OffsetArg>( this->args ).Get();
          uint32_t  size   = std::get<SizeArg>( this->args ).Get();
          void     *buffer = std::get<BufferArg>( this->args ).Get();
          return this->file->Read( offset, size, buffer, this->handler.get() );
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
  typedef ReadImpl<Bare> Read;

  //----------------------------------------------------------------------------
  //! Close operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state = Bare>
  class CloseImpl: public FileOperation<CloseImpl, state, Resp<void>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      CloseImpl( File *f ) :
          FileOperation<CloseImpl, state, Resp<void>>( f )
      {

      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      CloseImpl( File &f ) :
          FileOperation<CloseImpl, state, Resp<void>>( &f )
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
          FileOperation<CloseImpl, state, Resp<void>>( std::move( close ) )
      {

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
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        return this->file->Close( this->handler.get() );
      }
  };
  typedef CloseImpl<Bare> Close;

  //----------------------------------------------------------------------------
  //! Stat operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state = Bare>
  class StatImpl: public FileOperation<StatImpl, state, Resp<StatInfo>, Arg<bool>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      StatImpl( File *f ) :
          FileOperation<StatImpl, state, Resp<StatInfo>, Arg<bool>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      StatImpl( File &f ) :
          FileOperation<StatImpl, state, Resp<StatInfo>, Arg<bool>>( &f )
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
          FileOperation<StatImpl, state, Resp<StatInfo>, Arg<bool>>( std::move( stat ) )
      {

      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { ForceArg };

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
          bool force = std::get<ForceArg>( this->args ).Get();
          return this->file->Stat( force, this->handler.get() );
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
  class WriteImpl: public FileOperation<WriteImpl, state, Resp<void>, Arg<uint64_t>,
      Arg<uint32_t>, Arg<void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      WriteImpl( File *f ) :
          FileOperation<WriteImpl, state, Resp<void>, Arg<uint64_t>, Arg<uint32_t>,
              Arg<void*>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      WriteImpl( File &f ) :
          FileOperation<WriteImpl, state, Resp<void>, Arg<uint64_t>, Arg<uint32_t>,
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
          FileOperation<WriteImpl, state, Resp<void>, Arg<uint64_t>, Arg<uint32_t>,
              Arg<void*>>( std::move( write ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { OffsetArg, SizeArg, BufferArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Write";
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
          uint64_t  offset = std::get<OffsetArg>( this->args ).Get();
          uint32_t  size   = std::get<SizeArg>( this->args ).Get();
          void     *buffer = std::get<BufferArg>( this->args ).Get();
          return this->file->Write( offset, size, buffer, this->handler.get() );
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
  typedef WriteImpl<Bare> Write;

  //----------------------------------------------------------------------------
  //! Sync operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state = Bare>
  class SyncImpl: public FileOperation<SyncImpl, state, Resp<void>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      SyncImpl( File *f ) :
          FileOperation<SyncImpl, state, Resp<void>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      SyncImpl( File &f ) :
          FileOperation<SyncImpl, state, Resp<void>>( &f )
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
          FileOperation<SyncImpl, state, Resp<void>>( std::move( sync ) )
      {
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
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        return this->file->Sync( this->handler.get() );
      }
  };
  typedef SyncImpl<Bare> Sync;

  //----------------------------------------------------------------------------
  //! Truncate operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class TruncateImpl: public FileOperation<TruncateImpl, state, Resp<void>, Arg<uint64_t>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      TruncateImpl( File *f ) :
          FileOperation<TruncateImpl, state, Resp<void>, Arg<uint64_t>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      TruncateImpl( File &f ) :
          FileOperation<TruncateImpl, state, Resp<void>, Arg<uint64_t>>( &f )
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
          FileOperation<TruncateImpl, state, Resp<void>, Arg<uint64_t>>(
              std::move( trunc ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { SizeArg };

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
          uint64_t size = std::get<SizeArg>( this->args ).Get();
          return this->file->Truncate( size, this->handler.get() );
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
      Resp<VectorReadInfo>, Arg<ChunkList>, Arg<void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      VectorReadImpl( File *f ) :
          FileOperation<VectorReadImpl, state, Resp<VectorReadInfo>, Arg<ChunkList>,
          Arg<void*>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      VectorReadImpl( File &f ) :
          FileOperation<VectorReadImpl, state, Resp<VectorReadInfo>, Arg<ChunkList>,
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
      VectorReadImpl( VectorReadImpl<from> && vread ) :
          FileOperation<VectorReadImpl, state, Resp<VectorReadInfo>, Arg<ChunkList>,
              Arg<void*>>( std::move( vread ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { ChunksArg, BufferArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "VectorRead";
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
          ChunkList chunks( std::get<ChunksArg>( this->args ).Get() );
          void *buffer = std::get<BufferArg>( this->args ).Get();
          return this->file->VectorRead( chunks, buffer, this->handler.get() );
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
  typedef VectorReadImpl<Bare> VectorRead;

  //----------------------------------------------------------------------------
  //! VectorWrite operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class VectorWriteImpl: public FileOperation<VectorWriteImpl, state, Resp<void>,
      Arg<ChunkList>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      VectorWriteImpl( File *f ) :
          FileOperation<VectorWriteImpl, state, Resp<void>, Arg<ChunkList>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      VectorWriteImpl( File &f ) :
          FileOperation<VectorWriteImpl, state, Resp<void>, Arg<ChunkList>>( &f )
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
          FileOperation<VectorWriteImpl, state, Resp<void>, Arg<ChunkList>>(
              std::move( vwrite ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { ChunksArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "VectorWrite";
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
          const ChunkList chunks( std::get<ChunksArg>( this->args ).Get() );
          return this->file->VectorWrite( chunks, this->handler.get() );
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
  typedef VectorWriteImpl<Bare> VectorWrite;

  //----------------------------------------------------------------------------
  //! WriteV operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class WriteVImpl: public FileOperation<WriteVImpl, state, Resp<void>, Arg<uint64_t>,
      Arg<struct iovec*>, Arg<int>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      WriteVImpl( File *f ) :
          FileOperation<WriteVImpl, state, Resp<void>, Arg<uint64_t>, Arg<struct iovec*>,
              Arg<int>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      WriteVImpl( File &f ) :
          FileOperation<WriteVImpl, state, Resp<void>, Arg<uint64_t>, Arg<struct iovec*>,
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
          FileOperation<WriteVImpl, state, Resp<void>, Arg<uint64_t>, Arg<struct iovec*>,
              Arg<int>>( std::move( writev ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { OffsetArg, IovArg, IovcntArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "WriteV";
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
          uint64_t            offset = std::get<OffsetArg>( this->args ).Get();
          const struct iovec *iov    = std::get<IovArg>( this->args ).Get();
          int                 iovcnt = std::get<IovcntArg>( this->args ).Get();
          return this->file->WriteV( offset, iov, iovcnt, this->handler.get() );
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
  typedef WriteVImpl<Bare> WriteV;

  //----------------------------------------------------------------------------
  //! Fcntl operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state>
  class FcntlImpl: public FileOperation<FcntlImpl, state, Resp<Buffer>, Arg<Buffer>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      FcntlImpl( File *f ) :
          FileOperation<FcntlImpl, state, Resp<Buffer>, Arg<Buffer>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      FcntlImpl( File &f ) :
          FileOperation<FcntlImpl, state, Resp<Buffer>, Arg<Buffer>>( &f )
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
          FileOperation<FcntlImpl, state, Resp<Buffer>, Arg<Buffer>>( std::move( fcntl ) )
      {
      }

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { BufferArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Fcntl";
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
          Buffer arg( std::get<BufferArg>( this->args ).Get() );
          return this->file->Fcntl( arg, this->handler.get() );
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
  typedef FcntlImpl<Bare> Fcntl;

  //----------------------------------------------------------------------------
  //! Visa operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<State state = Bare>
  class VisaImpl: public FileOperation<VisaImpl, state, Resp<Buffer>>
  {
    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      VisaImpl( File *f ) :
          FileOperation<VisaImpl, state, Resp<Buffer>>( f )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      VisaImpl( File &f ) :
          FileOperation<VisaImpl, state, Resp<Buffer>>( &f )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param op : thBe object that is being converted
      //------------------------------------------------------------------------
      template<State from>
      VisaImpl( VisaImpl<from> && visa ) :
          FileOperation<VisaImpl, state, Resp<Buffer>>( std::move( visa ) )
      {
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
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl()
      {
        return this->file->Visa( this->handler.get() );
      }
  };
  typedef VisaImpl<Bare> Visa;
}

#endif // __XRD_CL_FILE_OPERATIONS_HH__

