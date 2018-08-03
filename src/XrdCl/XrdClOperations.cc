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

#include <XrdCl/XrdClOperations.hh>


namespace XrdCl {

    //----------------------------------------------------------------------------
    // OperationHandler
    //----------------------------------------------------------------------------

    OperationHandler::OperationHandler(ForwardingHandler *handler): nextOperation(NULL), semaphore(NULL){
        responseHandler = handler;
        params = handler->GetParamsContainer();
    }

    void OperationHandler::AddOperation(Operation<Handled> *operation){
        if(nextOperation){
            nextOperation->AddOperation(operation);
        } else {
            nextOperation = operation;
        }
    }

    void OperationHandler::HandleResponseWithHosts(XRootDStatus *status, AnyObject *response, HostList *hostList){
        if(!status->IsOK()){
            return HandleFailedOperationStatus(status, response);
        }
        if(nextOperation){
            responseHandler->HandleResponseWithHosts(status, response, hostList);
            return RunNextOperation();
        }
        // We need to copy status as original status object is destroyed in HandleResponse function
        auto statusCopy = new XRootDStatus(*status);
        responseHandler->HandleResponseWithHosts(status, response, hostList);
        workflow->EndWorkflowExecution(statusCopy);
    }

    void OperationHandler::HandleResponse(XRootDStatus *status, AnyObject *response){
        if(!status->IsOK()){
            return HandleFailedOperationStatus(status, response);   
        }
        if(nextOperation){
            responseHandler->HandleResponse(status, response);
            return RunNextOperation();
        }
        // We need to copy status as original status object is destroyed in HandleResponse function
        auto statusCopy = new XRootDStatus(*status);
        responseHandler->HandleResponse(status, response);
        workflow->EndWorkflowExecution(statusCopy);
    }

    void OperationHandler::HandleFailedOperationStatus(XRootDStatus *status, AnyObject *response){
        if(response){
            delete response;
        }
        workflow->EndWorkflowExecution(status);
    }

    void OperationHandler::RunNextOperation(){
        nextOperation->Run(params);
    }

    OperationHandler::~OperationHandler(){
        if(nextOperation) { delete nextOperation; }
        if(params) { delete params; }
        if(responseHandler) { delete responseHandler; }
    }

    void OperationHandler::AssignToWorkflow(Workflow *wf){
        workflow = wf;
        if(nextOperation){
            nextOperation->AssignToWorkflow(wf);
        }
    }


    //----------------------------------------------------------------------------
    // Workflow
    //----------------------------------------------------------------------------

    Workflow::Workflow(Operation<Handled> &op): semaphore(NULL), status(NULL){
        firstOperation = &op;
        firstOperationParams = new ParamsContainer();
    }

    Workflow::~Workflow(){
        delete firstOperation;
        delete firstOperationParams;
        if(semaphore) { delete semaphore; }
        if(status) { delete status; }
    }

    Workflow& Workflow::Run(){
        if(semaphore){
            throw std::logic_error("Workflow is already running");
        }
        semaphore = new XrdSysSemaphore(0);
        firstOperation->AssignToWorkflow(this);
        firstOperation->Run(firstOperationParams);
        return *this;
    }

    void Workflow::EndWorkflowExecution(XRootDStatus *lastOperationStatus){
        if(semaphore){
            status = lastOperationStatus;
            semaphore->Post();
        }
    }

    XRootDStatus Workflow::GetStatus(){
        return status ? *status : XRootDStatus();
    }

    void Workflow::Wait(){
        if(semaphore){
            semaphore->Wait();
        }
        return;
    }

};
