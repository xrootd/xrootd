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

#ifndef __XRD_CL_OPERATION_PARAMS_HH__
#define __XRD_CL_OPERATION_PARAMS_HH__


namespace XrdCl {

    class NotDefParam {} notdef;

    template <typename T>
    class OptionalParam {
        public:
            OptionalParam(T val): empty(false){
                value = val;    
            }

            OptionalParam(): empty(true){}

            OptionalParam(NotDefParam notdef): empty(true){}

            bool IsEmpty(){
                return empty;
            }

            T GetValue(){
                if(IsEmpty()){
                    throw std::logic_error("Cannot get parameter: value has not been specified");
                }
                return value;
            }

        private:
            T value;
            bool empty;
    };


    //--------------------------------------------------------------------
    //! Container to store file operation parameters
    //--------------------------------------------------------------------
    class ParamsContainer {
        public:
            //------------------------------------------------------------
            //! Get value from container
            //!
            //! @param key  key under which the value is stored
            //! @return     value
            //------------------------------------------------------------
            template <typename T>
            T GetParam(std::string key){
                T* valuePtr = GetPtrParam<T*>(key);
                return *valuePtr;
            }

            //------------------------------------------------------------
            //! Get pointer from container
            //!
            //! @param key  key under which the pointer is stored
            //! @return     pointer to stored object
            //------------------------------------------------------------
            template <typename T>
            T GetPtrParam(std::string key){
                if(!Exists(key)){
                    throw std::logic_error(std::string("Parameter ") + key + std::string(" has not been specified"));
                }
                AnyObject *obj = paramsMap[key];
                T valuePtr = 0;
                obj->Get(valuePtr);
                return valuePtr;
            }

            //------------------------------------------------------------
            //! Save value in container
            //!
            //! @param key  key under which the value will be saved
            //------------------------------------------------------------
            template <typename T>
            void SetParam(std::string key, T value){
                T *valuePtr = new T(value);
                SetPtrParam(key, valuePtr, true);
            }

            //------------------------------------------------------------
            //! Save pointer in container
            //!
            //! @param key  key under which the pointer will be saved
            //------------------------------------------------------------
            template <typename T>
            void SetPtrParam(std::string key, T* value, bool passOwnership = false){
                if(Exists(key)){
                    throw std::logic_error(std::string("Parameter ") + key + std::string(" has already been set"));
                }
                AnyObject *obj = new AnyObject();
                obj->Set(value, passOwnership);
                paramsMap[key] = obj;
            }     

            //------------------------------------------------------------
            //! Check whether given key exists in the container
            //!
            //! @return true if exists, false if not
            //------------------------------------------------------------
            bool Exists(std::string key){
                return paramsMap.find(key) != paramsMap.end();
            }

            ~ParamsContainer(){
                std::map<std::string, AnyObject*>::iterator it = paramsMap.begin();
                //----------------------------------------------------------------
                // Clear map
                //----------------------------------------------------------------
                while(it != paramsMap.end()){
                    AnyObject* obj = it->second;
                    std::string keyToErase = it->first;
                    it++;
                    paramsMap.erase(keyToErase);
                    delete obj;
                } 
            }

        private:
            std::map<std::string, AnyObject*> paramsMap;
    };

}




#endif // __XRD_CL_OPERATION_PARAMS_HH__
