#ifndef __XRD_CL_OPERATION_PARAMS_HH__
#define __XRD_CL_OPERATION_PARAMS_HH__

#include <iostream>

#define notdef(type) OptionalParam<type>()

using namespace std;

namespace XrdCl {

    template <typename T>
    class OptionalParam {
        public:
            OptionalParam(T val){
                value = val;
                empty = false;
            }

            OptionalParam(){
                empty = true;
            }

            bool IsEmpty(){
                return empty;
            }

            T GetValue(){
                if(IsEmpty()){
                    throw logic_error("Cannot get parameter: value has not been specified");
                }
                return value;
            }

        private:
            T value;
            bool empty;
    };

    class ParamsContainer {
        public:
            template <typename T>
            T GetParam(std::string key){
                T* valuePtr = GetPtrParam<T*>(key);
                return *valuePtr;
            }

            template <typename T>
            T GetPtrParam(std::string key){
                if(!Exists(key)){
                    throw logic_error(std::string("Parameter ") + key + std::string(" has not been specified"));
                }
                AnyObject *obj = paramsMap[key];
                T valuePtr = 0;
                obj->Get(valuePtr);
                cout<<"Getting "<<key<<" param"<<endl;
                return valuePtr;
            }

            template <typename T>
            void SetParam(std::string key, T value){
                T *valuePtr = new T(value);
                SetPtrParam(key, valuePtr, true);
            }

            template <typename T>
            void SetPtrParam(std::string key, T* value, bool passOwnership = false){
                if(Exists(key)){
                    throw logic_error(std::string("Parameter ") + key + std::string(" has already been set"));
                }
                AnyObject *obj = new AnyObject();
                obj->Set(value, passOwnership);
                paramsMap[key] = obj;
            }     

            bool Exists(std::string key){
                return paramsMap.find(key) != paramsMap.end();
            } 

            ~ParamsContainer(){
                std::map<std::string, AnyObject*>::iterator it = paramsMap.begin();
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
