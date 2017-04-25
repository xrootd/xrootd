#include "XrdCl/XrdClLocalFileHandler.hh"
#include "XrdCl/XrdClConstants.hh"

#include <string>
#include <iostream>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/uio.h>

namespace XrdCl{
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      LocalFileHandler::LocalFileHandler()
      {
         jmngr = new JobManager(3);//TO DO: How many?
         jmngr->Initialize();
         jmngr->Start();
      }
      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      LocalFileHandler::~LocalFileHandler()
      {
      }
      //------------------------------------------------------------------------
      // Open
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::Open( const std::string& url, uint16_t flags,
                     uint16_t mode, ResponseHandler* handler, uint16_t timeout )
      {
         Log *log = DefaultEnv::GetLog();
         AnyObject *obj = new AnyObject();
         XRootDStatus *st;
         LocalFileTask *task;
         HostList* hosts = new HostList();
         hosts->push_back( HostInfo( URL( "localhost" ), true ) );
         StatInfo *statInfo = new StatInfo();

         std::string fileurl = url;
         if( url.find("file://") == 0 )
            fileurl.erase( 0, 7 );
         else{
            log->Warning( FileMsg,
               "%s in lFileHandler::Open does not contain file:// at front", url.c_str() );
         }

         mode_t openmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; //TO DO
         fd = open( fileurl.c_str(), O_RDWR | O_CREAT, openmode );
         if( fd == -1 )
         {
            log->Error( FileMsg, "Unable to open %s: %s",
                                    fileurl.c_str(), strerror( errno ) );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            //task = new LocalFileTask( st, 0, hosts, handler );
            //jmngr->QueueJob(task);//TO DO: In case we return the error from FileStateHandler,
            //we can't queue the job else there is a segfault, also the OpenHandler can't be
            //deleted
            return *st;
         }
         else{ st = new XRootDStatus( stOK ); }

         struct stat ssp;
         if( fstat( fd, &ssp ) == -1 )
         {
            log->Error( FileMsg, "Unable to stat in Open" );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            return *st;
         }

         std::ostringstream data;
         data<<ssp.st_dev <<" "<< ssp.st_size <<" "<<ssp.st_mode<<" "<<ssp.st_mtime;
         log->Debug( FileMsg, data.str().c_str() );

         if( !statInfo->ParseServerResponse( data.str().c_str() ) ){
            log->Error( FileMsg, "Unable to ParseServerResponse for Local File Stat Open" );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            return *st;
         }

         uint8_t ufd = fd;// TO DO
         OpenInfo *openInfo = new OpenInfo( &ufd, 1, statInfo );
         obj->Set( openInfo );
         task = new LocalFileTask( st, obj, hosts, handler );
         jmngr->QueueJob( task );
         return *st;
      }
      //------------------------------------------------------------------------
      // Close
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::Close( ResponseHandler* handler, 
                                                   uint16_t timeout )
      {
         Log *log = DefaultEnv::GetLog();
         AnyObject *obj = new AnyObject();
         XRootDStatus *st;
         LocalFileTask *task;
         HostList* hosts = new HostList();
         hosts->push_back( HostInfo( URL( "localhost" ), true ) );

         if( close( fd ) == -1 )
         {
            log->Error( FileMsg, "Unable to close file fd: %i", fd );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            //task = new LocalFileTask( st, NULL, hosts, handler );
            //jmngr->QueueJob( task );
            return *st;
         }
         else{ 
            st = new XRootDStatus( stOK ); 
            task = new LocalFileTask( st, obj, hosts, handler );
            jmngr->QueueJob( task );
            return *st;
         }
      }
      //------------------------------------------------------------------------
      // Stat
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::Stat( ResponseHandler* handler,
                                                               uint16_t timeout )
      {
         Log *log = DefaultEnv::GetLog();
         AnyObject *obj = new AnyObject();
         XRootDStatus *st;
         LocalFileTask *task;
         HostList* hosts = new HostList();
         hosts->push_back( HostInfo( URL( "localhost" ), true ) );
         StatInfo *statInfo = new StatInfo();
         obj->Set(statInfo);

         struct stat ssp;
         if( fstat( fd, &ssp ) == -1 )
         {
            log->Error( FileMsg, "Unable to stat fd: %i in lFileHandler", fd );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            //task = new LocalFileTask( st, NULL, hosts, handler );
            //jmngr->QueueJob(task);
            return *st;
         }
         std::ostringstream data;
         data<<ssp.st_dev <<" "<< ssp.st_size <<" "<<ssp.st_mode<<" "<<ssp.st_mtime;
         log->Debug( FileMsg, data.str().c_str() );

         if( !statInfo->ParseServerResponse(data.str().c_str()) ){
            log->Error(FileMsg, "Unable to ParseServerResponse for lFileHandler statinfo" );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            //task = new LocalFileTask( st, NULL, hosts, handler );
            //jmngr->QueueJob(task);
            return *st;
         }
         else{
            st = new XRootDStatus( stOK );
            task = new LocalFileTask( st, obj, hosts, handler );
            jmngr->QueueJob( task );
            return *st;
         }
      }
      //------------------------------------------------------------------------
      // Read
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::Read( uint64_t offset, uint32_t size,
               void* buffer, ResponseHandler* handler, uint16_t timeout )
      {
         Log *log = DefaultEnv::GetLog();
         AnyObject *obj = new AnyObject();
         XRootDStatus *st;
         LocalFileTask *task;
         HostList* hosts = new HostList();
         hosts->push_back( HostInfo( URL( "localhost" ), true ) );
         ChunkInfo *resp = new ChunkInfo( offset, size, buffer );
         obj->Set(resp);

         if( pread( fd, buffer, size, offset ) == -1 ){
            log->Error( FileMsg, "Unable to read LocalFile" );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            //task = new LocalFileTask( st, NULL, hosts, handler );
            //jmngr->QueueJob(task);
            return *st;
         }
         else{
            st = new XRootDStatus( stOK );
            log->Dump( FileMsg, "Chunkinfo: size: %i, offset: %i, Buffer: %s",
                              resp->length, resp->offset, resp->buffer );
            task = new LocalFileTask( st, obj, hosts, handler );
            jmngr->QueueJob( task );
            return *st;
         }
      }
      //------------------------------------------------------------------------
      // Write
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::Write( uint64_t offset, uint32_t size,
            const void* buffer, ResponseHandler* handler, uint16_t timeout )
      {
         Log *log = DefaultEnv::GetLog();
         AnyObject *obj = new AnyObject();
         XRootDStatus *st;
         LocalFileTask *task;
         HostList* hosts = new HostList();
         hosts->push_back( HostInfo( URL( "localhost" ), true ) );

         int written=-1;
         if(( written = write( fd, buffer, size ) ) <= 0 ){
            log->Error( FileMsg, "write failed, wrote %i", written );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            //task = new LocalFileTask( st, NULL, hosts, handler );
            //jmngr->QueueJob( task );
            return *st;
         }
         else{
            log->Dump(FileMsg, "write succeeded, wrote %i", written );
            st = new XRootDStatus( stOK );
            task = new LocalFileTask( st, obj, hosts, handler );
            jmngr->QueueJob( task );
            return *st;
         }
      }
      //------------------------------------------------------------------------
      // Sync
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::Sync( ResponseHandler* handler,
                                                   uint16_t timeout )
      {
         Log *log = DefaultEnv::GetLog();
         AnyObject *obj = new AnyObject();
         XRootDStatus *st;
         LocalFileTask *task;
         HostList* hosts = new HostList();
         hosts->push_back( HostInfo( URL( "localhost" ), true ) );

         if( syncfs( fd ) == -1 ){
            log->Error( FileMsg, "Unable to Sync, filedescriptor: %i", fd);
            st = new XRootDStatus( stError, errErrorResponse, errno );
            //task = new LocalFileTask( st, NULL, hosts, handler );
            //jmngr->QueueJob(task);
            return *st;
         }
         else{
            st = new XRootDStatus( stOK );
            task = new LocalFileTask( st, obj, hosts, handler );
            jmngr->QueueJob(task);
            return *st;
         }
      }
      //------------------------------------------------------------------------
      // Truncate
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::Truncate(uint64_t size,
               ResponseHandler* handler, uint16_t timeout)
      {
         Log *log = DefaultEnv::GetLog();
         AnyObject *obj = new AnyObject();
         XRootDStatus *st;
         LocalFileTask *task;
         HostList* hosts = new HostList();
         hosts->push_back( HostInfo( URL( "localhost" ), true ) );

         if( ftruncate( fd, size ) == -1){
            log->Error( FileMsg, "Unable to Truncate, filedescriptor: %i", fd);
            st = new XRootDStatus( stError, errErrorResponse, errno );
            //task = new LocalFileTask( st, NULL, hosts, handler );
            //jmngr->QueueJob( task );
            return *st;
         }
         else{
            st = new XRootDStatus( stOK );
            task = new LocalFileTask( st, obj, hosts, handler );
            jmngr->QueueJob( task );
            return *st;
         }
      }
      //------------------------------------------------------------------------
      // VectorRead
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::VectorRead( const ChunkList& chunks,
               void* buffer, ResponseHandler* handler, uint16_t timeout )
      {
         Log *log = DefaultEnv::GetLog();
         AnyObject *obj = new AnyObject();
         XRootDStatus *st;
         LocalFileTask *task;
         HostList *hosts = new HostList();
         hosts->push_back( HostInfo( URL( "localhost" ), true ) );
         VectorReadInfo* info = new VectorReadInfo();

         int iovcnt;
         struct iovec iov[chunks.size()];
         iovcnt = sizeof(iov) / sizeof(struct iovec);

         for( unsigned int i = 0; i < chunks.size(); i++ ){
            iov[i].iov_base = chunks[i].buffer;
            iov[i].iov_len = chunks[i].length;
         }

         if( readv( fd, iov, iovcnt ) == -1 ){
            log->Error( FileMsg, "Unable to VectorRead, filedescriptor: %i", fd);
            st = new XRootDStatus( stError, errErrorResponse, errno );
            //LocalFileTask *task = new LocalFileTask( st, 0, hosts, handler );
            //jmngr->QueueJob(task);
            return *st;
         }
         else{
            info->GetChunks() = chunks;
            obj->Set(info);
            st = new XRootDStatus( stOK );
            task = new LocalFileTask( st, obj, hosts, handler );
            jmngr->QueueJob(task);
            return *st;
         }
      }
}
