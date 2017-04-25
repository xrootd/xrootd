#include "XrdCl/XrdClLocalFileTask.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"


namespace XrdCl
{
   LocalFileTask::LocalFileTask( XRootDStatus *st, AnyObject *obj, HostList *hosts, ResponseHandler *responsehandler )
   {
      this->st = st;
      this->obj = obj;
      this->hosts = hosts;
      this->responsehandler = responsehandler;
   }

   LocalFileTask::~LocalFileTask()
   {
   }

   void LocalFileTask::Run( void *arg )
   {
      if( responsehandler )
         responsehandler->HandleResponseWithHosts( st, obj, hosts );
   }
}
