#ifndef __XRD_CL_OPERATIONS_HH__
#define __XRD_CL_OPERATIONS_HH__

#include <iostream>
#include <XrdCl/XrdClFile.hh>

using namespace std;

namespace XrdCl {

    class Operation;
    class HandledOperation;

    class OperationHandler: public ResponseHandler {
        public:
            OperationHandler(ResponseHandler *handler);
            void AddOperation(HandledOperation *op);
            void AddNextHandler(ResponseHandler *handler);
            void SetSemaphore(XrdSysSemaphore *sem);
            ResponseHandler* GetHandler();
            virtual void HandleResponseWithHosts(XRootDStatus *status, AnyObject *response, HostList *hostList);
            virtual void HandleResponse(XRootDStatus *status, AnyObject *response);
            virtual ~OperationHandler();

        protected:
            void RunNextOperation();

        private:
            ResponseHandler *responseHandler;
            HandledOperation *nextOperation;
            XrdSysSemaphore *semaphore;

    };

    class Operation {
        friend class HandledOperation;

        public:
            Operation(File *f);
            Operation(File *f, OperationHandler *h);
            virtual ~Operation(){};
            HandledOperation& SetHandler(ResponseHandler *h);
            HandledOperation& operator>>(ResponseHandler *h);

            virtual string GetName() const {return "Operation";}

        protected:
            virtual XRootDStatus Run(OperationHandler *h);
            
            File *file;
    };

    class Read: public Operation {
        public:
            Read(File *f);
            Read(File *f, ResponseHandler *h);
            Read& operator()(uint64_t offset, uint32_t size, void *buffer);     

            string GetName() const {return "Read";}

        private:
            Read(Read* obj);
            XRootDStatus Run(OperationHandler *h);

            uint64_t _offset;
            uint32_t _size;
            void *_buffer;
    };


    class Open: public Operation {
        public:
            Open(File *f);
            Open(File *f, ResponseHandler *h);
            Open& operator()(const std::string &url, OpenFlags::Flags flags, Access::Mode mode = Access::None);
        
            string GetName() const {return "Open";}

        protected:
            Open(Open* obj);
            XRootDStatus Run(OperationHandler *h);   
            
            std::string _url;
            OpenFlags::Flags _flags;
            Access::Mode _mode;
    };

    class Close: public Operation {
        public:
            Close(File *f);
            Close(File *f, ResponseHandler *h);
            Close& operator()();

            string GetName() const {return "Close";}

        protected:
            XRootDStatus Run(OperationHandler *h);
            Close(Close* obj);
    };

    
    class HandledOperation {
        public:
            HandledOperation();
            HandledOperation(Operation* op, OperationHandler* h);
            ~HandledOperation();
            void AddOperation(HandledOperation *op);
            void SetSemaphore(XrdSysSemaphore *sem);
            HandledOperation& operator|(HandledOperation &op);
            XRootDStatus Run();
            string GetName();
        private:
            Operation* operation;
            OperationHandler* handler;
    };

    class Workflow {
        public:
            Workflow(HandledOperation& op);
            ~Workflow();
            Workflow& Run();
            void Wait();

        private:
            HandledOperation *firstOperation;
            XrdSysSemaphore *semaphore;
    };

}




#endif // __XRD_CL_OPERATIONS_HH__
