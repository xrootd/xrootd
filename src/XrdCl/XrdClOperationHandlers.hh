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

#ifndef __XRD_CL_OPERATION_HANDLERS_HH__
#define __XRD_CL_OPERATION_HANDLERS_HH__

#include "XrdCl/XrdClOperations.hh"


namespace XrdCl {

    class SimpleFunctionWrapper: public ForwardingHandler {
        public:
            SimpleFunctionWrapper(std::function<void(XrdCl::XRootDStatus&)> handleFunction): fun(handleFunction){}
    
            void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
                fun(*status);
                delete status;
                delete response;
                delete this;
            }

        private:
            std::function<void(XrdCl::XRootDStatus&)> fun;
    };

    class SimpleForwardingFunctionWrapper: public ForwardingHandler {
        public:
            SimpleForwardingFunctionWrapper(std::function<void(XrdCl::XRootDStatus&, OperationContext&)> handleFunction): fun(handleFunction){}
    
            void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
                auto paramsContainerWrapper = GetParamsContainerWrapper();
                fun(*status, *paramsContainerWrapper.get());
                delete status;
                delete response;
                delete this;
            }

        private:
            std::function<void(XrdCl::XRootDStatus&, OperationContext&)> fun;
    };

    template<typename ResponseType>
    class FunctionWrapper: public ForwardingHandler {
        public:
            FunctionWrapper(std::function<void(XrdCl::XRootDStatus&, ResponseType&)> handleFunction): fun(handleFunction){}

            void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
                ResponseType *res = 0;
                if( status->IsOK() ) response->Get(res);
                else res = &nullref;
                fun(*status, *res);
                delete status;
                delete response;
                delete this;
            }

        private:
            std::function<void(XrdCl::XRootDStatus&, ResponseType&)> fun;
            static ResponseType nullref;
    };

    template<typename ResponseType>
    ResponseType FunctionWrapper<ResponseType>::nullref;

    template<typename ResponseType>
    class ForwardingFunctionWrapper: public ForwardingHandler {
        public:
            ForwardingFunctionWrapper(std::function<void(XrdCl::XRootDStatus&, ResponseType&, OperationContext&)> handleFunction): fun(handleFunction){}

            void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
                ResponseType *res = 0;
                if( status->IsOK() ) response->Get(res);
                else res = &nullref;
                auto paramsContainerWrapper = GetParamsContainerWrapper();
                fun(*status, *res, *paramsContainerWrapper.get());
                delete status;
                delete response;
                delete this;
            }

        private:
            std::function<void(XrdCl::XRootDStatus&, ResponseType&, OperationContext &wrapper)> fun;
            static ResponseType nullref;
    };

    template<typename ResponseType>
    ResponseType ForwardingFunctionWrapper<ResponseType>::nullref;

    class ExOpenFuncWrapper : public ForwardingHandler {
        public:
            ExOpenFuncWrapper(File &f, std::function<void(XrdCl::XRootDStatus&, StatInfo&)> handleFunction): f(f), fun(handleFunction){}

            void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
              StatInfo *info = 0;
              if( status->IsOK() ) f.Stat( false, info );
              else info = &nullref;
              fun( *status, *info );
              delete info;
              delete status;
              delete response;
              delete this;
            }

        private:
            File &f;
            std::function<void(XrdCl::XRootDStatus&, StatInfo&)> fun;
            static StatInfo nullref;
    };

    StatInfo ExOpenFuncWrapper::nullref;

    class ForwardingExOpenFuncWrapper : public ForwardingHandler {
        public:
            ForwardingExOpenFuncWrapper(File &f, std::function<void(XrdCl::XRootDStatus&, StatInfo&, OperationContext&)> handleFunction): f(f), fun(handleFunction){}

            void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) {
              StatInfo *info = 0;
              if( status->IsOK() ) f.Stat( false, info );
              else info = &nullref;
              auto paramsContainerWrapper = GetParamsContainerWrapper();
              fun( *status, *info, *paramsContainerWrapper.get() );
              delete info;
              delete status;
              delete response;
              delete this;
            }

        private:
            File &f;
            std::function<void(XrdCl::XRootDStatus&, StatInfo&, OperationContext&)> fun;
            static StatInfo nullref;
    };

    StatInfo ForwardingExOpenFuncWrapper::nullref;

}


#endif // __XRD_CL_OPERATIONS_HANDLERS_HH__
