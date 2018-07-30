#include <iostream>
#include <XrdCl/XrdClOperations.hh>
#include <typeinfo>

using namespace std;


namespace XrdCl {

    //////////// OperationHandler

    OperationHandler::OperationHandler(ForwardingHandler *handler){
        responseHandler = handler;
        params = handler->GetParamsContainer();
        nextOperation = NULL;
        semaphore = NULL;
    }

    void OperationHandler::AddOperation(Operation<Handled> *op){
        if(nextOperation){
            nextOperation->AddOperation(op);
        } else {
            nextOperation = op;
        }
    }

    void OperationHandler::HandleResponseWithHosts(XRootDStatus *status, AnyObject *response, HostList *hostList){
        bool operationStatus = status->IsOK();
        if(operationStatus){
            responseHandler->HandleResponseWithHosts(status, response, hostList);
            RunNextOperation();
        } else {
            cout<<"Operation status = "<<operationStatus
                <<". Handler and next operations will not be run. Reason: "
                <<status->ToStr()<<endl;
            if(semaphore){
                semaphore->Post();
            }
        }
    }

    void OperationHandler::HandleResponse(XRootDStatus *status, AnyObject *response){
        responseHandler->HandleResponse(status, response);
        if(status->IsOK()){
            RunNextOperation();
        } else {
            cout<<"Operation status = "<<status->IsOK()<<". Next operation will not be run."<<endl;
            if(semaphore){
                semaphore->Post();
            }
        }
    }

    void OperationHandler::RunNextOperation(){
        if(nextOperation){ 
            cout<<"Running next operation:  "<<nextOperation->GetName()<<endl;
            nextOperation->Run(params); 
        } else if (semaphore){
            semaphore->Post();
        }
    }

    OperationHandler::~OperationHandler(){
        if(nextOperation) { delete nextOperation; }
        if(params) { delete params; }
        if(responseHandler) { delete responseHandler; }
    }

    ForwardingHandler* OperationHandler::GetHandler(){
        return responseHandler;
    }

    void OperationHandler::SetSemaphore(XrdSysSemaphore *sem){
        semaphore = sem;
        if(nextOperation){
            nextOperation->SetSemaphore(sem);
        }
    }


    //////////////// Workflow

    Workflow::Workflow(Operation<Handled> &op){
        firstOperation = &op;
        firstOperationParams = new ParamsContainer();
        semaphore = NULL;        
    }

    Workflow::~Workflow(){
        delete firstOperation;
        delete firstOperationParams;
        if(semaphore) {
            delete semaphore;
        }
    }

    Workflow& Workflow::Run(){
        if(!semaphore){
            semaphore = new XrdSysSemaphore(0);
            firstOperation->SetSemaphore(semaphore);
            firstOperation->Run(firstOperationParams);
        } else {
            cout<<"Workflow is already running"<<endl;
        }
        return *this;
    }

    void Workflow::Wait(){
        semaphore->Wait();
        return;
    }

};
