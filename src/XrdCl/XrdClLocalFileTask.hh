#ifndef __XRD_CL_LOCAL_FILE_TASK_HH__
#define __XRD_CL_LOCAL_FILE_TASK_HH__

#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClAnyObject.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

namespace XrdCl{
    class LocalFileTask : public Job{
        
        public:
        LocalFileTask(  XRootDStatus * st, AnyObject* obj, HostList* hosts, ResponseHandler* responsehandler );
        ~LocalFileTask();
        virtual void Run( void *arg );
        
    private:
        XRootDStatus* st;
        AnyObject* obj;
        HostList* hosts;
        ResponseHandler* responsehandler;
    };
}

#endif