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

#include <iostream>
#include <memory>
#include <stdexcept>
#include <sstream>
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClOperationParams.hh"

namespace XrdCl {

    enum State {Bare, Configured, Handled};
    template <State state> class Operation;

    //---------------------------------------------------------------------------
    //! Handler allowing forwarding parameters to the next operation in workflow
    //---------------------------------------------------------------------------
    class ForwardingHandler: public ResponseHandler {
        friend class OperationHandler;
        
        public:
            ForwardingHandler(): responseHandler(NULL), wrapper(false){
                container = std::shared_ptr<ParamsContainer>(new ParamsContainer());
            }

            ForwardingHandler(ResponseHandler *handler): responseHandler(handler), wrapper(true){
                container = std::shared_ptr<ParamsContainer>(new ParamsContainer());
            }

            virtual void HandleResponseWithHosts(XRootDStatus *status, AnyObject *response, HostList *hostList){
                if(wrapper){
                    responseHandler->HandleResponseWithHosts(status, response, hostList);
                } else {
                    CleanMemory(status, response, hostList);
                }
                delete this;
            }

            virtual void HandleResponse(XRootDStatus *status, AnyObject *response){
                if(wrapper){
                    responseHandler->HandleResponse(status, response);
                } else {
                    CleanMemory(status, response, NULL);
                }
                delete this;
            }

            template <typename T>
            void ForwardParam(typename T::type value, int bucket = 1){
                container->SetParam<T>(value, bucket);
            }

        private:
            void CleanMemory(XRootDStatus *status, AnyObject *response, HostList *hostList){
                delete status;
                delete response;
                delete hostList;
            }

            std::shared_ptr<ParamsContainer> GetParamsContainer(){
                return container;
            }

            std::shared_ptr<ParamsContainer> container;

        protected:
            ResponseHandler* responseHandler;
            bool wrapper;
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
            Workflow(Operation<Handled>& op, bool enableLogging = true);

            //------------------------------------------------------------------------
            //! Constructor
            //!
            //! @param op first operation of the sequence
            //------------------------------------------------------------------------
            Workflow(Operation<Handled>* op, bool enableLogging = true);

            ~Workflow();

            //------------------------------------------------------------------------
            //! Run first workflow operation
            //!
            //! @return original workflow object
            //------------------------------------------------------------------------
            Workflow& Run(std::shared_ptr<ParamsContainer> params = NULL, int bucket = 1);

            //------------------------------------------------------------------------
            //! Wait for workflow execution end
            //!
            //! @return original workflow object
            //------------------------------------------------------------------------
            Workflow& Wait();

            //------------------------------------------------------------------------
            //! Get workflow execution status
            //! 
            //! @return workflow execution status if available, otherwise default 
            //! XRootDStatus object
            //------------------------------------------------------------------------
            XRootDStatus GetStatus();

            //------------------------------------------------------------------
            //! Get workflow description
            //!
            //! @return description of the workflow
            //------------------------------------------------------------------
            std::string ToString();

            //------------------------------------------------------------------
            //! Add operation description to the descriptions list
            //! 
            //! @param description description of assigned operation
            //------------------------------------------------------------------
            void AddOperationInfo(std::string description);

            //------------------------------------------------------------------
            //! Log operation descriptions
            //------------------------------------------------------------------
            void Print();

        private:
            //------------------------------------------------------------------------
            //! Release the semaphore and save status
            //! 
            //! @param lastOperationStatus status of last executed operation.
            //!                    It is set as status of workflow execution.
            //------------------------------------------------------------------------
            void EndWorkflowExecution(XRootDStatus *lastOperationStatus);

            Operation<Handled> *firstOperation;
            std::unique_ptr<XrdSysSemaphore> semaphore;
            XRootDStatus *status;
            std::list<std::string> operationDescriptions;
            bool logging;
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
            Workflow *workflow;
            std::shared_ptr<ParamsContainer> params;
    };

    //----------------------------------------------------------------------
    //! File operation template
    //! 
    //! @tparam state   describes current operation configuration state
    //----------------------------------------------------------------------
    template <State state>
    class Operation {
        // Declare friendship between templates
        template<State> friend class Operation;

        friend class Workflow;
        friend class OperationHandler;

        public:
            //------------------------------------------------------------------
            //! Constructor
            //!
            //! @param f  file on which operation will be performed
            //------------------------------------------------------------------
            Operation(File *f): file(f){                
                static_assert(state == Bare, "Constructor is available only for type Operation<Bare>");
                handler = NULL;
            }

            virtual ~Operation(){}

            //------------------------------------------------------------------
            //! Add handler which will be executed after operation ends
            //!
            //! @param h  handler to add
            //------------------------------------------------------------------
            Operation<Handled>& operator>>(ForwardingHandler *h){
                static_assert(state == Configured, "Operator >> is available only for type Operation<Configured>");
                auto handler = std::unique_ptr<OperationHandler>(new OperationHandler(h));
                Operation<Handled> *op = this->TransformToHandled(std::move(handler));
                return *op;
            }

            //------------------------------------------------------------------
            //! Add handler which will be executed after operation ends
            //!
            //! @param h  handler to add
            //------------------------------------------------------------------
            Operation<Handled>& operator>>(ResponseHandler *h){
                static_assert(state == Configured, "Operator >> is available only for type Operation<Configured>");
                ForwardingHandler *forwardingHandler = new ForwardingHandler(h);
                auto handler = std::unique_ptr<OperationHandler>(new OperationHandler(forwardingHandler));
                Operation<Handled> *op = this->TransformToHandled(std::move(handler));
                return *op;
            }
            
            //------------------------------------------------------------------
            //! Add operation to handler
            //!
            //! @param op   operation to add
            //! @return     handled operation
            //------------------------------------------------------------------
            Operation<Handled>& operator|(Operation<Handled> &op){
                static_assert(state == Handled, "Operator || is available only for type Operation<Handled>");
                AddOperation(&op);
                return *this;
            }

            virtual std::string ToString() = 0;

        protected:     
            //------------------------------------------------------------------
            //! Constructor (used internally to change copy object with 
            //! change of template parameter)
            //!
            //! @param f  file on which operation will be performed
            //! @param h  operation handler
            //------------------------------------------------------------------
            Operation(File *f, std::unique_ptr<OperationHandler> h): file(f){
                handler = std::move(h);
            }

            //------------------------------------------------------------------
            //! Save handler and change template type to handled
            //!
            //! @param h    handler object
            //! @return     handled operation
            //------------------------------------------------------------------
            virtual Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h) = 0;

            //------------------------------------------------------------------
            //! Set workflow pointer in the handler
            //!
            //! @param wf   workflow to set
            //------------------------------------------------------------------
            void AssignToWorkflow(Workflow *wf){
                static_assert(state == Handled, "Only Operation<Handled> can be assigned to workflow");
                wf->AddOperationInfo(ToString());
                handler->AssignToWorkflow(wf);
            }

            //------------------------------------------------------------------
            //! Run operation
            //!
            //! @param params   container with parameters forwarded from
            //!                 previous operation
            //! @return         status of the operation
            //------------------------------------------------------------------
            virtual XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucket = 1) = 0;

            //------------------------------------------------------------------
            //! Handle error caused by missing parameter
            //!
            //! @param err  error object
            //! @return     default operation status (actual status containg
            //!             error information is passed to the handler)
            //------------------------------------------------------------------
            virtual XRootDStatus HandleError(const std::logic_error& err){
                XRootDStatus *st = new XRootDStatus(stError, err.what());
                handler->HandleResponse(st, 0);
                return XRootDStatus();
            }

            //------------------------------------------------------------------
            //! Add next operation to the handler
            //!
            //! @param op  operation to add
            //------------------------------------------------------------------
            void AddOperation(Operation<Handled> *op){
                static_assert(state == Handled, "AddOperation method is available only for type Operation<Handled>");
                if(handler){
                    handler->AddOperation(op);
                }
            }

            File *file;
            std::unique_ptr<OperationHandler> handler;
    };


    template <State state>
    class OpenImpl: public Operation<state> {
        public:
            OpenImpl(File *f): Operation<state>(f){}
            OpenImpl(File &f): Operation<state>(&f){}
            OpenImpl(File *f, std::unique_ptr<OperationHandler> h): Operation<state>(f, std::move(h)){}
            
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

            void SetParams(OptionalParam<std::string> url, OptionalParam<OpenFlags::Flags> flags, OptionalParam<Access::Mode> mode = Access::None){
                _url = url;
                _flags = flags;
                _mode = mode;
            }

            OpenImpl<Configured>& operator()(OptionalParam<std::string> url, OptionalParam<OpenFlags::Flags> flags, OptionalParam<Access::Mode> mode = Access::None){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                OpenImpl<Configured>* o = new OpenImpl<Configured>(this->file, NULL);
                o->SetParams(url, flags, mode);
                return *o;
            }

            std::string ToString(){
                return "Open";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucket = 1){
                try{
                    std::string url = _url.IsEmpty() ? params->GetParam<UrlArg>(bucket) : _url.GetValue();
                    OpenFlags::Flags flags = _flags.IsEmpty() ? params->GetParam<FlagsArg>(bucket) : _flags.GetValue();
                    Access::Mode mode = _mode.IsEmpty() ? params->GetParam<ModeArg>(bucket) : _mode.GetValue();
                    return this->file->Open(url, flags, mode, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                OpenImpl<Handled>* o = new OpenImpl<Handled>(this->file, std::move(h));
                o->SetParams(_url, _flags, _mode);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _url; 
            OptionalParam<OpenFlags::Flags> _flags;
            OptionalParam<Access::Mode> _mode;
    };
    typedef OpenImpl<Bare> Open;
    template <State state> const std::string OpenImpl<state>::UrlArg::key = "url";
    template <State state> const std::string OpenImpl<state>::FlagsArg::key = "flags";
    template <State state> const std::string OpenImpl<state>::ModeArg::key = "mode";


    template <State state>
    class ReadImpl: public Operation<state> {
        public:
            ReadImpl(File *f): Operation<state>(f){}
            ReadImpl(File &f): Operation<state>(&f){}
            ReadImpl(File *f, std::unique_ptr<OperationHandler> h): Operation<state>(f, std::move(h)){}

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

            void SetParams(OptionalParam<uint64_t> offset, OptionalParam<uint32_t> size, OptionalParam<void*> buffer) {
                _offset = offset;
                _size = size;
                _buffer = buffer;
            }

            ReadImpl<Configured>& operator()(OptionalParam<uint64_t> offset, OptionalParam<uint32_t> size, OptionalParam<void*> buffer) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                ReadImpl<Configured>* r = new ReadImpl<Configured>(this->file, NULL);
                r->SetParams(offset, size, buffer);
                return *r;
            }     

            std::string ToString(){
                return "Read";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucket = 1){
                try {
                    uint64_t offset = _offset.IsEmpty() ? params->GetParam<OffsetArg>(bucket) : _offset.GetValue();
                    uint32_t size = _size.IsEmpty() ? params->GetParam<SizeArg>(bucket) : _size.GetValue();
                    void *buffer = _buffer.IsEmpty() ? params->GetParam<BufferArg>(bucket) : _buffer.GetValue();
                    return this->file->Read(offset, size, buffer, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }        
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                ReadImpl<Handled>* r = new ReadImpl<Handled>(this->file, std::move(h));
                r->SetParams(_offset, _size, _buffer);
                delete this;
                return r;
            }

            OptionalParam<uint64_t> _offset;
            OptionalParam<uint32_t> _size;
            OptionalParam<void*> _buffer;
    };
    typedef ReadImpl<Bare> Read;
    template <State state> const std::string ReadImpl<state>::OffsetArg::key = "offset";
    template <State state> const std::string ReadImpl<state>::SizeArg::key = "size";
    template <State state> const std::string ReadImpl<state>::BufferArg::key = "buffer";


    template <State state = Bare>
    class CloseImpl: public Operation<state> {
        public:
            CloseImpl(File *f): Operation<state>(f){}
            CloseImpl(File &f): Operation<state>(&f){}
            CloseImpl(File *f, std::unique_ptr<OperationHandler> h): Operation<state>(f, std::move(h)){}

            CloseImpl<Configured>& operator()(){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                CloseImpl<Configured> *c = new CloseImpl<Configured>(this->file, NULL);
                return *c;
            }

            std::string ToString(){
                return "Close";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucket = 1){
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
    class StatImpl: public Operation<state> {
        public:
            StatImpl(File *f): Operation<state>(f){}
            StatImpl(File &f): Operation<state>(&f){}
            StatImpl(File *f, std::unique_ptr<OperationHandler> h): Operation<state>(f, std::move(h)){}

            struct ForceArg {
                static const std::string key;
                typedef bool type;
            };


            void SetParams(OptionalParam<bool> force) {
                _force = force;
            }

            StatImpl<Configured>& operator()(OptionalParam<bool> force){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                StatImpl<Configured> *c = new StatImpl<Configured>(this->file, NULL);
                c->SetParams(force);
                return *c;
            }

            std::string ToString(){
                return "Stat";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucket = 1){
                try {
                    bool force = _force.IsEmpty() ? params->GetParam<ForceArg>(bucket) : _force.GetValue();
                    return this->file->Stat(force, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                StatImpl<Handled> *c = new StatImpl<Handled>(this->file, std::move(h));
                c->SetParams(_force);
                delete this;
                return c;
            }

            OptionalParam<bool> _force;
    };
    typedef StatImpl<Bare> Stat;
    template <State state> const std::string StatImpl<state>::ForceArg::key = "force";


    template <State state>
    class WriteImpl: public Operation<state> {
        public:
            WriteImpl(File *f): Operation<state>(f){}
            WriteImpl(File &f): Operation<state>(&f){}
            WriteImpl(File *f, std::unique_ptr<OperationHandler> h): Operation<state>(f, std::move(h)){}
            
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

            void SetParams(OptionalParam<uint64_t> offset, OptionalParam<uint32_t> size, OptionalParam<void*> buffer) {
                _offset = offset;
                _size = size;
                _buffer = buffer;
            }

            WriteImpl<Configured>& operator()(OptionalParam<uint64_t> offset, OptionalParam<uint32_t> size, OptionalParam<void*> buffer) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                WriteImpl<Configured>* r = new WriteImpl<Configured>(this->file, NULL);
                r->SetParams(offset, size, buffer);
                return *r;
            }     

            std::string ToString(){
                return "Write";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucket = 1){
                try {
                    uint64_t offset = _offset.IsEmpty() ? params->GetParam<OffsetArg>(bucket) : _offset.GetValue();
                    uint32_t size = _size.IsEmpty() ? params->GetParam<SizeArg>(bucket) : _size.GetValue();
                    void *buffer = _buffer.IsEmpty() ? params->GetParam<BufferArg>(bucket) : _buffer.GetValue();
                    return this->file->Write(offset, size, buffer, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                WriteImpl<Handled>* r = new WriteImpl<Handled>(this->file, std::move(h));
                r->SetParams(_offset, _size, _buffer);
                delete this;
                return r;
            }

            OptionalParam<uint64_t> _offset;
            OptionalParam<uint32_t> _size;
            OptionalParam<void*> _buffer;
    };
    typedef WriteImpl<Bare> Write;
    template <State state> const std::string WriteImpl<state>::OffsetArg::key = "offset";
    template <State state> const std::string WriteImpl<state>::SizeArg::key = "size";
    template <State state> const std::string WriteImpl<state>::BufferArg::key = "buffer";


    template <State state = Bare>
    class SyncImpl: public Operation<state> {
        public:
            SyncImpl(File *f): Operation<state>(f){}
            SyncImpl(File &f): Operation<state>(&f){}
            SyncImpl(File *f, std::unique_ptr<OperationHandler> h): Operation<state>(f, std::move(h)){}

            SyncImpl<Configured>& operator()(){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                SyncImpl<Configured> *c = new SyncImpl<Configured>(this->file, NULL);
                return *c;
            }

            std::string ToString(){
                return "Sync";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucket = 1){
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
    class TruncateImpl: public Operation<state> {
        public:
            TruncateImpl(File *f): Operation<state>(f){}
            TruncateImpl(File &f): Operation<state>(&f){}
            TruncateImpl(File *f, std::unique_ptr<OperationHandler> h): Operation<state>(f, std::move(h)){}
            
            struct SizeArg {
                static const std::string key;
                typedef uint64_t type;
            };

            void SetParams(OptionalParam<uint64_t> size) {
                _size = size;
            }

            TruncateImpl<Configured>& operator()(OptionalParam<uint64_t> size) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                TruncateImpl<Configured>* r = new TruncateImpl<Configured>(this->file, NULL);
                r->SetParams(size);
                return *r;
            }     

            std::string ToString(){
                return "Truncate";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucket = 1){
                try {
                    uint32_t size = _size.IsEmpty() ? params->GetParam<SizeArg>(bucket) : _size.GetValue();
                    return this->file->Truncate(size, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                TruncateImpl<Handled>* r = new TruncateImpl<Handled>(this->file, std::move(h));
                r->SetParams(_size);
                delete this;
                return r;
            }

            OptionalParam<uint64_t> _size;
    };
    typedef TruncateImpl<Bare> Truncate;
    template <State state> const std::string TruncateImpl<state>::SizeArg::key = "size";


    template <State state>
    class VectorReadImpl: public Operation<state> {
        public:
            VectorReadImpl(File *f): Operation<state>(f){}
            VectorReadImpl(File &f): Operation<state>(&f){}
            VectorReadImpl(File *f, std::unique_ptr<OperationHandler> h): Operation<state>(f, std::move(h)){}
            
            struct ChunksArg {
                static const std::string key;
                typedef ChunkList type;
            };

            struct BufferArg {
                static const std::string key;
                typedef char* type;
            };

            void SetParams(OptionalParam<ChunkList> chunks, OptionalParam<void*> buffer) {
                _chunks = chunks;
                _buffer = buffer;
            }

            VectorReadImpl<Configured>& operator()(OptionalParam<ChunkList> chunks, OptionalParam<void*> buffer) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                VectorReadImpl<Configured>* r = new VectorReadImpl<Configured>(this->file, NULL);
                r->SetParams(chunks, buffer);
                return *r;
            }

            std::string ToString(){
                return "VectorRead";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucket = 1){
                try {
                    const ChunkList& chunks = _chunks.IsEmpty() ? params->GetParam<ChunksArg>(bucket) : _chunks.GetValue();
                    void *buffer = _buffer.IsEmpty() ? params->GetParam<BufferArg>(bucket) : _buffer.GetValue();
                    return this->file->VectorRead(chunks, buffer, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                VectorReadImpl<Handled>* r = new VectorReadImpl<Handled>(this->file, std::move(h));
                r->SetParams(_chunks, _buffer);
                delete this;
                return r;
            }

            OptionalParam<ChunkList> _chunks;
            OptionalParam<void*> _buffer;
    };
    typedef VectorReadImpl<Bare> VectorRead;
    template <State state> const std::string VectorReadImpl<state>::ChunksArg::key = "chunks";
    template <State state> const std::string VectorReadImpl<state>::BufferArg::key = "buffer";


    template <State state>
    class VectorWriteImpl: public Operation<state> {
        public:
            VectorWriteImpl(File *f): Operation<state>(f){}
            VectorWriteImpl(File &f): Operation<state>(&f){}
            VectorWriteImpl(File *f, std::unique_ptr<OperationHandler> h): Operation<state>(f, std::move(h)){}
            
            struct ChunksArg {
                static const std::string key;
                typedef ChunkList type;
            };

            void SetParams(OptionalParam<ChunkList> chunks) {
                _chunks = chunks;
            }

            VectorWriteImpl<Configured>& operator()(OptionalParam<ChunkList> chunks) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                VectorWriteImpl<Configured>* r = new VectorWriteImpl<Configured>(this->file, NULL);
                r->SetParams(chunks);
                return *r;
            }     

            std::string ToString(){
                return "VectorWrite";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucket = 1){
                try {
                    const ChunkList& chunks = _chunks.IsEmpty() ? params->GetParam<ChunksArg>(bucket) : _chunks.GetValue();
                    return this->file->VectorWrite(chunks, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                VectorWriteImpl<Handled>* r = new VectorWriteImpl<Handled>(this->file, std::move(h));
                r->SetParams(_chunks);
                delete this;
                return r;
            }

            OptionalParam<ChunkList> _chunks;
    };
    typedef VectorWriteImpl<Bare> VectorWrite;
    template <State state> const std::string VectorWriteImpl<state>::ChunksArg::key = "chunks";


    template <State state>
    class WriteVImpl: public Operation<state> {
        public:
            WriteVImpl(File *f): Operation<state>(f){}
            WriteVImpl(File &f): Operation<state>(&f){}
            WriteVImpl(File *f, std::unique_ptr<OperationHandler> h): Operation<state>(f, std::move(h)){}
            
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

            void SetParams(OptionalParam<uint64_t> offset, OptionalParam<struct iovec*> iov, OptionalParam<int> iovcnt) {
                _offset = offset;
                _iov = iov;
                _iovcnt = iovcnt;
            }

            WriteVImpl<Configured>& operator()(OptionalParam<uint64_t> offset, OptionalParam<struct iovec*> iov, OptionalParam<int> iovcnt) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                WriteVImpl<Configured>* r = new WriteVImpl<Configured>(this->file, NULL);
                r->SetParams(offset, iov, iovcnt);
                return *r;
            }     

            std::string ToString(){
                return "WriteV";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucket = 1){
                try {                    
                    uint64_t offset = _offset.IsEmpty() ? params->GetParam<OffsetArg>(bucket) : _offset.GetValue();
                    const struct iovec* iov = _iov.IsEmpty() ? params->GetParam<IovArg>(bucket) : _iov.GetValue();
                    int iovcnt = _iovcnt.IsEmpty() ? params->GetParam<IovcntArg>(bucket) : _iovcnt.GetValue();
                    return this->file->WriteV(offset, iov, iovcnt, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }

            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                WriteVImpl<Handled>* r = new WriteVImpl<Handled>(this->file, std::move(h));
                r->SetParams(_offset, _iov, _iovcnt);
                delete this;
                return r;
            }

            OptionalParam<uint64_t> _offset;
            OptionalParam<struct iovec*> _iov;
            OptionalParam<int> _iovcnt;
    };
    typedef WriteVImpl<Bare> WriteV;
    template <State state> const std::string WriteVImpl<state>::OffsetArg::key = "offset";
    template <State state> const std::string WriteVImpl<state>::IovArg::key = "iov";
    template <State state> const std::string WriteVImpl<state>::IovcntArg::key = "iovcnt";


    template <State state>
    class FcntlImpl: public Operation<state> {
        public:
            FcntlImpl(File *f): Operation<state>(f){}
            FcntlImpl(File &f): Operation<state>(&f){}
            FcntlImpl(File *f, std::unique_ptr<OperationHandler> h): Operation<state>(f, std::move(h)){}
            
            struct BufferArg {
                static const std::string key;
                typedef Buffer type;
            };

            void SetParams(OptionalParam<Buffer> arg) {
                _arg = arg;
            }

            FcntlImpl<Configured>& operator()(OptionalParam<Buffer> arg) {
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                FcntlImpl<Configured>* r = new FcntlImpl<Configured>(this->file, NULL);
                r->SetParams(arg);
                return *r;
            }     

            std::string ToString(){
                return "Fcntl";
            }

        protected:    
            XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucket = 1){
                try {
                    const Buffer& arg = _arg.IsEmpty() ? params->GetParam<BufferArg>(bucket) : _arg.GetValue();
                    return this->file->Fcntl(arg, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                FcntlImpl<Handled>* r = new FcntlImpl<Handled>(this->file, std::move(h));
                r->SetParams(_arg);
                delete this;
                return r;
            }

            OptionalParam<Buffer> _arg;
    };
    typedef FcntlImpl<Bare> Fcntl;
    template <State state> const std::string FcntlImpl<state>::BufferArg::key = "arg";


    template <State state = Bare>
    class VisaImpl: public Operation<state> {
        public:
            VisaImpl(File *f): Operation<state>(f){}
            VisaImpl(File &f): Operation<state>(&f){}
            VisaImpl(File *f, std::unique_ptr<OperationHandler> h): Operation<state>(f, std::move(h)){}

            VisaImpl<Configured>& operator()(){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                VisaImpl<Configured> *c = new VisaImpl<Configured>(this->file, NULL);
                return *c;
            }

            std::string ToString(){
                return "Visa";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucket = 1){
                return this->file->Visa(this->handler.get());
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                VisaImpl<Handled> *c = new VisaImpl<Handled>(this->file, std::move(h));
                delete this;
                return c;
            }
    };
    typedef VisaImpl<Bare> Visa;


    template <State state = Bare>
    class MultiWorkflow: public Operation<state> {
        public:
            MultiWorkflow(std::initializer_list<Operation<Handled>*> operations): Operation<state>(NULL, NULL){
                std::initializer_list<Operation<Handled>*>::iterator it = operations.begin();
                while(it != operations.end()){
                    std::unique_ptr<Workflow> w(new Workflow(*it, false));
                    workflows.push_back(std::move(w));
                    it++;
                }
            }
            
            MultiWorkflow(std::vector<std::unique_ptr<Workflow>> workflowsArray, std::unique_ptr<OperationHandler> h): Operation<state>(NULL, std::move(h)){
                workflows.swap(workflowsArray);
            }

            std::string ToString(){
                std::ostringstream oss;
                oss<<"Multiworkflow(";
                for(int i=0; i<workflows.size(); i++){
                    oss<<workflows[i]->ToString();
                    if(i != workflows.size() - 1){
                        oss<<" && ";
                    }
                }
                oss<<")";
                return oss.str();
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ParamsContainer> params, int bucketDefault = 0){
                for(int i=0; i<workflows.size(); i++){
                    int bucket = i + 1;
                    workflows[i]->Run(params, bucket);
                }

                bool statusOK = true;
                std::string statusMessage = "";

                for(int i=0; i<workflows.size(); i++){
                    workflows[i]->Wait();                    
                    auto result = workflows[i]->GetStatus();
                    if(!result.IsOK()){
                        statusOK = false;
                        statusMessage = result.ToStr();
                    }
                }
                const uint16_t status = statusOK ? stOK : stError;

                XRootDStatus *st = new XRootDStatus(status, statusMessage);
                this->handler->HandleResponseWithHosts(st, NULL, NULL);

                return XRootDStatus();
            }

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                MultiWorkflow<Handled> *c = new MultiWorkflow<Handled>(std::move(workflows), std::move(h));
                return c;
            }

            std::vector<std::unique_ptr<Workflow>> workflows;
    };
    typedef MultiWorkflow<Configured> MultiWorkflowOperation;

}


#endif // __XRD_CL_OPERATIONS_HH__
