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

#ifndef __XRD_CL_OPERATIONS_HH__
#define __XRD_CL_OPERATIONS_HH__

#include <XrdCl/XrdClFile.hh>
#include <XrdCl/XrdClOperationParams.hh>

namespace XrdCl {

    enum State {Bare, Configured, Handled};
    template <State state> class Operation;

    //---------------------------------------------------------------------------
    //! Handler allowing forwarding parameters to the next operation in workflow
    //---------------------------------------------------------------------------
    class ForwardingHandler: public ResponseHandler {
        public:
            ForwardingHandler(){
                container = new ParamsContainer();
            }

            ParamsContainer *GetParamsContainer(){
                return container;
            }

            virtual void HandleResponseWithHosts(XRootDStatus *status, AnyObject *response, HostList *hostList){
                if(hostList){delete hostList;}
                HandleResponse(status, response);
            }

            virtual void HandleResponse(XRootDStatus *status, AnyObject *response){
                if(status){delete status;}
                if(response){delete response;}
            }


        protected:
            ParamsContainer *container;
    };

    //---------------------------------------------------------------------------
    //! File operations workflow
    //---------------------------------------------------------------------------   
    class Workflow {
        friend class OperationHandler;

        public:
            //------------------------------------------------------------------------
            //! Constructor
            //!
            //! @param op first operation of the sequence
            //------------------------------------------------------------------------            
            Workflow(Operation<Handled>& op);
            ~Workflow();

            //------------------------------------------------------------------------
            //! Run first workflow operation
            //!
            //! @return original workflow object
            //------------------------------------------------------------------------            
            Workflow& Run();

            //------------------------------------------------------------------------
            //! Wait for workflow execution end
            //------------------------------------------------------------------------
            void Wait();

            //------------------------------------------------------------------------
            //! Get workflow execution status
            //! 
            //! @return workflow execution status if available, otherwise default 
            //! XRootDStatus object
            //------------------------------------------------------------------------
            XRootDStatus GetStatus();

        private:
            //------------------------------------------------------------------------
            //! Release the semaphore and save status
            //! 
            //! @param lastOperationStatus status of last executed operation.
            //!                    It is set as status of workflow execution.
            //------------------------------------------------------------------------
            void EndWorkflowExecution(XRootDStatus *lastOperationStatus);

            Operation<Handled> *firstOperation;
            XrdSysSemaphore *semaphore;
            ParamsContainer *firstOperationParams;
            XRootDStatus *status;
    };

    //---------------------------------------------------------------------------
    //! Wrapper for ForwardingHandler, used only internally to run next operation
    //! after previous one is finished
    //---------------------------------------------------------------------------
    class OperationHandler: public ResponseHandler {
        public:
            OperationHandler(ForwardingHandler *handler);
            virtual void HandleResponseWithHosts(XRootDStatus *status, AnyObject *response, HostList *hostList);
            virtual void HandleResponse(XRootDStatus *status, AnyObject *response);
            virtual ~OperationHandler();

            //------------------------------------------------------------------------
            //! Add new operation to the sequence
            //!
            //! @param operation    operation to add
            //------------------------------------------------------------------------
            void AddOperation(Operation<Handled> *operation);
            
            //------------------------------------------------------------------------
            //! Set workflow to this and all next handlers. In the last handler
            //! it is used to finish workflow execution
            //!
            //! @param wf   workflow to set
            //------------------------------------------------------------------------
            void AssignToWorkflow(Workflow *wf);

        protected:
            //------------------------------------------------------------------------
            //! Run next operation if it is available, otherwise end workflow
            //------------------------------------------------------------------------
            void RunNextOperation();

        private:
            //------------------------------------------------------------------------
            //! Log information about negative operation status and end workflow
            //!
            //! @param status   status of the operation
            //------------------------------------------------------------------------
            void HandleFailedOperationStatus(XRootDStatus *status, AnyObject *response);


            ForwardingHandler *responseHandler;
            Operation<Handled> *nextOperation;
            XrdSysSemaphore *semaphore;
            Workflow *workflow;
            ParamsContainer *params;

    };

    //----------------------------------------------------------------------
    //! File operation template
    //----------------------------------------------------------------------
    template <State state>
    class Operation {

        public:
            //------------------------------------------------------------------
            //! Constructor
            //!
            //! @param f  file on which operation will be performed
            //------------------------------------------------------------------
            Operation(File *f): handler(NULL){
                file = f;
            }

            //------------------------------------------------------------------
            //! Constructor (used internally to change copy object with 
            //! change of template parameter)
            //!
            //! @param f  file on which operation will be performed
            //! @param h  operation handler
            //------------------------------------------------------------------
            Operation(File *f, OperationHandler *h){
                file = f;
                handler = h;
            }

            virtual ~Operation(){
                if(handler){
                    delete handler;
                }
            }

            //------------------------------------------------------------------
            //! Add handler which will be executed after operation ends
            //!
            //! @param h  handler to add
            //------------------------------------------------------------------
            Operation<Handled>& operator>>(ForwardingHandler *h){
                static_assert(state == Configured, "Operator >> is available only for type Operation<Configured>");
                return *this;
            }

            //------------------------------------------------------------------
            //! Add next operation to the handler
            //!
            //! @param op  operation to add
            //------------------------------------------------------------------
            void AddOperation(Operation<Handled> *op){
                static_assert(state == Handled, "AddOperation method is available only for type Operation<Handled>");
            }
            
            //------------------------------------------------------------------
            //! Add operation to handler
            //!
            //! @param op   operation to add
            //! @return     handled operation
            //------------------------------------------------------------------
            Operation<Handled>& operator|(Operation<Handled> &op){
                static_assert(state == Handled, "Operator || is available only for type Operation<Handled>");
                return op;
            }

            //------------------------------------------------------------------
            //! Set workflow pointer in the handler
            //!
            //! @param wf   workflow to set
            //------------------------------------------------------------------
            void AssignToWorkflow(Workflow *wf){
                if(handler){
                    handler->AssignToWorkflow(wf);
                }
            }

            //------------------------------------------------------------------
            //! Run operation
            //!
            //! @param params   container with parameters forwarded from
            //!                 previous operation
            //! @return         status of the operation
            //------------------------------------------------------------------
            virtual XRootDStatus Run(ParamsContainer *params) = 0;

            //------------------------------------------------------------------
            //! Handle error caused by missing parameter
            //!
            //! @param err  error object
            //! @return     default operation status (actual status containg
            //!             error information is passed to the handler)
            //------------------------------------------------------------------
            virtual XRootDStatus HandleError(std::logic_error err){
                XRootDStatus *st = new XRootDStatus(stError, err.what());
                handler->HandleResponse(st, 0);
                return XRootDStatus();
            }

        protected:     
            //------------------------------------------------------------------
            //! Save handler and change template type to handled
            //!
            //! @param h    handler object
            //! @return     handled operation
            //------------------------------------------------------------------
            virtual Operation<Handled>* TransformToHandled(OperationHandler *h) = 0;

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

        protected:
            XRootDStatus Run(ParamsContainer *params){
                try{
                    std::string url = _url.IsEmpty() ? params->GetParam<std::string>("url") : _url.GetValue();
                    OpenFlags::Flags flags = _flags.IsEmpty() ? params->GetParam<OpenFlags::Flags>("flags") : _flags.GetValue();
                    Access::Mode mode = _mode.IsEmpty() ? params->GetParam<Access::Mode>("mode") : _mode.GetValue();
                    return this->file->Open(url, flags, mode, this->handler);
                } catch(std::logic_error err){
                    return this->HandleError(err);
                }
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

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                try {
                    uint64_t offset = _offset.IsEmpty() ? params->GetParam<uint64_t>("offset") : _offset.GetValue();
                    uint32_t size = _size.IsEmpty() ? params->GetParam<uint32_t>("size") : _size.GetValue();
                    void *buffer = _buffer.IsEmpty() ? params->GetPtrParam<char*>("buffer") : _buffer.GetValue();
                    return this->file->Read(offset, size, buffer, this->handler);
                } catch(std::logic_error err){
                    return this->HandleError(err);
                }        
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

        protected:
            XRootDStatus Run(ParamsContainer *params){
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

        protected:
            XRootDStatus Run(ParamsContainer *params){
                try {
                    bool force = _force.IsEmpty() ? params->GetParam<bool>("force") : _force.GetValue();
                    return this->file->Stat(force, this->handler);
                } catch(std::logic_error err){
                    return this->HandleError(err);
                }
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

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                try {
                    uint64_t offset = _offset.IsEmpty() ? params->GetParam<uint64_t>("offset") : _offset.GetValue();
                    uint32_t size = _size.IsEmpty() ? params->GetParam<uint32_t>("size") : _size.GetValue();
                    void *buffer = _buffer.IsEmpty() ? params->GetPtrParam<char*>("buffer") : _buffer.GetValue();
                    return this->file->Write(offset, size, buffer, this->handler);
                } catch(std::logic_error err){
                    return this->HandleError(err);
                }
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

        protected:
            XRootDStatus Run(ParamsContainer *params){
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

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                try {
                    uint32_t size = _size.IsEmpty() ? params->GetParam<uint64_t>("size") : _size.GetValue();
                    return this->file->Truncate(size, this->handler);
                } catch(std::logic_error err){
                    return this->HandleError(err);
                }
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

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                try {
                    const ChunkList& chunks = _chunks.IsEmpty() ? params->GetParam<ChunkList>("chunks") : _chunks.GetValue();
                    void *buffer = _buffer.IsEmpty() ? params->GetPtrParam<char*>("buffer") : _buffer.GetValue();
                    return this->file->VectorRead(chunks, buffer, this->handler);
                } catch(std::logic_error err){
                    return this->HandleError(err);
                }
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

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                try {
                    const ChunkList& chunks = _chunks.IsEmpty() ? params->GetParam<ChunkList>("chunks") : _chunks.GetValue();
                    return this->file->VectorWrite(chunks, this->handler);
                } catch(std::logic_error err){
                    return this->HandleError(err);
                }
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

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                try {                    
                    uint64_t offset = _offset.IsEmpty() ? params->GetParam<uint64_t>("offset") : _offset.GetValue();
                    const struct iovec* iov = _iov.IsEmpty() ? params->GetPtrParam<struct iovec*>("iov") : _iov.GetValue();
                    int iovcnt = _iovcnt.IsEmpty() ? params->GetParam<int>("iovcnt") : _iovcnt.GetValue();
                    return this->file->WriteV(offset, iov, iovcnt, this->handler);
                } catch(std::logic_error err){
                    return this->HandleError(err);
                }

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

        protected:    
            XRootDStatus Run(ParamsContainer *params){
                try {
                    const Buffer& arg = _arg.IsEmpty() ? params->GetParam<Buffer>("arg") : _arg.GetValue();
                    return this->file->Fcntl(arg, this->handler);
                } catch(std::logic_error err){
                    return this->HandleError(err);
                }
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

        protected:
            XRootDStatus Run(ParamsContainer *params){
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
