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

    template <State state, typename... Args>
    class FileOperation: public Operation<state> {
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
    class OpenImpl: public FileOperation<state> {
        public:
            OpenImpl(File *f): FileOperation<state>(f){}
            OpenImpl(File &f): FileOperation<state>(&f){}
            OpenImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}
            
            struct UrlArg {
                static const std::string key;
                typedef std::string type;
            };

            struct FlagsArg {
                static const std::string key;
                typedef OpenFlags::Flags type;
            };

            struct ModeArg {
                static const std::string key;
                typedef Access::Mode type;
            };

            void SetArgs(Arg<std::string> &url, Arg<OpenFlags::Flags> &flags, Arg<Access::Mode> &mode = Access::None){
                _url = std::move( url );
                _flags = std::move( flags );
                _mode = std::move( mode );
            }

            OpenImpl<Configured>& operator()(Arg<std::string> url, Arg<OpenFlags::Flags> flags, Arg<Access::Mode> mode = Access::None){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                OpenImpl<Configured>* o = new OpenImpl<Configured>(this->file, NULL);
                o->SetArgs(url, flags, mode);
                return *o;
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
                    std::string url = _url.IsEmpty() ? params->GetArg<UrlArg>(bucket) : _url.GetValue();
                    OpenFlags::Flags flags = _flags.IsEmpty() ? params->GetArg<FlagsArg>(bucket) : _flags.GetValue();
                    Access::Mode mode = _mode.IsEmpty() ? params->GetArg<ModeArg>(bucket) : _mode.GetValue();
                    return this->file->Open(url, flags, mode, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                OpenImpl<Handled>* o = new OpenImpl<Handled>(this->file, std::move(h));
                o->SetArgs(_url, _flags, _mode);
                delete this;
                return o;
            }

            Arg<std::string> _url;
            Arg<OpenFlags::Flags> _flags;
            Arg<Access::Mode> _mode;
    };
    typedef OpenImpl<Bare> Open;
    template <State state> const std::string OpenImpl<state>::UrlArg::key = "url";
    template <State state> const std::string OpenImpl<state>::FlagsArg::key = "flags";
    template <State state> const std::string OpenImpl<state>::ModeArg::key = "mode";


    template <State state>
    class ReadImpl: public FileOperation<state> {
        public:
            ReadImpl(File *f): FileOperation<state>(f){}
            ReadImpl(File &f): FileOperation<state>(&f){}
            ReadImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}

            struct OffsetArg {
                static const std::string key;
                typedef uint64_t type;
            };

            struct SizeArg {
                static const std::string key;
                typedef uint32_t type;
            };

            struct BufferArg {
                static const std::string key;
                typedef void* type;
            };

            void SetArgs(Arg<uint64_t> &offset, Arg<uint32_t> &size, Arg<void*> &buffer) {
                _offset = std::move( offset );
                _size = std::move( size );
                _buffer = std::move( buffer );
            }

            ReadImpl<Configured>& operator()(Arg<uint64_t> offset, Arg<uint32_t> size, Arg<void*> buffer) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                ReadImpl<Configured>* r = new ReadImpl<Configured>(this->file, NULL);
                r->SetArgs(offset, size, buffer);
                return *r;
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
                    uint64_t offset = _offset.IsEmpty() ? params->GetArg<OffsetArg>(bucket) : _offset.GetValue();
                    uint32_t size = _size.IsEmpty() ? params->GetArg<SizeArg>(bucket) : _size.GetValue();
                    void *buffer = _buffer.IsEmpty() ? params->GetArg<BufferArg>(bucket) : _buffer.GetValue();
                    return this->file->Read(offset, size, buffer, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }        
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                ReadImpl<Handled>* r = new ReadImpl<Handled>(this->file, std::move(h));
                r->SetArgs(_offset, _size, _buffer);
                delete this;
                return r;
            }

            Arg<uint64_t> _offset;
            Arg<uint32_t> _size;
            Arg<void*> _buffer;
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

            CloseImpl<Configured>& operator()(){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                CloseImpl<Configured> *c = new CloseImpl<Configured>(this->file, NULL);
                return *c;
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
                delete this;
                return c;
            }
    };
    typedef CloseImpl<Bare> Close;

    
    template <State state = Bare>
    class StatImpl: public FileOperation<state> {
        public:
            StatImpl(File *f): FileOperation<state>(f){}
            StatImpl(File &f): FileOperation<state>(&f){}
            StatImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}

            struct ForceArg {
                static const std::string key;
                typedef bool type;
            };


            void SetArgs(Arg<bool> &force) {
                _force = std::move( force );
            }

            StatImpl<Configured>& operator()(Arg<bool> force){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                StatImpl<Configured> *c = new StatImpl<Configured>(this->file, NULL);
                c->SetArgs(force);
                return *c;
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
                    bool force = _force.IsEmpty() ? params->GetArg<ForceArg>(bucket) : _force.GetValue();
                    return this->file->Stat(force, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                StatImpl<Handled> *c = new StatImpl<Handled>(this->file, std::move(h));
                c->SetArgs(_force);
                delete this;
                return c;
            }

            Arg<bool> _force;
    };
    template <State state> const std::string StatImpl<state>::ForceArg::key = "force";

    StatImpl<Bare> Stat(File *file){
        return StatImpl<Bare>(file, nullptr);
    }


    template <State state>
    class WriteImpl: public FileOperation<state> {
        public:
            WriteImpl(File *f): FileOperation<state>(f){}
            WriteImpl(File &f): FileOperation<state>(&f){}
            WriteImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}
            
            struct OffsetArg {
                static const std::string key;
                typedef uint64_t type;
            };

            struct SizeArg {
                static const std::string key;
                typedef uint32_t type;
            };

            struct BufferArg {
                static const std::string key;
                typedef void* type;
            };

            void SetArgs(Arg<uint64_t> &offset, Arg<uint32_t> &size, Arg<void*> &buffer) {
                _offset = std::move( offset );
                _size = std::move( size );
                _buffer = std::move( buffer );
            }

            WriteImpl<Configured>& operator()(Arg<uint64_t> offset, Arg<uint32_t> size, Arg<void*> buffer) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                WriteImpl<Configured>* r = new WriteImpl<Configured>(this->file, NULL);
                r->SetArgs(offset, size, buffer);
                return *r;
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
                    uint64_t offset = _offset.IsEmpty() ? params->GetArg<OffsetArg>(bucket) : _offset.GetValue();
                    uint32_t size = _size.IsEmpty() ? params->GetArg<SizeArg>(bucket) : _size.GetValue();
                    void *buffer = _buffer.IsEmpty() ? params->GetArg<BufferArg>(bucket) : _buffer.GetValue();
                    return this->file->Write(offset, size, buffer, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                WriteImpl<Handled>* r = new WriteImpl<Handled>(this->file, std::move(h));
                r->SetArgs(_offset, _size, _buffer);
                delete this;
                return r;
            }

            Arg<uint64_t> _offset;
            Arg<uint32_t> _size;
            Arg<void*> _buffer;
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

            SyncImpl<Configured>& operator()(){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                SyncImpl<Configured> *c = new SyncImpl<Configured>(this->file, NULL);
                return *c;
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
                delete this;
                return c;
            }
    };
    typedef SyncImpl<Bare> Sync;


    template <State state>
    class TruncateImpl: public FileOperation<state> {
        public:
            TruncateImpl(File *f): FileOperation<state>(f){}
            TruncateImpl(File &f): FileOperation<state>(&f){}
            TruncateImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}
            
            struct SizeArg {
                static const std::string key;
                typedef uint64_t type;
            };

            void SetArgs(Arg<uint64_t> &size) {
                _size = std::move( size );
            }

            TruncateImpl<Configured>& operator()(Arg<uint64_t> size) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                TruncateImpl<Configured>* r = new TruncateImpl<Configured>(this->file, NULL);
                r->SetArgs(size);
                return *r;
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
                    uint32_t size = _size.IsEmpty() ? params->GetArg<SizeArg>(bucket) : _size.GetValue();
                    return this->file->Truncate(size, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                TruncateImpl<Handled>* r = new TruncateImpl<Handled>(this->file, std::move(h));
                r->SetArgs(_size);
                delete this;
                return r;
            }

            Arg<uint64_t> _size;
    };
    template <State state> const std::string TruncateImpl<state>::SizeArg::key = "size";

     TruncateImpl<Bare> Truncate(File *file){
        return TruncateImpl<Bare>(file, nullptr);
    }


    template <State state>
    class VectorReadImpl: public FileOperation<state> {
        public:
            VectorReadImpl(File *f): FileOperation<state>(f){}
            VectorReadImpl(File &f): FileOperation<state>(&f){}
            VectorReadImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}
            
            struct ChunksArg {
                static const std::string key;
                typedef ChunkList type;
            };

            struct BufferArg {
                static const std::string key;
                typedef char* type;
            };

            void SetArgs(Arg<ChunkList> &chunks, Arg<void*> &buffer) {
                _chunks = std::move( chunks );
                _buffer = std::move( buffer );
            }

            VectorReadImpl<Configured>& operator()(Arg<ChunkList> chunks, Arg<void*> buffer) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                VectorReadImpl<Configured>* r = new VectorReadImpl<Configured>(this->file, NULL);
                r->SetArgs(chunks, buffer);
                return *r;
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
                    const ChunkList& chunks = _chunks.IsEmpty() ? params->GetArg<ChunksArg>(bucket) : _chunks.GetValue();
                    void *buffer = _buffer.IsEmpty() ? params->GetArg<BufferArg>(bucket) : _buffer.GetValue();
                    return this->file->VectorRead(chunks, buffer, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                VectorReadImpl<Handled>* r = new VectorReadImpl<Handled>(this->file, std::move(h));
                r->SetArgs(_chunks, _buffer);
                delete this;
                return r;
            }

            Arg<ChunkList> _chunks;
            Arg<void*> _buffer;
    };
    typedef VectorReadImpl<Bare> VectorRead;
    template <State state> const std::string VectorReadImpl<state>::ChunksArg::key = "chunks";
    template <State state> const std::string VectorReadImpl<state>::BufferArg::key = "buffer";


    template <State state>
    class VectorWriteImpl: public FileOperation<state> {
        public:
            VectorWriteImpl(File *f): FileOperation<state>(f){}
            VectorWriteImpl(File &f): FileOperation<state>(&f){}
            VectorWriteImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}
            
            struct ChunksArg {
                static const std::string key;
                typedef ChunkList type;
            };

            void SetArgs(Arg<ChunkList> &chunks) {
                _chunks = std::move( chunks );
            }

            VectorWriteImpl<Configured>& operator()(Arg<ChunkList> chunks) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                VectorWriteImpl<Configured>* r = new VectorWriteImpl<Configured>(this->file, NULL);
                r->SetArgs(chunks);
                return *r;
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
                    const ChunkList& chunks = _chunks.IsEmpty() ? params->GetArg<ChunksArg>(bucket) : _chunks.GetValue();
                    return this->file->VectorWrite(chunks, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                VectorWriteImpl<Handled>* r = new VectorWriteImpl<Handled>(this->file, std::move(h));
                r->SetArgs(_chunks);
                delete this;
                return r;
            }

            Arg<ChunkList> _chunks;
    };
    typedef VectorWriteImpl<Bare> VectorWrite;
    template <State state> const std::string VectorWriteImpl<state>::ChunksArg::key = "chunks";


    template <State state>
    class WriteVImpl: public FileOperation<state> {
        public:
            WriteVImpl(File *f): FileOperation<state>(f){}
            WriteVImpl(File &f): FileOperation<state>(&f){}
            WriteVImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}
            
            struct OffsetArg {
                static const std::string key;
                typedef uint64_t type;
            };

            struct IovArg {
                static const std::string key;
                typedef struct iovec* type;
            };

            struct IovcntArg {
                static const std::string key;
                typedef int type;
            };

            void SetArgs(Arg<uint64_t> &offset, Arg<struct iovec*> &iov, Arg<int> &iovcnt) {
                _offset = std::move( offset );
                _iov = std::move( iov );
                _iovcnt = std::move( iovcnt );
            }

            WriteVImpl<Configured>& operator()(Arg<uint64_t> offset, Arg<struct iovec*> iov, Arg<int> iovcnt) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                WriteVImpl<Configured>* r = new WriteVImpl<Configured>(this->file, NULL);
                r->SetArgs(offset, iov, iovcnt);
                return *r;
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
                    uint64_t offset = _offset.IsEmpty() ? params->GetArg<OffsetArg>(bucket) : _offset.GetValue();
                    const struct iovec* iov = _iov.IsEmpty() ? params->GetArg<IovArg>(bucket) : _iov.GetValue();
                    int iovcnt = _iovcnt.IsEmpty() ? params->GetArg<IovcntArg>(bucket) : _iovcnt.GetValue();
                    return this->file->WriteV(offset, iov, iovcnt, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }

            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                WriteVImpl<Handled>* r = new WriteVImpl<Handled>(this->file, std::move(h));
                r->SetArgs(_offset, _iov, _iovcnt);
                delete this;
                return r;
            }

            Arg<uint64_t> _offset;
            Arg<struct iovec*> _iov;
            Arg<int> _iovcnt;
    };
    typedef WriteVImpl<Bare> WriteV;
    template <State state> const std::string WriteVImpl<state>::OffsetArg::key = "offset";
    template <State state> const std::string WriteVImpl<state>::IovArg::key = "iov";
    template <State state> const std::string WriteVImpl<state>::IovcntArg::key = "iovcnt";


    template <State state>
    class FcntlImpl: public FileOperation<state> {
        public:
            FcntlImpl(File *f): FileOperation<state>(f){}
            FcntlImpl(File &f): FileOperation<state>(&f){}
            FcntlImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}
            
            struct BufferArg {
                static const std::string key;
                typedef Buffer type;
            };

            void SetArgs(Arg<Buffer> &arg) {
                _arg = std::move(arg);
            }

            FcntlImpl<Configured>& operator()(Arg<Buffer> arg) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                FcntlImpl<Configured>* r = new FcntlImpl<Configured>(this->file, NULL);
                r->SetArgs(arg);
                return *r;
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
                    const Buffer& arg = _arg.IsEmpty() ? params->GetArg<BufferArg>(bucket) : std::move(_arg.GetValue());
                    return this->file->Fcntl(arg, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                FcntlImpl<Handled>* r = new FcntlImpl<Handled>(this->file, std::move(h));
                r->SetArgs(_arg);
                delete this;
                return r;
            }

            Arg<Buffer> _arg;
    };
    typedef FcntlImpl<Bare> Fcntl;
    template <State state> const std::string FcntlImpl<state>::BufferArg::key = "arg";


    template <State state = Bare>
    class VisaImpl: public FileOperation<state> {
        public:
            VisaImpl(File *f): FileOperation<state>(f){}
            VisaImpl(File &f): FileOperation<state>(&f){}
            VisaImpl(File *f, std::unique_ptr<OperationHandler> h): FileOperation<state>(f, std::move(h)){}

            VisaImpl<Configured>& operator()(){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                VisaImpl<Configured> *c = new VisaImpl<Configured>(this->file, NULL);
                return *c;
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
                delete this;
                return c;
            }
    };
    typedef VisaImpl<Bare> Visa;

}



#endif // __XRD_CL_FILE_OPERATIONS_HH__

