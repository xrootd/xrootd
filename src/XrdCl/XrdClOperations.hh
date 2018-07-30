#ifndef __XRD_CL_OPERATIONS_HH__
#define __XRD_CL_OPERATIONS_HH__

#include <iostream>
#include <XrdCl/XrdClFile.hh>
#include <XrdCl/XrdClOperationParams.hh>

using namespace std;

namespace XrdCl {

    enum State {Bare, Configured, Handled};
    template <State state> class Operation;

    class ForwardingHandler: public ResponseHandler {
        public:
            ForwardingHandler(){
                container = new ParamsContainer();
            }

            ParamsContainer *GetParamsContainer(){
                return container;
            }

        protected:
            ParamsContainer *container;
    };


    class OperationHandler: public ResponseHandler {
        public:
            OperationHandler(ForwardingHandler *handler);
            void AddOperation(Operation<Handled> *op);
            void AddNextHandler(ForwardingHandler *handler);
            void SetSemaphore(XrdSysSemaphore *sem);
            ForwardingHandler* GetHandler();
            virtual void HandleResponseWithHosts(XRootDStatus *status, AnyObject *response, HostList *hostList);
            virtual void HandleResponse(XRootDStatus *status, AnyObject *response);
            virtual ~OperationHandler();

        protected:
            void RunNextOperation();

        private:
            ForwardingHandler *responseHandler;
            Operation<Handled> *nextOperation;
            XrdSysSemaphore *semaphore;
            ParamsContainer *params;

    };


    class Workflow {
        public:
            Workflow(Operation<Handled>& op);
            ~Workflow();
            Workflow& Run();
            void Wait();

        private:
            Operation<Handled> *firstOperation;
            XrdSysSemaphore *semaphore;
            ParamsContainer *firstOperationParams;
    };


    template <State state>
    class Operation {

        public:
            Operation(File *f){
                file = f;
                handler = NULL;
            }

            Operation(File *f, OperationHandler *h){
                file = f;
                handler = h;
            }

            virtual ~Operation(){
                if(handler){
                    delete handler;
                }
            }

            Operation<Handled>& operator>>(ForwardingHandler *h){
                static_assert(state == Configured, "Operator >> is available only for type Operation<Configured>");
                return *this;
            }

            virtual string GetName() const {
                return "Operation";
            }

            void AddOperation(Operation<Handled> *op){
                static_assert(state == Handled, "AddOperation method is available only for type Operation<Handled>");
            }
            
            Operation<Handled>& operator|(Operation<Handled> &op){
                static_assert(state == Handled, "Operator || is available only for type Operation<Handled>");
                return op;
            }

            void SetSemaphore(XrdSysSemaphore *sem){
                if(handler){
                    handler->SetSemaphore(sem);
                }
            }

            virtual XRootDStatus Run(ParamsContainer *params){
                cout<<"Running operation"<<endl;
                return XRootDStatus();
            }

        protected:     
            virtual Operation<Handled>* TransformToHandled(OperationHandler *h){
                Operation<Handled> *op = new Operation<Handled>(file, h);
                delete this;
                return op;
            }

            File *file;
            OperationHandler* handler;
    };

    template<> Operation<Handled>& Operation<Configured>::operator>>(ForwardingHandler *h){
        OperationHandler *handler = new OperationHandler(h);
        Operation<Handled> *op = this->TransformToHandled(handler);
        return *op;
    }

    template<> void Operation<Handled>::AddOperation(Operation<Handled> *op){
        if(handler){
            handler->AddOperation(op);
        }
    }

    template<> Operation<Handled>& Operation<Handled>::operator|(Operation<Handled> &op){
        AddOperation(&op);
        return *this;
    }


    template <State state>
    class OpenImpl: public Operation<state> {
        public:
            OpenImpl(File *f): Operation<state>(f){}
            OpenImpl(File *f, OperationHandler *h): Operation<state>(f, h){}
            
            void SetParams(OptionalParam<std::string> url, OptionalParam<OpenFlags::Flags> flags, OptionalParam<Access::Mode> mode = Access::None){
                _url = url;
                _flags = flags;
                _mode = mode;
            }

            OpenImpl<Configured>& operator()(OptionalParam<std::string> url, OptionalParam<OpenFlags::Flags> flags, OptionalParam<Access::Mode> mode = Access::None){
                OpenImpl<Configured>* o = new OpenImpl<Configured>(this->file, this->handler);
                o->SetParams(url, flags, mode);
                return *o;
            }
        
            string GetName() const {return "Open";}

        protected:
            XRootDStatus Run(ParamsContainer *params){
                cout<<"Running Open"<<endl;
                std::string url = _url.IsEmpty() ? params->GetParam<string>("url") : _url.GetValue();
                OpenFlags::Flags flags = _flags.IsEmpty() ? params->GetParam<OpenFlags::Flags>("flags") : _flags.GetValue();
                Access::Mode mode = _mode.IsEmpty() ? params->GetParam<Access::Mode>("mode") : _mode.GetValue();
                return this->file->Open(url, flags, mode, this->handler);
            }  

            Operation<Handled>* TransformToHandled(OperationHandler *h){
                OpenImpl<Handled>* o = new OpenImpl<Handled>(this->file, h);
                o->SetParams(_url, _flags, _mode);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _url; 
            OptionalParam<OpenFlags::Flags> _flags;
            OptionalParam<Access::Mode> _mode;
    };
    typedef OpenImpl<Bare> Open;

    template <State state>
    class ReadImpl: public Operation<state> {
        public:
            ReadImpl(File *f): Operation<state>(f){}
            ReadImpl(File *f, OperationHandler *h): Operation<state>(f, h){}
            
            void SetParams(OptionalParam<uint64_t> offset, OptionalParam<uint32_t> size, OptionalParam<void*> buffer) {
                _offset = offset;
                _size = size;
                _buffer = buffer;
            }

            ReadImpl<Configured>& operator()(OptionalParam<uint64_t> offset, OptionalParam<uint32_t> size, OptionalParam<void*> buffer) {
                ReadImpl<Configured>* r = new ReadImpl<Configured>(this->file, this->handler);
                r->SetParams(offset, size, buffer);
                return *r;
            }     

            string GetName() const {return "Read";}

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                uint64_t offset = _offset.IsEmpty() ? params->GetParam<uint64_t>("offset") : _offset.GetValue();
                uint32_t size = _size.IsEmpty() ? params->GetParam<uint32_t>("size") : _size.GetValue();
                void *buffer = _buffer.IsEmpty() ? params->GetPtrParam<char*>("buffer") : _buffer.GetValue();
                cout<<"Running Read, offset = "<<offset<<endl;
                return this->file->Read(offset, size, buffer, this->handler);
            }

            Operation<Handled>* TransformToHandled(OperationHandler *h){
                ReadImpl<Handled>* r = new ReadImpl<Handled>(this->file, h);
                r->SetParams(_offset, _size, _buffer);
                delete this;
                return r;
            }

            OptionalParam<uint64_t> _offset;
            OptionalParam<uint32_t> _size;
            OptionalParam<void*> _buffer;
    };
    typedef ReadImpl<Bare> Read;


    template <State state = Bare>
    class CloseImpl: public Operation<state> {
        public:
            CloseImpl(File *f): Operation<state>(f){}
            CloseImpl(File *f, OperationHandler *h): Operation<state>(f, h){}

            CloseImpl<Configured>& operator()(){
                CloseImpl<Configured> *c = new CloseImpl<Configured>(this->file, this->handler);
                return *c;
            }

            string GetName() const {return "Close";}

        protected:
            XRootDStatus Run(ParamsContainer *params){
                cout<<"Running Close"<<endl;
                return this->file->Close(this->handler);
            }

            Operation<Handled>* TransformToHandled(OperationHandler *h){
                CloseImpl<Handled> *c = new CloseImpl<Handled>(this->file, h);
                delete this;
                return c;
            }
    };
    typedef CloseImpl<Bare> Close;

    
    template <State state = Bare>
    class StatImpl: public Operation<state> {
        public:
            StatImpl(File *f): Operation<state>(f){}
            StatImpl(File *f, OperationHandler *h): Operation<state>(f, h){}

            void SetParams(OptionalParam<bool> force) {
                _force = force;
            }

            StatImpl<Configured>& operator()(OptionalParam<bool> force){
                StatImpl<Configured> *c = new StatImpl<Configured>(this->file, this->handler);
                c->SetParams(force);
                return *c;
            }
            
            string GetName() const {return "Stat";}

        protected:
            XRootDStatus Run(ParamsContainer *params){
                bool force = _force.IsEmpty() ? params->GetParam<bool>("force") : _force.GetValue();
                return this->file->Stat(force, this->handler);
            }

            Operation<Handled>* TransformToHandled(OperationHandler *h){
                StatImpl<Handled> *c = new StatImpl<Handled>(this->file, h);
                c->SetParams(_force);
                delete this;
                return c;
            }

            OptionalParam<bool> _force;
    };
    typedef StatImpl<Bare> Stat;


    template <State state>
    class WriteImpl: public Operation<state> {
        public:
            WriteImpl(File *f): Operation<state>(f){}
            WriteImpl(File *f, OperationHandler *h): Operation<state>(f, h){}
            
            void SetParams(OptionalParam<uint64_t> offset, OptionalParam<uint32_t> size, OptionalParam<void*> buffer) {
                _offset = offset;
                _size = size;
                _buffer = buffer;
            }

            WriteImpl<Configured>& operator()(OptionalParam<uint64_t> offset, OptionalParam<uint32_t> size, OptionalParam<void*> buffer) {
                WriteImpl<Configured>* r = new WriteImpl<Configured>(this->file, this->handler);
                r->SetParams(offset, size, buffer);
                return *r;
            }     

            string GetName() const {return "Write";}

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                uint64_t offset = _offset.IsEmpty() ? params->GetParam<uint64_t>("offset") : _offset.GetValue();
                uint32_t size = _size.IsEmpty() ? params->GetParam<uint32_t>("size") : _size.GetValue();
                void *buffer = _buffer.IsEmpty() ? params->GetPtrParam<char*>("buffer") : _buffer.GetValue();
                cout<<"Running Write"<<endl;
                return this->file->Write(offset, size, buffer, this->handler);
            }

            Operation<Handled>* TransformToHandled(OperationHandler *h){
                WriteImpl<Handled>* r = new WriteImpl<Handled>(this->file, h);
                r->SetParams(_offset, _size, _buffer);
                delete this;
                return r;
            }

            OptionalParam<uint64_t> _offset;
            OptionalParam<uint32_t> _size;
            OptionalParam<void*> _buffer;
    };
    typedef WriteImpl<Bare> Write;


    template <State state = Bare>
    class SyncImpl: public Operation<state> {
        public:
            SyncImpl(File *f): Operation<state>(f){}
            SyncImpl(File *f, OperationHandler *h): Operation<state>(f, h){}

            SyncImpl<Configured>& operator()(){
                SyncImpl<Configured> *c = new SyncImpl<Configured>(this->file, this->handler);
                return *c;
            }

            string GetName() const {return "Sync";}

        protected:
            XRootDStatus Run(ParamsContainer *params){
                cout<<"Running Sync"<<endl;
                return this->file->Sync(this->handler);
            }

            Operation<Handled>* TransformToHandled(OperationHandler *h){
                SyncImpl<Handled> *c = new SyncImpl<Handled>(this->file, h);
                delete this;
                return c;
            }
    };
    typedef SyncImpl<Bare> Sync;


    template <State state>
    class TruncateImpl: public Operation<state> {
        public:
            TruncateImpl(File *f): Operation<state>(f){}
            TruncateImpl(File *f, OperationHandler *h): Operation<state>(f, h){}
            
            void SetParams(OptionalParam<uint64_t> size) {
                _size = size;
            }

            TruncateImpl<Configured>& operator()(OptionalParam<uint64_t> size) {
                TruncateImpl<Configured>* r = new TruncateImpl<Configured>(this->file, this->handler);
                r->SetParams(size);
                return *r;
            }     

            string GetName() const {return "Write";}

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                uint32_t size = _size.IsEmpty() ? params->GetParam<uint64_t>("size") : _size.GetValue();
                cout<<"Running Truncate, size = "<<size<<endl;
                return this->file->Truncate(size, this->handler);
            }

            Operation<Handled>* TransformToHandled(OperationHandler *h){
                TruncateImpl<Handled>* r = new TruncateImpl<Handled>(this->file, h);
                r->SetParams(_size);
                delete this;
                return r;
            }

            OptionalParam<uint64_t> _size;
    };
    typedef TruncateImpl<Bare> Truncate;


    template <State state>
    class VectorReadImpl: public Operation<state> {
        public:
            VectorReadImpl(File *f): Operation<state>(f){}
            VectorReadImpl(File *f, OperationHandler *h): Operation<state>(f, h){}
            
            void SetParams(OptionalParam<ChunkList> chunks, OptionalParam<void*> buffer) {
                _chunks = chunks;
                _buffer = buffer;
            }

            VectorReadImpl<Configured>& operator()(OptionalParam<ChunkList> chunks, OptionalParam<void*> buffer) {
                VectorReadImpl<Configured>* r = new VectorReadImpl<Configured>(this->file, this->handler);
                r->SetParams(chunks, buffer);
                return *r;
            }

            string GetName() const {return "VectorRead";}

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                const ChunkList& chunks = _chunks.IsEmpty() ? params->GetParam<ChunkList>("chunks") : _chunks.GetValue();
                void *buffer = _buffer.IsEmpty() ? params->GetPtrParam<char*>("buffer") : _buffer.GetValue();
                cout<<"Running VectorRead"<<endl;
                return this->file->VectorRead(chunks, buffer, this->handler);
            }

            Operation<Handled>* TransformToHandled(OperationHandler *h){
                VectorReadImpl<Handled>* r = new VectorReadImpl<Handled>(this->file, h);
                r->SetParams(_chunks, _buffer);
                delete this;
                return r;
            }

            OptionalParam<ChunkList> _chunks;
            OptionalParam<void*> _buffer;
    };
    typedef VectorReadImpl<Bare> VectorRead;


    template <State state>
    class VectorWriteImpl: public Operation<state> {
        public:
            VectorWriteImpl(File *f): Operation<state>(f){}
            VectorWriteImpl(File *f, OperationHandler *h): Operation<state>(f, h){}
            
            void SetParams(OptionalParam<ChunkList> chunks) {
                _chunks = chunks;
            }

            VectorWriteImpl<Configured>& operator()(OptionalParam<ChunkList> chunks) {
                VectorWriteImpl<Configured>* r = new VectorWriteImpl<Configured>(this->file, this->handler);
                r->SetParams(chunks);
                return *r;
            }     

            string GetName() const {return "VectorWrite";}

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                const ChunkList& chunks = _chunks.IsEmpty() ? params->GetParam<ChunkList>("chunks") : _chunks.GetValue();
                cout<<"Running VectorWrite"<<endl;
                return this->file->VectorWrite(chunks, this->handler);
            }

            Operation<Handled>* TransformToHandled(OperationHandler *h){
                VectorWriteImpl<Handled>* r = new VectorWriteImpl<Handled>(this->file, h);
                r->SetParams(_chunks);
                delete this;
                return r;
            }

            OptionalParam<ChunkList> _chunks;
    };
    typedef VectorWriteImpl<Bare> VectorWrite;


    template <State state>
    class WriteVImpl: public Operation<state> {
        public:
            WriteVImpl(File *f): Operation<state>(f){}
            WriteVImpl(File *f, OperationHandler *h): Operation<state>(f, h){}
            
            void SetParams(OptionalParam<uint64_t> offset, OptionalParam<struct iovec*> iov, OptionalParam<int> iovcnt) {
                _offset = offset;
                _iov = iov;
                _iovcnt = iovcnt;
            }

            WriteVImpl<Configured>& operator()(OptionalParam<uint64_t> offset, OptionalParam<struct iovec*> iov, OptionalParam<int> iovcnt) {
                WriteVImpl<Configured>* r = new WriteVImpl<Configured>(this->file, this->handler);
                r->SetParams(offset, iov, iovcnt);
                return *r;
            }     

            string GetName() const {return "WriteV";}

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                uint64_t offset = _offset.IsEmpty() ? params->GetParam<uint64_t>("offset") : _offset.GetValue();
                const struct iovec* iov = _iov.IsEmpty() ? params->GetPtrParam<struct iovec*>("iov") : _iov.GetValue();
                int iovcnt = _iovcnt.IsEmpty() ? params->GetParam<int>("iovcnt") : _iovcnt.GetValue();
                cout<<"Running WriteV"<<endl;

                char* ptr = (char*) iov[1].iov_base;
                cout<<"iov data:  ";
                while(*ptr != '\0'){
                    cout<<*ptr;
                    ptr++;
                }
                cout<<endl;

                return this->file->WriteV(offset, iov, iovcnt, this->handler);
            }

            Operation<Handled>* TransformToHandled(OperationHandler *h){
                WriteVImpl<Handled>* r = new WriteVImpl<Handled>(this->file, h);
                r->SetParams(_offset, _iov, _iovcnt);
                delete this;
                return r;
            }

            OptionalParam<uint64_t> _offset;
            OptionalParam<struct iovec*> _iov;
            OptionalParam<int> _iovcnt;
    };
    typedef WriteVImpl<Bare> WriteV;


    template <State state>
    class FcntlImpl: public Operation<state> {
        public:
            FcntlImpl(File *f): Operation<state>(f){}
            FcntlImpl(File *f, OperationHandler *h): Operation<state>(f, h){}
            
            void SetParams(OptionalParam<Buffer> arg) {
                _arg = arg;
            }

            FcntlImpl<Configured>& operator()(OptionalParam<Buffer> arg) {
                FcntlImpl<Configured>* r = new FcntlImpl<Configured>(this->file, this->handler);
                r->SetParams(arg);
                return *r;
            }     

            string GetName() const {return "Fcntl";}

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                const Buffer& arg = _arg.IsEmpty() ? params->GetParam<Buffer>("arg") : _arg.GetValue();
                cout<<"Running Fcntl"<<endl;
                return this->file->Fcntl(arg, this->handler);
            }

            Operation<Handled>* TransformToHandled(OperationHandler *h){
                FcntlImpl<Handled>* r = new FcntlImpl<Handled>(this->file, h);
                r->SetParams(_arg);
                delete this;
                return r;
            }

            OptionalParam<Buffer> _arg;
    };
    typedef FcntlImpl<Bare> Fcntl;


    template <State state = Bare>
    class VisaImpl: public Operation<state> {
        public:
            VisaImpl(File *f): Operation<state>(f){}
            VisaImpl(File *f, OperationHandler *h): Operation<state>(f, h){}

            VisaImpl<Configured>& operator()(){
                VisaImpl<Configured> *c = new VisaImpl<Configured>(this->file, this->handler);
                return *c;
            }

            string GetName() const {return "Close";}

        protected:
            XRootDStatus Run(ParamsContainer *params){
                cout<<"Running Close"<<endl;
                return this->file->Visa(this->handler);
            }

            Operation<Handled>* TransformToHandled(OperationHandler *h){
                VisaImpl<Handled> *c = new VisaImpl<Handled>(this->file, h);
                delete this;
                return c;
            }
    };
    typedef VisaImpl<Bare> Visa;

}


#endif // __XRD_CL_OPERATIONS_HH__
