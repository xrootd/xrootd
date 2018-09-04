//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Krzysztof Jamrog <krzysztof.piotr.jamrog@cern.ch>
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


namespace XrdCl {

    template <State state>
    class FileOperation: public Operation<state> {

          template<State> friend class FileOperation;

        public:
            //------------------------------------------------------------------
            //! Constructor
            //!
            //! @param f  file on which operation will be performed
            //------------------------------------------------------------------
            FileOperation(File *f): file(f){}

            //------------------------------------------------------------------
            //! Copy constructor - used to return Bare objects by value
            //------------------------------------------------------------------
            FileOperation(const FileOperation<Bare> &op): Operation<Bare>(), file(op.file){
                static_assert(state == Bare, "Copy constructor is available only for type FileOperation<Bare>");
            }

            template<State from>
            FileOperation( FileOperation<from> && op ) : Operation<state>( std::move( op ) ), file( op.file )
            {

            }

            virtual ~FileOperation(){}

        protected:
            //------------------------------------------------------------------
            //! Constructor (used internally to change copy object with 
            //! change of template parameter)
            //!
            //! @param f  file on which operation will be performed
            //! @param h  operation handler
            //------------------------------------------------------------------
            FileOperation(File *f, std::unique_ptr<OperationHandler> h): Operation<state>(std::move(h)), file(f){}

            File *file;
    };

    template <State state>
    class OpenImpl: public FileOperation<state>, public Args<Arg<std::string>, Arg<OpenFlags::Flags>, Arg<Access::Mode>> {
        public:
            OpenImpl(File *f): FileOperation<state>(f) {}
            OpenImpl(File &f): FileOperation<state>(&f) {}
            OpenImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)) {}

            template<State from>
            OpenImpl( OpenImpl<from> && open ) : FileOperation<state>( std::move( open ) ),
                                                 Args<Arg<std::string>, Arg<OpenFlags::Flags>, Arg<Access::Mode>>( std::move( open ) ){}
            struct UrlArg {
                static const int index = 0;
                static const std::string key;
                typedef std::string type;
            };

            struct FlagsArg {
                static const int index = 1;
                static const std::string key;
                typedef OpenFlags::Flags type;
            };

            struct ModeArg {
                static const int index = 2;
                static const std::string key;
                typedef Access::Mode type;
            };

            OpenImpl<Configured> operator()(Arg<std::string> url, Arg<OpenFlags::Flags> flags, Arg<Access::Mode> mode = Access::None){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( url ), std::move( flags ), std::move( mode ) );
                return OpenImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, StatInfo&)> handleFunction){
              ForwardingHandler *forwardingHandler = new ExOpenFuncWrapper(*this->file, handleFunction);
              return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, StatInfo&, OperationContext&)> handleFunction){
              ForwardingHandler *forwardingHandler = new ForwardingExOpenFuncWrapper(*this->file, handleFunction);
              return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Open";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    std::string      &url   = Get<UrlArg>( params, bucket );
                    OpenFlags::Flags &flags = Get<FlagsArg>( params, bucket );
                    Access::Mode     &mode  = Get<ModeArg>( params, bucket );
                    return this->file->Open(url, flags, mode, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                OpenImpl<Handled>* o = new OpenImpl<Handled>(this->file, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    typedef OpenImpl<Bare> Open;
    template <State state> const std::string OpenImpl<state>::UrlArg::key = "url";
    template <State state> const std::string OpenImpl<state>::FlagsArg::key = "flags";
    template <State state> const std::string OpenImpl<state>::ModeArg::key = "mode";


    template <State state>
    class ReadImpl: public FileOperation<state>, public Args<Arg<uint64_t>, Arg<uint32_t>, Arg<void*>> {
        public:
            ReadImpl(File *f): FileOperation<state>(f){}
            ReadImpl(File &f): FileOperation<state>(&f){}
            ReadImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}

            template<State from>
            ReadImpl( ReadImpl<from> && read ) : FileOperation<state>( std::move( read ) ),
                                                 Args<Arg<uint64_t>, Arg<uint32_t>, Arg<void*>>( std::move( read ) ) { }

            struct OffsetArg {
                static const int index = 0;
                static const std::string key;
                typedef uint64_t type;
            };

            struct SizeArg {
                static const int index = 1;
                static const std::string key;
                typedef uint32_t type;
            };

            struct BufferArg {
                static const int index = 2;
                static const std::string key;
                typedef void* type;
            };

            ReadImpl<Configured> operator()(Arg<uint64_t> offset, Arg<uint32_t> size, Arg<void*> buffer) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( offset ), std::move( size ), std::move( buffer ) );
                return ReadImpl<Configured>( std::move( *this ) );
            }     

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, ChunkInfo&)> handleFunction){
                ForwardingHandler *forwardingHandler = new FunctionWrapper<ChunkInfo>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, ChunkInfo&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<ChunkInfo>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Read";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try {
                    uint64_t &offset = Get<OffsetArg>( params, bucket );
                    uint32_t &size   = Get<SizeArg>( params, bucket );
                    void     *buffer = Get<BufferArg>( params, bucket );
                    return this->file->Read(offset, size, buffer, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }        
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                ReadImpl<Handled>* r = new ReadImpl<Handled>(this->file, std::move(h));
                r->TakeArgs( std::move( _args ) );
                return r;
            }
    };
    typedef ReadImpl<Bare> Read;
    template <State state> const std::string ReadImpl<state>::OffsetArg::key = "offset";
    template <State state> const std::string ReadImpl<state>::SizeArg::key = "size";
    template <State state> const std::string ReadImpl<state>::BufferArg::key = "buffer";


    template <State state = Bare>
    class CloseImpl: public FileOperation<state> {
        public:
            CloseImpl(File *f): FileOperation<state>(f){}
            CloseImpl(File &f): FileOperation<state>(&f){}
            CloseImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}

            template<State from>
            CloseImpl( CloseImpl<from> && close ) : FileOperation<state>( std::move( close ) ){ }

            CloseImpl<Configured> operator()(){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                return CloseImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Close";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                return this->file->Close(this->handler.get());
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                CloseImpl<Handled> *c = new CloseImpl<Handled>(this->file, std::move(h));
                return c;
            }
    };
    typedef CloseImpl<Bare> Close;

    
    template <State state = Bare>
    class StatImpl: public FileOperation<state>, public Args<Arg<bool>> {
        public:
            StatImpl(File *f): FileOperation<state>(f){}
            StatImpl(File &f): FileOperation<state>(&f){}
            StatImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}

            template<State from>
            StatImpl( StatImpl<from> && stat ) : FileOperation<state>( std::move( stat ) ), Args<Arg<bool>>( std::move( stat ) )
            {

            }

            struct ForceArg {
                static const int index = 0;
                static const std::string key;
                typedef bool type;
            };

            StatImpl<Configured> operator()(Arg<bool> force){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( force ) );
                return StatImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, StatInfo&)> handleFunction){
                ForwardingHandler *forwardingHandler = new FunctionWrapper<StatInfo>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, StatInfo&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<StatInfo>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Stat";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try {
                    bool force = Get<ForceArg>( params, bucket );
                    return this->file->Stat(force, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                StatImpl<Handled> *c = new StatImpl<Handled>(this->file, std::move(h));
                c->TakeArgs( std::move( _args ) );
                return c;
            }
    };
    template <State state> const std::string StatImpl<state>::ForceArg::key = "force";

    StatImpl<Bare> Stat(File *file){
        return StatImpl<Bare>(file, nullptr);
    }


    template <State state>
    class WriteImpl: public FileOperation<state>, public Args<Arg<uint64_t>, Arg<uint32_t>,Arg<void*>> {
        public:
            WriteImpl(File *f): FileOperation<state>(f){}
            WriteImpl(File &f): FileOperation<state>(&f){}
            WriteImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}
            
            template<State from>
            WriteImpl( WriteImpl<from> && write ) : FileOperation<state>( std::move( write ) ), Args<Arg<uint64_t>, Arg<uint32_t>,Arg<void*>>( std::move( write ) ) {}

            struct OffsetArg {
                static const int index = 0;
                static const std::string key;
                typedef uint64_t type;
            };

            struct SizeArg {
                static const int index = 1;
                static const std::string key;
                typedef uint32_t type;
            };

            struct BufferArg {
                static const int index = 2;
                static const std::string key;
                typedef void* type;
            };

            WriteImpl<Configured> operator()(Arg<uint64_t> offset, Arg<uint32_t> size, Arg<void*> buffer) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( offset ), std::move( size ), std::move( buffer ) );
                return WriteImpl<Configured>( std::move( *this ) );
            }     

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Write";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try {
                    uint64_t &offset = Get<OffsetArg>( params, bucket );
                    uint32_t &size   = Get<SizeArg>( params, bucket );
                    void     *buffer = Get<BufferArg>( params, bucket );
                    return this->file->Write(offset, size, buffer, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                WriteImpl<Handled>* r = new WriteImpl<Handled>(this->file, std::move(h));
                r->TakeArgs( std::move( _args ) );
                return r;
            }
    };
    typedef WriteImpl<Bare> Write;
    template <State state> const std::string WriteImpl<state>::OffsetArg::key = "offset";
    template <State state> const std::string WriteImpl<state>::SizeArg::key = "size";
    template <State state> const std::string WriteImpl<state>::BufferArg::key = "buffer";


    template <State state = Bare>
    class SyncImpl: public FileOperation<state> {
        public:
            SyncImpl(File *f): FileOperation<state>(f){}
            SyncImpl(File &f): FileOperation<state>(&f){}
            SyncImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}

            template<State from>
            SyncImpl( SyncImpl<state> && sync ) : FileOperation<state>( std::move( sync ) ) { }

            SyncImpl<Configured> operator()(){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                return SyncImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Sync";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                return this->file->Sync(this->handler.get());
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                SyncImpl<Handled> *c = new SyncImpl<Handled>(this->file, std::move(h));
                return c;
            }
    };
    typedef SyncImpl<Bare> Sync;


    template <State state>
    class TruncateImpl: public FileOperation<state>, public Args<Arg<uint64_t>> {
        public:
            TruncateImpl(File *f): FileOperation<state>(f){}
            TruncateImpl(File &f): FileOperation<state>(&f){}
            TruncateImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}
            
            template<State from>
            TruncateImpl( TruncateImpl<from> && trunc ) : FileOperation<state>( std::move( trunc ) ), Args<Arg<uint64_t>>( std::move( trunc ) ) { }

            struct SizeArg {
                static const int index = 0;
                static const std::string key;
                typedef uint64_t type;
            };

            TruncateImpl<Configured> operator()(Arg<uint64_t> size) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( size ) );
                return TruncateImpl<Configured>( std::move( this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            } 

            std::string ToString(){
                return "Truncate";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try {
                    uint64_t &size = Get<SizeArg>( params, bucket );
                    return this->file->Truncate(size, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                TruncateImpl<Handled>* r = new TruncateImpl<Handled>(this->file, std::move(h));
                r->TakeArgs( std::move( _args ) );
                return r;
            }
    };
    template <State state> const std::string TruncateImpl<state>::SizeArg::key = "size";

     TruncateImpl<Bare> Truncate(File *file){
        return TruncateImpl<Bare>(file, nullptr);
    }


    template <State state>
    class VectorReadImpl: public FileOperation<state>, public Args<Arg<ChunkList>, Arg<void*>> {
        public:
            VectorReadImpl(File *f): FileOperation<state>(f){}
            VectorReadImpl(File &f): FileOperation<state>(&f){}
            VectorReadImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}
            
            template<State from>
            VectorReadImpl( VectorReadImpl<from> && vread ) : FileOperation<state>( std::move( vread ) ), Args<Arg<ChunkList>, Arg<void*>>( std::move( vread ) ) { }

            struct ChunksArg {
                static const int index = 0;
                static const std::string key;
                typedef ChunkList type;
            };

            struct BufferArg {
                static const int index = 1;
                static const std::string key;
                typedef char* type;
            };

            VectorReadImpl<Configured> operator()(Arg<ChunkList> chunks, Arg<void*> buffer) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( chunks ), std::move( buffer ) );
                return VectorReadImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "VectorRead";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try {
                    const ChunkList &chunks = Get<ChunksArg>( params, bucket );
                    void            *buffer = Get<BufferArg>( params, bucket );
                    return this->file->VectorRead(chunks, buffer, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                VectorReadImpl<Handled>* r = new VectorReadImpl<Handled>(this->file, std::move(h));
                r->TakeArgs( std::move( _args ) );
                return r;
            }
    };
    typedef VectorReadImpl<Bare> VectorRead;
    template <State state> const std::string VectorReadImpl<state>::ChunksArg::key = "chunks";
    template <State state> const std::string VectorReadImpl<state>::BufferArg::key = "buffer";


    template <State state>
    class VectorWriteImpl: public FileOperation<state>, Args<Arg<ChunkList>> {
        public:
            VectorWriteImpl(File *f): FileOperation<state>(f){}
            VectorWriteImpl(File &f): FileOperation<state>(&f){}
            VectorWriteImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}
            
            template<State from>
            VectorWriteImpl( VectorWriteImpl<from> && vwrite ) : FileOperation<state>( std::move( vwrite ) ), Args<Arg<ChunkList>>( std::move( vwrite ) ) { }

            struct ChunksArg {
                static const int index = 0;
                static const std::string key;
                typedef ChunkList type;
            };

            VectorWriteImpl<Configured> operator()(Arg<ChunkList> chunks) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( chunks ) );
                return VectorWriteImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "VectorWrite";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try {
                    const ChunkList& chunks = Get<ChunksArg>( params, bucket );
                    return this->file->VectorWrite(chunks, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                VectorWriteImpl<Handled>* r = new VectorWriteImpl<Handled>(this->file, std::move(h));
                r->TakeArgs( std::move( _args ) );
                return r;
            }
    };
    typedef VectorWriteImpl<Bare> VectorWrite;
    template <State state> const std::string VectorWriteImpl<state>::ChunksArg::key = "chunks";


    template <State state>
    class WriteVImpl: public FileOperation<state>, public Args<Arg<uint64_t>, Arg<struct iovec*>, Arg<int>> {
        public:
            WriteVImpl(File *f): FileOperation<state>(f){}
            WriteVImpl(File &f): FileOperation<state>(&f){}
            WriteVImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}
            
            template<State from>
            WriteVImpl( WriteVImpl<from> && writev ) : FileOperation<state>( std::move( writev ) ), Args<Arg<uint64_t>, Arg<struct iovec*>, Arg<int>>( std::move( writev ) ) { }

            struct OffsetArg {
                static const int index = 0;
                static const std::string key;
                typedef uint64_t type;
            };

            struct IovArg {
                static const int index = 1;
                static const std::string key;
                typedef struct iovec* type;
            };

            struct IovcntArg {
                static const int index = 2;
                static const std::string key;
                typedef int type;
            };

            WriteVImpl<Configured> operator()(Arg<uint64_t> offset, Arg<struct iovec*> iov, Arg<int> iovcnt) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( offset ), std::move( iov ), std::move( iovcnt ) );
                return WriteVImpl<Configured>( std::move( this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "WriteV";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try {                    
                    uint64_t           &offset = Get<OffsetArg>( params, bucket );
                    const struct iovec *iov    = Get<IovArg>( params, bucket );
                    int                &iovcnt = Get<IovcntArg>( params, bucket );
                    return this->file->WriteV(offset, iov, iovcnt, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }

            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                WriteVImpl<Handled>* r = new WriteVImpl<Handled>(this->file, std::move(h));
                r->TakeArgs( std::move( _args ) );
                return r;
            }
    };
    typedef WriteVImpl<Bare> WriteV;
    template <State state> const std::string WriteVImpl<state>::OffsetArg::key = "offset";
    template <State state> const std::string WriteVImpl<state>::IovArg::key = "iov";
    template <State state> const std::string WriteVImpl<state>::IovcntArg::key = "iovcnt";


    template <State state>
    class FcntlImpl: public FileOperation<state>, public Args<Arg<Buffer>> {
        public:
            FcntlImpl(File *f): FileOperation<state>(f){}
            FcntlImpl(File &f): FileOperation<state>(&f){}
            FcntlImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}
            
            template<State from>
            FcntlImpl( FcntlImpl<from> && fcntl ) : FileOperation<state>( std::move( fcntl ) ), Args<Arg<Buffer>>( std::move( fcntl )  ) { }

            struct BufferArg {
                static const int index = 0;
                static const std::string key;
                typedef Buffer type;
            };

            FcntlImpl<Configured> operator()(Arg<Buffer> arg) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( arg ) );
                return FcntlImpl<Configured>( std::move( *this ) );
            }     

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, Buffer&)> handleFunction){
                ForwardingHandler *forwardingHandler = new FunctionWrapper<Buffer>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, Buffer&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<Buffer>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Fcntl";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try {
                    const Buffer& arg = Get<BufferArg>( params, bucket );
                    return this->file->Fcntl(arg, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                FcntlImpl<Handled>* r = new FcntlImpl<Handled>(this->file, std::move(h));
                r->TakeArgs( std::move( _args ) );
                return r;
            }
    };
    typedef FcntlImpl<Bare> Fcntl;
    template <State state> const std::string FcntlImpl<state>::BufferArg::key = "arg";


    template <State state = Bare>
    class VisaImpl: public FileOperation<state> {
        public:
            VisaImpl(File *f): FileOperation<state>(f){}
            VisaImpl(File &f): FileOperation<state>(&f){}
            VisaImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}

            template<State from>
            VisaImpl( VisaImpl<from> && visa ): FileOperation<state>( std::move( visa ) ) { }

            VisaImpl<Configured> operator()(){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                return VisaImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, Buffer&)> handleFunction){
                ForwardingHandler *forwardingHandler = new FunctionWrapper<Buffer>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, Buffer&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<Buffer>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Visa";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                return this->file->Visa(this->handler.get());
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                VisaImpl<Handled> *c = new VisaImpl<Handled>(this->file, std::move(h));
                return c;
            }
    };
    typedef VisaImpl<Bare> Visa;
}



#endif // __XRD_CL_FILE_OPERATIONS_HH__

