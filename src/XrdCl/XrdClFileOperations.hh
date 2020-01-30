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
      FileOperation( File *f, Arguments... args): ConcreteOperation<Derived, false, Response, Arguments...>( std::move( args )... ), file(f)
      {
      }

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param f    : file on which the operation will be performed
      //! @param args : file operation arguments
      //------------------------------------------------------------------------
      FileOperation( File &f, Arguments... args): FileOperation( &f, std::move( args )... )
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
      File *file;
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
            return make_finalized( new ExOpenFuncWrapper( this->file, func ) );
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
      OpenImpl( File *f, Arg<std::string> url, Arg<OpenFlags::Flags> flags,
                Arg<Access::Mode> mode = Access::None ) :
          FileOperation<OpenImpl, HasHndl, Resp<void>, Arg<std::string>, Arg<OpenFlags::Flags>,
            Arg<Access::Mode>>( f, std::move( url ), std::move( flags ), std::move( mode ) )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor (@see FileOperation)
      //------------------------------------------------------------------------
      OpenImpl( File &f, Arg<std::string> url, Arg<OpenFlags::Flags> flags,
                Arg<Access::Mode> mode = Access::None ) :
          FileOperation<OpenImpl, HasHndl, Resp<void>, Arg<std::string>, Arg<OpenFlags::Flags>,
            Arg<Access::Mode>>( &f, std::move( url ), std::move( flags ), std::move( mode ) )
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
      OpenImpl( OpenImpl<from> && open ) :
          FileOperation<OpenImpl, HasHndl, Resp<void>, Arg<std::string>,
              Arg<OpenFlags::Flags>, Arg<Access::Mode>>( std::move( open ) )
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
      //! @param func : function/functor/lambda
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
  typedef OpenImpl<false> Open;

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
  typedef ReadImpl<false> Read;

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
      XRootDStatus RunImpl()
      {
        return this->file->Close( this->handler.get() );
      }
  };
  typedef CloseImpl<false> Close;

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
  inline StatImpl<false> Stat( File *file, Arg<bool> force )
  {
    return StatImpl<false>( file, std::move( force ) );
  }

  //----------------------------------------------------------------------------
  //! Factory for creating StatImpl objects (as there is another Stat in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline StatImpl<false> Stat( File &file, Arg<bool> force )
  {
    return StatImpl<false>( file, std::move( force ) );
  }

  //----------------------------------------------------------------------------
  //! Write operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class WriteImpl: public FileOperation<WriteImpl, HasHndl, Resp<void>, Arg<uint64_t>,
      Arg<uint32_t>, Arg<void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<WriteImpl, HasHndl, Resp<void>, Arg<uint64_t>, Arg<uint32_t>,
                          Arg<void*>>::FileOperation;

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
  typedef WriteImpl<false> Write;

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
      XRootDStatus RunImpl()
      {
        return this->file->Sync( this->handler.get() );
      }
  };
  typedef SyncImpl<false> Sync;

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
  inline TruncateImpl<false> Truncate( File *file, Arg<uint64_t> size )
  {
    return TruncateImpl<false>( file, std::move( size ) );
  }

  //----------------------------------------------------------------------------
  //! Factory for creating TruncateImpl objects (as there is another Stat in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline TruncateImpl<false> Truncate( File &file, Arg<uint64_t> size )
  {
    return TruncateImpl<false>( file, std::move( size ) );
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
  typedef VectorReadImpl<false> VectorRead;

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
  typedef VectorWriteImpl<false> VectorWrite;

  //----------------------------------------------------------------------------
  //! WriteV operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class WriteVImpl: public FileOperation<WriteVImpl, HasHndl, Resp<void>, Arg<uint64_t>,
      Arg<struct iovec*>, Arg<int>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<WriteVImpl, HasHndl, Resp<void>, Arg<uint64_t>,
                          Arg<struct iovec*>, Arg<int>>::FileOperation;

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
  typedef WriteVImpl<false> WriteV;

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
      XRootDStatus RunImpl()
      {
        return this->file->Visa( this->handler.get() );
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
      XRootDStatus RunImpl()
      {
        try
        {
          std::string name  = std::get<NameArg>( this->args ).Get();
          std::string value = std::get<ValueArg>( this->args ).Get();
          // wrap the arguments with a vector
          std::vector<xattr_t> attrs;
          attrs.push_back( xattr_t( std::move( name ), std::move( value ) ) );
          // wrap the PipelineHandler so the response gets unpacked properly
          UnpackXAttrStatus *handler = new UnpackXAttrStatus( this->handler.get() );
          XRootDStatus st = this->file->SetXAttr( attrs, handler );
          if( !st.IsOK() ) delete handler;
          return st;
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
  //! Factory for creating SetXAttrImpl objects (as there is another SetXAttr in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline SetXAttrImpl<false> SetXAttr( File *file, Arg<std::string> name, Arg<std::string> value )
  {
    return SetXAttrImpl<false>( file, std::move( name ), std::move( value ) );
  }

  //----------------------------------------------------------------------------
  //! Factory for creating SetXAttrImpl objects (as there is another SetXAttr in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline SetXAttrImpl<false> SetXAttr( File &file, Arg<std::string> name, Arg<std::string> value )
  {
    return SetXAttrImpl<false>( file, std::move( name ), std::move( value ) );
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
      XRootDStatus RunImpl()
      {
        try
        {
          std::vector<xattr_t> attrs = std::get<AttrsArg>( this->args ).Get();
          return this->file->SetXAttr( attrs, this->handler.get() );
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
  //! Factory for creating SetXAttrBulkImpl objects (as there is another SetXAttr
  //! in FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline SetXAttrBulkImpl<false> SetXAttr( File *file, Arg<std::vector<xattr_t>> attrs )
  {
    return SetXAttrBulkImpl<false>( file, std::move( attrs ) );
  }

  //----------------------------------------------------------------------------
  //! Factory for creating SetXAttrBulkImpl objects (as there is another SetXAttr
  //! in FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline SetXAttrBulkImpl<false> SetXAttr( File &file, Arg<std::vector<xattr_t>> attrs )
  {
    return SetXAttrBulkImpl<false>( file, std::move( attrs ) );
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
      XRootDStatus RunImpl()
      {
        try
        {
          std::string name = std::get<NameArg>( this->args ).Get();
          // wrap the argument with a vector
          std::vector<std::string> attrs;
          attrs.push_back( std::move( name ) );
          // wrap the PipelineHandler so the response gets unpacked properly
          UnpackXAttr *handler = new UnpackXAttr( this->handler.get() );
          XRootDStatus st = this->file->GetXAttr( attrs, handler );
          if( !st.IsOK() ) delete handler;
          return st;
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
  //! Factory for creating GetXAttrImpl objects (as there is another GetXAttr in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline GetXAttrImpl<false> GetXAttr( File *file, Arg<std::string> name )
  {
    return GetXAttrImpl<false>( file, std::move( name ) );
  }

  //----------------------------------------------------------------------------
  //! Factory for creating GetXAttrImpl objects (as there is another GetXAttr in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline GetXAttrImpl<false> GetXAttr( File &file, Arg<std::string> name )
  {
    return GetXAttrImpl<false>( file, std::move( name ) );
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
      XRootDStatus RunImpl()
      {
        try
        {
          std::vector<std::string> attrs = std::get<NamesArg>( this->args ).Get();
          return this->file->GetXAttr( attrs, this->handler.get() );
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
  //! Factory for creating GetXAttrBulkImpl objects (as there is another GetXAttr in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline GetXAttrBulkImpl<false> GetXAttr( File *file, Arg<std::vector<std::string>> attrs )
  {
    return GetXAttrBulkImpl<false>( file, std::move( attrs ) );
  }

  //----------------------------------------------------------------------------
  //! Factory for creating GetXAttrBulkImpl objects (as there is another GetXAttr in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline GetXAttrBulkImpl<false> GetXAttr( File &file, Arg<std::vector<std::string>> attrs )
  {
    return GetXAttrBulkImpl<false>( file, std::move( attrs ) );
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
      XRootDStatus RunImpl()
      {
        try
        {
          std::string name = std::get<NameArg>( this->args ).Get();
          // wrap the argument with a vector
          std::vector<std::string> attrs;
          attrs.push_back( std::move( name ) );
          // wrap the PipelineHandler so the response gets unpacked properly
          UnpackXAttrStatus *handler = new UnpackXAttrStatus( this->handler.get() );
          XRootDStatus st = this->file->DelXAttr( attrs, handler );
          if( !st.IsOK() ) delete handler;
          return st;
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
  //! Factory for creating DelXAttrImpl objects (as there is another DelXAttr in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline DelXAttrImpl<false> DelXAttr( File *file, Arg<std::string> name )
  {
    return DelXAttrImpl<false>( file, std::move( name ) );
  }

  //----------------------------------------------------------------------------
  //! Factory for creating DelXAttrImpl objects (as there is another DelXAttr in
  //! FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline DelXAttrImpl<false> DelXAttr( File &file, Arg<std::string> name )
  {
    return DelXAttrImpl<false>( file, std::move( name ) );
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
      XRootDStatus RunImpl()
      {
        try
        {
          std::vector<std::string> attrs = std::get<NamesArg>( this->args ).Get();
          return this->file->DelXAttr( attrs, this->handler.get() );
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
  //! Factory for creating DelXAttrBulkImpl objects (as there is another DelXAttr
  //! in FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline DelXAttrBulkImpl<false> DelXAttr( File *file, Arg<std::vector<std::string>> attrs )
  {
    return DelXAttrBulkImpl<false>( file, std::move( attrs ) );
  }

  //----------------------------------------------------------------------------
  //! Factory for creating DelXAttrBulkImpl objects (as there is another DelXAttr
  //! in FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline DelXAttrBulkImpl<false> DelXAttr( File &file, Arg<std::vector<std::string>> attrs )
  {
    return DelXAttrBulkImpl<false>( file, std::move( attrs ) );
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
      XRootDStatus RunImpl()
      {
        return this->file->ListXAttr( this->handler.get() );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating ListXAttrImpl objects (as there is another ListXAttr
  //! in FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline ListXAttrImpl<false> ListXAttr( File *file )
  {
    return ListXAttrImpl<false>( file );
  }

  //----------------------------------------------------------------------------
  //! Factory for creating ListXAttrImpl objects (as there is another ListXAttr
  //! in FileSystem there would be a clash of typenames).
  //----------------------------------------------------------------------------
  inline ListXAttrImpl<false> ListXAttr( File &file )
  {
    return ListXAttrImpl<false>( file );
  }
}

#endif // __XRD_CL_FILE_OPERATIONS_HH__

