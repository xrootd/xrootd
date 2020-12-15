/*
 * XrdEcScheduleHandler.cc
 *
 *  Created on: Nov 1, 2019
 *      Author: simonm
 */




#include "XrdEc/XrdEcScheduleHandler.hh"

namespace XrdEc
{
  class ResponseJob : public XrdCl::Job
  {
    public:
      ResponseJob( XrdCl::ResponseHandler *handler,
                   XrdCl::XRootDStatus    *status,
                   XrdCl::AnyObject       *response ):
        pHandler( handler ), pStatus( status ), pResponse( response )
      {
      }

      virtual void Run( void *arg )
      {
        pHandler->HandleResponse( pStatus, pResponse );
        delete this;
      }

    private:

      XrdCl::ResponseHandler *pHandler;
      XrdCl::XRootDStatus    *pStatus;
      XrdCl::AnyObject       *pResponse;
  };

  void ScheduleHandler( uint64_t offset, uint32_t size, char *buffer, XrdCl::ResponseHandler *handler )
  {
    if( !handler ) return;

    XrdCl::ChunkInfo *chunk = new XrdCl::ChunkInfo();
    chunk->offset = offset;
    chunk->length = size;
    chunk->buffer = buffer;

    XrdCl::AnyObject *resp = new XrdCl::AnyObject();
    resp->Set( chunk );

    ResponseJob *job = new ResponseJob( handler, new XrdCl::XRootDStatus(), resp );
    XrdCl::DefaultEnv::GetPostMaster()->GetJobManager()->QueueJob( job );
  }

  void ScheduleHandler( XrdCl::ResponseHandler *handler, const XrdCl::XRootDStatus &st )
  {
    if( !handler ) return;

    ResponseJob *job = new ResponseJob( handler, new XrdCl::XRootDStatus( st ), 0 );
    XrdCl::DefaultEnv::GetPostMaster()->GetJobManager()->QueueJob( job );
  }
}
