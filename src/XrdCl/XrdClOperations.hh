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
            ForwardingHandler(): container(new ParamsContainer()), responseHandler(NULL), wrapper(false){}

            ForwardingHandler(ResponseHandler *handler): container(new ParamsContainer()), responseHandler(handler), wrapper(true){}

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
            //------------------------------------------------------------------
            Operation(): handler(nullptr){                
                static_assert(state == Bare, "Constructor is available only for type Operation<Bare>");
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
            //! @param h  operation handler
            //------------------------------------------------------------------
            Operation(std::unique_ptr<OperationHandler> h): handler(std::move(h)){}

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

            std::unique_ptr<OperationHandler> handler;
    };

}


#endif // __XRD_CL_OPERATIONS_HH__
