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

#include <iostream>
#include <string>
#include <sstream>

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
    //! Is is nested  structure, the first layer
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
            T GetParam(std::string key, int bucket = 1){
                T* valuePtr = GetPtrParam<T*>(key, bucket);
                return *valuePtr;
            }

            //------------------------------------------------------------
            //! Get pointer from container
            //!
            //! @param key  key under which the pointer is stored
            //! @return     pointer to stored object
            //------------------------------------------------------------
            template <typename T>
            T GetPtrParam(std::string key, int bucket = 1){
                if(!Exists(key, bucket)){
                    std::ostringstream oss;
                    oss<<"Parameter "<<key<<" has not been specified in bucket "<<bucket;
                    throw std::logic_error(oss.str());
                }
                AnyObject *obj = paramsMap[bucket][key];
                T valuePtr = 0;
                obj->Get(valuePtr);
                return valuePtr;
            }

            //------------------------------------------------------------
            //! Save value in container
            //!
            //! @param key      key under which the value will be saved
            //! @param value    value to save
            //! @param bucket   bucket in which key will be added
            //------------------------------------------------------------
            template <typename T>
            void SetParam(std::string key, T value, int bucket = 1){
                T *valuePtr = new T(value);
                SetPtrParam(key, valuePtr, true);
            }

            //------------------------------------------------------------
            //! Save pointer in container
            //!
            //! @param key      key under which the pointer will be saved
            //! @param value    pointer to save
            //! @param passOwnership    flag indicating whether memory 
            //!                         should be released automatically
            //!                         after removing object from map
            //! @param bucket   bucket to which key will be saved
            //------------------------------------------------------------
            template <typename T>
            void SetPtrParam(std::string key, T* value, bool passOwnership, int bucket = 1){
                if(!BucketExists(bucket)){
                    paramsMap[bucket] = std::map<std::string, AnyObject*>();
                }
                if(paramsMap[bucket].find(key) != paramsMap[bucket].end()){
                    std::ostringstream oss;
                    oss<<"Parameter "<<key<<" has already been set in bucket "<<bucket;
                    throw std::logic_error(oss.str());
                }
                AnyObject *obj = new AnyObject();
                obj->Set(value, passOwnership);
                paramsMap[bucket][key] = obj;
            }     

            //------------------------------------------------------------
            //! Check whether given key exists in the container under 
            //! given bucket
            //!
            //! @param key      key to check
            //! @param bucket   bucket which will be checked
            //! @return         true if exists, false if not
            //------------------------------------------------------------
            bool Exists(std::string key, int bucket = 1){
                return paramsMap.find(bucket) != paramsMap.end() && paramsMap[bucket].find(key) != paramsMap[bucket].end();
            }

            //------------------------------------------------------------------
            //! Check whether given bucket exist in the container
            //!
            //! @param bucket   bucket to check
            //! @return         true if exists, false if not
            //------------------------------------------------------------------
            bool BucketExists(int bucket){
                return paramsMap.find(bucket) != paramsMap.end();
            }

            ~ParamsContainer(){
                std::map<int, std::map<std::string, AnyObject*>>::iterator buckets = paramsMap.begin();
                //----------------------------------------------------------------
                // Destroy dynamically allocated objects stored in map
                //----------------------------------------------------------------
                while(buckets != paramsMap.end()){
                    std::map<std::string, AnyObject*> objectsMap = buckets->second; 
                    std::map<std::string, AnyObject*>::iterator it = objectsMap.begin();
                    while(it != objectsMap.end()){
                        AnyObject* obj = it->second;
                        it++;
                        delete obj;
                    }
                    buckets++;
                } 
            }

        private:
            std::map<int, std::map<std::string, AnyObject*>> paramsMap;
    };

}




#endif // __XRD_CL_OPERATION_PARAMS_HH__
