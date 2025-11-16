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
#include "XrdCl/XrdClCtx.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Base class for all file related operations
  //!
  //! @arg Derived : the class that derives from this template (CRTP)
  //! @arg HasHndl : true if operation has a handler, false otherwise
  //! @arg Args    : operation arguments
  //----------------------------------------------------------------------------
  template<template<bool> class Derived, bool HasHndl, typename Response, typename ... Arguments>
  class FileOperation: public ConcreteOperation<Derived, HasHndl, Response, Arguments...>
  {

      template<template<bool> class, bool, typename, typename ...> friend class FileOperation;

    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param f    : file on which the operation will be performed
      //! @param args : file operation arguments
      //------------------------------------------------------------------------
      FileOperation( Ctx<File> f, Arguments... args): ConcreteOperation<Derived, false, Response, Arguments...>( std::move( args )... ), file( std::move( f ) )
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
      FileOperation( FileOperation<Derived, from, Response, Arguments...> && op ) :
        ConcreteOperation<Derived, HasHndl, Response, Arguments...>( std::move( op ) ), file( op.file )
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
      Ctx<File> file;
  };

  //----------------------------------------------------------------------------
  //! Open operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class OpenImpl: public FileOperation<OpenImpl, HasHndl, Resp<void>, Arg<std::string>,
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
          ExResp( const Ctx<File> &file ): file( file )
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
          Ctx<File> file;
      };

    public:

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      OpenImpl( Ctx<File> f, Arg<std::string> url, Arg<OpenFlags::Flags> flags,
                Arg<Access::Mode> mode = Access::None ) :
          FileOperation<OpenImpl, HasHndl, Resp<void>, Arg<std::string>, Arg<OpenFlags::Flags>,
            Arg<Access::Mode>>( std::move( f ), std::move( url ), std::move( flags ),
            std::move( mode ) )
      {
      }

      //------------------------------------------------------------------------
      //! Move constructor from other states
      //!
      //! @arg from : state from which the object is being converted
      //!
      //! @param open : the object that is being converted
      //------------------------------------------------------------------------
      template<bool from>
      OpenImpl( OpenImpl<from> && open ) :
          FileOperation<OpenImpl, HasHndl, Resp<void>, Arg<std::string>, Arg<OpenFlags::Flags>,
            Arg<Access::Mode>>( std::move( open ) )
      {
      }


      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { UrlArg, FlagsArg, ModeArg };

      //------------------------------------------------------------------------
      //! Overload of operator>> defined in ConcreteOperation, we're adding
      //! additional capabilities by using ExResp factory (@see ExResp).
      //!
      //! @param hdlr : function/functor/lambda
      //------------------------------------------------------------------------
      template<typename Hdlr>
      OpenImpl<true> operator>>( Hdlr &&hdlr )
      {
        ExResp factory( *this->file );
        return this->StreamImpl( factory.Create( hdlr ) );
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
      //! @param pipelineTimeout : pipeline timeout
      //! @return                : status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        const std::string &url     = std::get<UrlArg>( this->args );
        OpenFlags::Flags   flags   = std::get<FlagsArg>( this->args );
        Access::Mode       mode    = std::get<ModeArg>( this->args );
        time_t             timeout = pipelineTimeout < this->timeout ?
                                     pipelineTimeout : this->timeout;
        return this->file->Open( url, flags, mode, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating ReadImpl objects
  //----------------------------------------------------------------------------
  inline OpenImpl<false> Open( Ctx<File> file, Arg<std::string> url, Arg<OpenFlags::Flags> flags,
                               Arg<Access::Mode> mode = Access::None, time_t timeout = 0 )
  {
    return OpenImpl<false>( std::move( file ), std::move( url ), std::move( flags ),
                            std::move( mode ) ).Timeout( timeout );
  }

  //----------------------------------------------------------------------------
  //! Read operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class ReadImpl: public FileOperation<ReadImpl, HasHndl, Resp<ChunkInfo>,
      Arg<uint64_t>, Arg<uint32_t>, Arg<void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<ReadImpl, HasHndl, Resp<ChunkInfo>, Arg<uint64_t>,
                          Arg<uint32_t>, Arg<void*>>::FileOperation;

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
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        uint64_t  offset  = std::get<OffsetArg>( this->args ).Get();
        uint32_t  size    = std::get<SizeArg>( this->args ).Get();
        void     *buffer  = std::get<BufferArg>( this->args ).Get();
        time_t    timeout = pipelineTimeout < this->timeout ?
                            pipelineTimeout : this->timeout;
        return this->file->Read( offset, size, buffer, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating ReadImpl objects
  //----------------------------------------------------------------------------
  inline ReadImpl<false> Read( Ctx<File> file, Arg<uint64_t> offset, Arg<uint32_t> size,
                               Arg<void*> buffer, time_t timeout = 0 )
  {
    return ReadImpl<false>( std::move( file ), std::move( offset ), std::move( size ),
                            std::move( buffer ) ).Timeout( timeout );
  }

  //----------------------------------------------------------------------------
  //! PgRead operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class PgReadImpl: public FileOperation<PgReadImpl, HasHndl, Resp<PageInfo>,
      Arg<uint64_t>, Arg<uint32_t>, Arg<void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<PgReadImpl, HasHndl, Resp<PageInfo>, Arg<uint64_t>,
                          Arg<uint32_t>, Arg<void*>>::FileOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { OffsetArg, SizeArg, BufferArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "PgRead";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        uint64_t  offset  = std::get<OffsetArg>( this->args ).Get();
        uint32_t  size    = std::get<SizeArg>( this->args ).Get();
        void     *buffer  = std::get<BufferArg>( this->args ).Get();
        time_t    timeout = pipelineTimeout < this->timeout ?
                            pipelineTimeout : this->timeout;
        return this->file->PgRead( offset, size, buffer, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating PgReadImpl objects
  //----------------------------------------------------------------------------
  inline PgReadImpl<false> PgRead( Ctx<File> file, Arg<uint64_t> offset,
                                 Arg<uint32_t> size, Arg<void*> buffer,
                                 time_t timeout = 0 )
  {
    return PgReadImpl<false>( std::move( file ), std::move( offset ), std::move( size ),
                            std::move( buffer ) ).Timeout( timeout );
  }

  //----------------------------------------------------------------------------
  //! RdWithRsp: factory for creating ReadImpl/PgReadImpl objects
  //----------------------------------------------------------------------------
  template<typename RSP> struct ReadTrait { };

  template<> struct ReadTrait<ChunkInfo> { using RET = ReadImpl<false>; };

  template<> struct ReadTrait<PageInfo> { using RET = PgReadImpl<false>; };

  template<typename RSP> inline typename ReadTrait<RSP>::RET
  RdWithRsp( Ctx<File> file, Arg<uint64_t> offset, Arg<uint32_t> size,
             Arg<void*> buffer, time_t timeout = 0 );

  template<> inline ReadImpl<false>
  RdWithRsp<ChunkInfo>( Ctx<File> file, Arg<uint64_t> offset, Arg<uint32_t> size,
                        Arg<void*> buffer, time_t timeout )
  {
    return Read( std::move( file ), std::move( offset ), std::move( size ),
                 std::move( buffer ), timeout );
  }

  template<> inline PgReadImpl<false>
  RdWithRsp<PageInfo>( Ctx<File> file, Arg<uint64_t> offset, Arg<uint32_t> size,
                       Arg<void*> buffer, time_t timeout )
  {
    return PgRead( std::move( file ), std::move( offset ), std::move( size ),
                   std::move( buffer ), timeout );
  }

  //----------------------------------------------------------------------------
  //! PgWrite operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class PgWriteImpl: public FileOperation<PgWriteImpl, HasHndl, Resp<void>,
      Arg<uint64_t>, Arg<uint32_t>, Arg<void*>, Arg<std::vector<uint32_t>>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<PgWriteImpl, HasHndl, Resp<void>, Arg<uint64_t>,
                          Arg<uint32_t>, Arg<void*>, Arg<std::vector<uint32_t>>>::FileOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { OffsetArg, SizeArg, BufferArg, CksumsArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "PgWrite";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        uint64_t               offset = std::get<OffsetArg>( this->args ).Get();
        uint32_t               size   = std::get<SizeArg>( this->args ).Get();
        void                  *buffer = std::get<BufferArg>( this->args ).Get();
        std::vector<uint32_t>  cksums = std::get<CksumsArg>( this->args ).Get();
        time_t    timeout = pipelineTimeout < this->timeout ?
                            pipelineTimeout : this->timeout;
        return this->file->PgWrite( offset, size, buffer, cksums, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating PgReadImpl objects
  //----------------------------------------------------------------------------
  inline PgWriteImpl<false> PgWrite( Ctx<File> file, Arg<uint64_t> offset,
                                    Arg<uint32_t> size, Arg<void*> buffer,
                                    Arg<std::vector<uint32_t>> cksums,
                                    time_t timeout = 0 )
  {
    return PgWriteImpl<false>( std::move( file ), std::move( offset ), std::move( size ),
                               std::move( buffer ), std::move( cksums ) ).Timeout( timeout );
  }

  //----------------------------------------------------------------------------
  //! Factory for creating PgReadImpl objects
  //----------------------------------------------------------------------------
  inline PgWriteImpl<false> PgWrite( Ctx<File> file, Arg<uint64_t> offset,
                                    Arg<uint32_t> size, Arg<void*> buffer,
                                    time_t timeout = 0 )
  {
    std::vector<uint32_t> cksums;
    return PgWriteImpl<false>( std::move( file ), std::move( offset ), std::move( size ),
                               std::move( buffer ), std::move( cksums ) ).Timeout( timeout );
  }

  //----------------------------------------------------------------------------
  //! Close operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class CloseImpl: public FileOperation<CloseImpl, HasHndl, Resp<void>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<CloseImpl, HasHndl, Resp<void>>::FileOperation;

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
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        time_t   timeout = pipelineTimeout < this->timeout ?
                           pipelineTimeout : this->timeout;
        return this->file->Close( handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating CloseImpl objects
  //----------------------------------------------------------------------------
  inline CloseImpl<false> Close( Ctx<File> file, time_t timeout = 0 )
  {
    return CloseImpl<false>( std::move( file ) ).Timeout( timeout );
  }

  //----------------------------------------------------------------------------
  //! Stat operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class StatImpl: public FileOperation<StatImpl, HasHndl, Resp<StatInfo>, Arg<bool>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<StatImpl, HasHndl, Resp<StatInfo>, Arg<bool>>::FileOperation;

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
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        bool     force   = std::get<ForceArg>( this->args ).Get();
        time_t   timeout = pipelineTimeout < this->timeout ?
                           pipelineTimeout : this->timeout;
        return this->file->Stat( force, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating StatImpl objects (as there is another Stat in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline StatImpl<false> Stat( Ctx<File> file, Arg<bool> force, time_t timeout = 0 )
  {
    return StatImpl<false>( std::move( file ), std::move( force ) ).Timeout( timeout );
  }

  //----------------------------------------------------------------------------
  //! Write operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class WriteImpl: public FileOperation<WriteImpl, HasHndl, Resp<void>, Arg<uint64_t>,
      Arg<uint32_t>, Arg<const void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<WriteImpl, HasHndl, Resp<void>, Arg<uint64_t>, Arg<uint32_t>,
                          Arg<const void*>>::FileOperation;

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
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        uint64_t    offset = std::get<OffsetArg>( this->args ).Get();
        uint32_t    size   = std::get<SizeArg>( this->args ).Get();
        const void *buffer = std::get<BufferArg>( this->args ).Get();
        time_t      timeout = pipelineTimeout < this->timeout ?
                            pipelineTimeout : this->timeout;
        return this->file->Write( offset, size, buffer, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating WriteImpl objects
  //----------------------------------------------------------------------------
  inline WriteImpl<false> Write( Ctx<File> file, Arg<uint64_t> offset, Arg<uint32_t> size,
                                 Arg<const void*> buffer, time_t timeout = 0 )
  {
    return WriteImpl<false>( std::move( file ), std::move( offset ), std::move( size ),
                             std::move( buffer ) ).Timeout( timeout );
  }

  //----------------------------------------------------------------------------
  //! Sync operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class SyncImpl: public FileOperation<SyncImpl, HasHndl, Resp<void>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<SyncImpl, HasHndl, Resp<void>>::FileOperation;

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
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        time_t   timeout = pipelineTimeout < this->timeout ?
                           pipelineTimeout : this->timeout;
        return this->file->Sync( handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating SyncImpl objects
  //----------------------------------------------------------------------------
  inline SyncImpl<false> Sync( Ctx<File> file, time_t timeout = 0 )
  {
    return SyncImpl<false>( std::move( file ) ).Timeout( timeout );
  }

  //----------------------------------------------------------------------------
  //! Truncate operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class TruncateImpl: public FileOperation<TruncateImpl, HasHndl, Resp<void>, Arg<uint64_t>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<TruncateImpl, HasHndl, Resp<void>, Arg<uint64_t>>::FileOperation;

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
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        uint64_t size    = std::get<SizeArg>( this->args ).Get();
        time_t   timeout = pipelineTimeout < this->timeout ?
                           pipelineTimeout : this->timeout;
        return this->file->Truncate( size, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating TruncateImpl objects (as there is another Stat in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline TruncateImpl<false> Truncate( Ctx<File> file, Arg<uint64_t> size, time_t timeout )
  {
    return TruncateImpl<false>( std::move( file ), std::move( size ) ).Timeout( timeout );
  }

  //----------------------------------------------------------------------------
  //! VectorRead operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class VectorReadImpl: public FileOperation<VectorReadImpl, HasHndl,
      Resp<VectorReadInfo>, Arg<ChunkList>, Arg<void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<VectorReadImpl, HasHndl, Resp<VectorReadInfo>, Arg<ChunkList>,
                          Arg<void*>>::FileOperation;

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
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        ChunkList &chunks  = std::get<ChunksArg>( this->args ).Get();
        void      *buffer  = std::get<BufferArg>( this->args ).Get();
        time_t     timeout = pipelineTimeout < this->timeout ?
                             pipelineTimeout : this->timeout;
        return this->file->VectorRead( chunks, buffer, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating VectorReadImpl objects
  //----------------------------------------------------------------------------
  inline VectorReadImpl<false> VectorRead( Ctx<File> file, Arg<ChunkList> chunks,
                                           Arg<void*> buffer, time_t timeout = 0 )
  {
    return VectorReadImpl<false>( std::move( file ), std::move( chunks ), std::move( buffer ) ).Timeout( timeout );
  }

  inline VectorReadImpl<false> VectorRead( Ctx<File> file, Arg<ChunkList> chunks,
                                           time_t timeout = 0 )
  {
    return VectorReadImpl<false>( std::move( file ), std::move( chunks ), nullptr ).Timeout( timeout );
  }

  //----------------------------------------------------------------------------
  //! VectorWrite operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class VectorWriteImpl: public FileOperation<VectorWriteImpl, HasHndl, Resp<void>,
      Arg<ChunkList>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<VectorWriteImpl, HasHndl, Resp<void>, Arg<ChunkList>>::FileOperation;

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
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        const ChunkList &chunks  = std::get<ChunksArg>( this->args ).Get();
        time_t           timeout = pipelineTimeout < this->timeout ?
                                   pipelineTimeout : this->timeout;
        return this->file->VectorWrite( chunks, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating VectorWriteImpl objects
  //----------------------------------------------------------------------------
  inline VectorWriteImpl<false> VectorWrite( Ctx<File> file, Arg<ChunkList> chunks,
                                             time_t timeout = 0 )
  {
    return VectorWriteImpl<false>( std::move( file ), std::move( chunks ) ).Timeout( timeout );
  }

  //----------------------------------------------------------------------------
  //! WriteV operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class WriteVImpl: public FileOperation<WriteVImpl, HasHndl, Resp<void>, Arg<uint64_t>,
                                         Arg<std::vector<iovec>>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<WriteVImpl, HasHndl, Resp<void>, Arg<uint64_t>,
                          Arg<std::vector<iovec>>>::FileOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { OffsetArg, IovArg };

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
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        uint64_t            offset  = std::get<OffsetArg>( this->args ).Get();
        std::vector<iovec> &stdiov  = std::get<IovArg>( this->args ).Get();
        time_t              timeout = pipelineTimeout < this->timeout ?
                                      pipelineTimeout : this->timeout;

        int iovcnt = stdiov.size();
        iovec iov[iovcnt];
        for( size_t i = 0; i < stdiov.size(); ++i )
        {
          iov[i].iov_base = stdiov[i].iov_base;
          iov[i].iov_len  = stdiov[i].iov_len;
        }

        return this->file->WriteV( offset, iov, iovcnt, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating WriteVImpl objects
  //----------------------------------------------------------------------------
  inline WriteVImpl<false> WriteV( Ctx<File> file, Arg<uint64_t> offset,
                                   Arg<std::vector<iovec>> iov, time_t timeout = 0 )
  {
    return WriteVImpl<false>( std::move( file ), std::move( offset ),
                              std::move( iov ) ).Timeout( timeout );
  }

  //----------------------------------------------------------------------------
  //! Fcntl operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class FcntlImpl: public FileOperation<FcntlImpl, HasHndl, Resp<Buffer>, Arg<Buffer>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<FcntlImpl, HasHndl, Resp<Buffer>, Arg<Buffer>>::FileOperation;

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
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        Buffer   &arg     = std::get<BufferArg>( this->args ).Get();
        time_t    timeout = pipelineTimeout < this->timeout ?
                            pipelineTimeout : this->timeout;
        return this->file->Fcntl( arg, handler, timeout );
      }
  };
  typedef FcntlImpl<false> Fcntl;

  //----------------------------------------------------------------------------
  //! Visa operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class VisaImpl: public FileOperation<VisaImpl, HasHndl, Resp<Buffer>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<VisaImpl, HasHndl, Resp<Buffer>>::FileOperation;

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
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        time_t   timeout = pipelineTimeout < this->timeout ?
                           pipelineTimeout : this->timeout;
        return this->file->Visa( handler, timeout );
      }
  };
  typedef VisaImpl<false> Visa;

  //----------------------------------------------------------------------------
  //! SetXAttr operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class SetXAttrImpl: public FileOperation<SetXAttrImpl, HasHndl, Resp<void>,
      Arg<std::string>, Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<SetXAttrImpl, HasHndl, Resp<void>,
                          Arg<std::string>, Arg<std::string>>::FileOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { NameArg, ValueArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "SetXAttrImpl";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        std::string &name  = std::get<NameArg>( this->args ).Get();
        std::string &value = std::get<ValueArg>( this->args ).Get();
        // wrap the arguments with a vector
        std::vector<xattr_t> attrs;
        attrs.push_back( xattr_t( name, value ) );
        // wrap the PipelineHandler so the response gets unpacked properly
        UnpackXAttrStatus *h = new UnpackXAttrStatus( handler );
        time_t       timeout = pipelineTimeout < this->timeout ?
                                     pipelineTimeout : this->timeout;
        XRootDStatus st = this->file->SetXAttr( attrs, h, timeout );
        if( !st.IsOK() ) delete h;
        return st;
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating SetXAttrImpl objects (as there is another SetXAttr in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline SetXAttrImpl<false> SetXAttr( Ctx<File> file, Arg<std::string> name, Arg<std::string> value )
  {
    return SetXAttrImpl<false>( std::move( file ), std::move( name ), std::move( value ) );
  }

  //----------------------------------------------------------------------------
  //! SetXAttr bulk operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class SetXAttrBulkImpl: public FileOperation<SetXAttrBulkImpl, HasHndl,
      Resp<std::vector<XAttrStatus>>, Arg<std::vector<xattr_t>>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<SetXAttrBulkImpl, HasHndl, Resp<std::vector<XAttrStatus>>,
                          Arg<std::vector<xattr_t>>>::FileOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { AttrsArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "SetXAttrBulkImpl";
      }


    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        std::vector<xattr_t> &attrs   = std::get<AttrsArg>( this->args ).Get();
        time_t                timeout = pipelineTimeout < this->timeout ?
                                        pipelineTimeout : this->timeout;
        return this->file->SetXAttr( attrs, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating SetXAttrBulkImpl objects (as there is another SetXAttr
  //! in FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline SetXAttrBulkImpl<false> SetXAttr( Ctx<File> file, Arg<std::vector<xattr_t>> attrs )
  {
    return SetXAttrBulkImpl<false>( std::move( file ), std::move( attrs ) );
  }

  //----------------------------------------------------------------------------
  //! GetXAttr operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class GetXAttrImpl: public FileOperation<GetXAttrImpl, HasHndl, Resp<std::string>,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<GetXAttrImpl, HasHndl, Resp<std::string>,
                          Arg<std::string>>::FileOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { NameArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "GetXAttrImpl";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        std::string &name = std::get<NameArg>( this->args ).Get();
        // wrap the argument with a vector
        std::vector<std::string> attrs;
        attrs.push_back( name );
        // wrap the PipelineHandler so the response gets unpacked properly
        UnpackXAttr   *h = new UnpackXAttr( handler );
        time_t   timeout = pipelineTimeout < this->timeout ?
                           pipelineTimeout : this->timeout;
        XRootDStatus st = this->file->GetXAttr( attrs, h, timeout );
        if( !st.IsOK() ) delete h;
        return st;
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating GetXAttrImpl objects (as there is another GetXAttr in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline GetXAttrImpl<false> GetXAttr( Ctx<File> file, Arg<std::string> name )
  {
    return GetXAttrImpl<false>( std::move( file ), std::move( name ) );
  }

  //----------------------------------------------------------------------------
  //! GetXAttr bulk operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class GetXAttrBulkImpl: public FileOperation<GetXAttrBulkImpl, HasHndl, Resp<std::vector<XAttr>>,
      Arg<std::vector<std::string>>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<GetXAttrBulkImpl, HasHndl, Resp<std::vector<XAttr>>,
                          Arg<std::vector<std::string>>>::FileOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { NamesArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "GetXAttrBulkImpl";
      }


    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        std::vector<std::string> &attrs   = std::get<NamesArg>( this->args ).Get();
        time_t                    timeout = pipelineTimeout < this->timeout ?
                                            pipelineTimeout : this->timeout;
        return this->file->GetXAttr( attrs, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating GetXAttrBulkImpl objects (as there is another GetXAttr in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline GetXAttrBulkImpl<false> GetXAttr( Ctx<File> file, Arg<std::vector<std::string>> attrs )
  {
    return GetXAttrBulkImpl<false>( std::move( file ), std::move( attrs ) );
  }

  //----------------------------------------------------------------------------
  //! DelXAttr operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class DelXAttrImpl: public FileOperation<DelXAttrImpl, HasHndl, Resp<void>,
      Arg<std::string>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<DelXAttrImpl, HasHndl, Resp<void>, Arg<std::string>>::FileOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { NameArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "DelXAttrImpl";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        std::string &name = std::get<NameArg>( this->args ).Get();
        // wrap the argument with a vector
        std::vector<std::string> attrs;
        attrs.push_back( name );
        // wrap the PipelineHandler so the response gets unpacked properly
        UnpackXAttrStatus *h = new UnpackXAttrStatus( handler );
        time_t       timeout = pipelineTimeout < this->timeout ?
                               pipelineTimeout : this->timeout;
        XRootDStatus st = this->file->DelXAttr( attrs, h, timeout );
        if( !st.IsOK() ) delete h;
        return st;
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating DelXAttrImpl objects (as there is another DelXAttr in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline DelXAttrImpl<false> DelXAttr( Ctx<File> file, Arg<std::string> name )
  {
    return DelXAttrImpl<false>( std::move( file ), std::move( name ) );
  }

  //----------------------------------------------------------------------------
  //! DelXAttr bulk operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class DelXAttrBulkImpl: public FileOperation<DelXAttrBulkImpl, HasHndl,
      Resp<std::vector<XAttrStatus>>, Arg<std::vector<std::string>>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<DelXAttrBulkImpl, HasHndl, Resp<std::vector<XAttrStatus>>,
                          Arg<std::vector<std::string>>>::FileOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { NamesArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "DelXAttrBulkImpl";
      }


    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        std::vector<std::string> &attrs   = std::get<NamesArg>( this->args ).Get();
        time_t                    timeout = pipelineTimeout < this->timeout ?
                                            pipelineTimeout : this->timeout;
        return this->file->DelXAttr( attrs, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating DelXAttrBulkImpl objects (as there is another DelXAttr
  //! in FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline DelXAttrBulkImpl<false> DelXAttr( Ctx<File> file, Arg<std::vector<std::string>> attrs )
  {
    return DelXAttrBulkImpl<false>( std::move( file ), std::move( attrs ) );
  }

  //----------------------------------------------------------------------------
  //! ListXAttr bulk operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class ListXAttrImpl: public FileOperation<ListXAttrImpl, HasHndl,
      Resp<std::vector<XAttr>>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<ListXAttrImpl, HasHndl, Resp<std::vector<XAttr>>>::FileOperation;

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "ListXAttrImpl";
      }


    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        time_t   timeout = pipelineTimeout < this->timeout ?
                           pipelineTimeout : this->timeout;
        return this->file->ListXAttr( handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating ListXAttrImpl objects (as there is another ListXAttr
  //! in FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline ListXAttrImpl<false> ListXAttr( Ctx<File> file )
  {
    return ListXAttrImpl<false>( std::move( file ) );
  }
}

#endif // __XRD_CL_FILE_OPERATIONS_HH__

