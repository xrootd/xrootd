//------------------------------------------------------------------------------
// Copyright (c) 2017 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH 
// Author: Paul-Niklas Kramp <p.n.kramp@gsi.de>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------
#include "XrdCl/XrdClLocalFileHandler.hh"
#include "XrdCl/XrdClConstants.hh"

#include <string>
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
         StatInfo *statInfo = new StatInfo();

         std::string fileurl = url;
         if( url.find("file://") == 0 )
            fileurl.erase( 0, 7 );
         else{
            log->Warning( FileMsg,
               "%s in lFileHandler::Open does not contain file:// at front", 
               url.c_str() );
         }
         //---------------------------------------------------------------------
         // Prepare Flags
         //---------------------------------------------------------------------
         uint16_t openflags = 0;
         if( flags & kXR_new ){
            openflags |= O_CREAT;
         }
         if( flags & kXR_open_wrto ){
            openflags |= O_WRONLY;
         } else if ( flags & kXR_open_updt ){
            openflags |= O_RDWR;
         } else {
            openflags |= O_RDONLY;
         }
         if( flags & kXR_delete )
            openflags |= O_TRUNC;
         if( flags & kXR_mkdir ){
            if( mkpath( (char *)fileurl.c_str(), mode ) == -1 )
               log->Error(FileMsg, "Mkpath failed");
         }
         //---------------------------------------------------------------------
         // Open File
         //---------------------------------------------------------------------
         fd = open( fileurl.c_str(), openflags, mode  );
         if( fd == -1 )
         {
            log->Error( FileMsg, "Unable to open %s: %s",
                                    fileurl.c_str(), strerror( errno ) );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            return QueueTask( st, 0, handler );
         }
         else{ st = new XRootDStatus( stOK ); }
         //---------------------------------------------------------------------
         // Stat File and cache statInfo in openInfo
         //---------------------------------------------------------------------
         struct stat ssp;
         if( fstat( fd, &ssp ) == -1 )
         {
            log->Error( FileMsg, "Unable to stat in Open" );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            return QueueTask( st, 0, handler );
         }

         std::ostringstream data;
         data<<ssp.st_dev <<" "<< ssp.st_size <<" "<<ssp.st_mode<<" "<<ssp.st_mtime;

         if( !statInfo->ParseServerResponse( data.str().c_str() ) ){
            log->Error( FileMsg, "Unable to ParseServerResponse for Local File Stat Open" );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            return QueueTask( st, 0, handler );
         }

         uint8_t ufd = fd;
         OpenInfo *openInfo = new OpenInfo( &ufd, 1, statInfo );
         obj->Set( openInfo );
         return QueueTask( new XRootDStatus(), obj, handler );
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

         if( close( fd ) == -1 )
         {
            log->Error( FileMsg, "Unable to close file fd: %i", fd );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            return QueueTask( st, 0, handler );
         }
         else{
            st = new XRootDStatus( stOK );
            return QueueTask( st, obj, handler );
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
         StatInfo *statInfo = new StatInfo();
         obj->Set(statInfo);

         struct stat ssp;
         if( fstat( fd, &ssp ) == -1 || true )
         {
            log->Error( FileMsg, "Unable to stat fd: %i in lFileHandler", fd );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            return QueueTask( st, 0, handler );
         }
         std::ostringstream data;
         data<< ssp.st_dev << " " << ssp.st_size << " " << ssp.st_mode << " " << ssp.st_mtime;
         log->Debug( FileMsg, data.str().c_str() );

         if( !statInfo->ParseServerResponse(data.str().c_str()) ){
            log->Error(FileMsg, "Unable to ParseServerResponse for lFileHandler statinfo" );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            return QueueTask( st, 0, handler );
         }
         else{
            st = new XRootDStatus( stOK );
            return QueueTask( st, obj, handler );
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
         ChunkInfo *resp;

         int read = 0;
         if( ( read = pread( fd, buffer, size, offset ) ) == -1 ){
            log->Error( FileMsg, "Unable to read local file" );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            return QueueTask( st, 0, handler );
         }
         else{
            resp = new ChunkInfo( offset, read,/*size,*/ buffer );
            //TO DO: What is supposed to happen in case readBytes<size? Is the 
            //chunkinfo supposed to have size or read as chunkinfo size?
            obj->Set(resp);
            st = new XRootDStatus( stOK );
            log->Dump( FileMsg, "Chunkinfo: size: %i, offset: %i, Buffer: %s",
                              resp->length, resp->offset, resp->buffer );
            return QueueTask( st, obj, handler );
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
         int written = -1;
         if(( written = pwrite( fd, buffer, size, offset ) ) <= 0 ){
            log->Error( FileMsg, "Unable to write to localfile, wrote %i bytes", written );
            st = new XRootDStatus( stError, errErrorResponse, errno );
            return QueueTask( st, 0, handler );
         }
         else{
            log->Dump( FileMsg, "Write succeeded, wrote %i bytes", written );
            st = new XRootDStatus( stOK );
            return QueueTask( st, obj, handler );
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

         if( syncfs( fd ) == -1 ){
            log->Error( FileMsg, "Unable to Sync, filedescriptor: %i", fd);
            st = new XRootDStatus( stError, errErrorResponse, errno );
            return QueueTask( st, 0, handler );
         }
         else{
            st = new XRootDStatus( stOK );
            return QueueTask( st, obj, handler );
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

         if( ftruncate( fd, size ) == -1 ){
            log->Error( FileMsg, "Unable to Truncate, filedescriptor: %i", fd);
            st = new XRootDStatus( stError, errErrorResponse, errno );
            return QueueTask( st, 0, handler );
         }
         else{
            st = new XRootDStatus( stOK );
            return QueueTask( st, obj, handler );
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
            return QueueTask( st, 0, handler );
         }
         else{
            info->GetChunks() = chunks;
            obj->Set(info);
            for(auto chunk : chunks){
            log->Dump( FileMsg, "Chunkinfo: size: %i, offset: %i, Buffer: %s",
               chunk.length, chunk.offset, chunk.buffer );
            }
            return QueueTask( new XRootDStatus(), obj, handler );
         }
      }
      //------------------------------------------------------------------------
      // QueueTask - queues error/success tasks for all operations.
      // Must always return stOK.
      // Is always creating the same hostlist containing only localhost.
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::QueueTask( XRootDStatus *st, AnyObject *obj,
                                    ResponseHandler *handler )
      {
            HostList* hosts = new HostList();
            hosts->push_back( HostInfo( URL( "localhost" ), true ) );
            LocalFileTask *task = new LocalFileTask( st, obj, hosts, handler );
            jmngr->QueueJob( task );
            return XRootDStatus( stOK );
      }
      //------------------------------------------------------------------------
      // mkpath - creates the folders specified in file_path
      // called if kXR_mkdir flag is set
      //------------------------------------------------------------------------
      int LocalFileHandler::mkpath(char* file_path, mode_t mode) {
        char* p;
        for (p=strchr(file_path+1, '/'); p; p=strchr(p+1, '/')) {
          *p='\0';
          if (mkdir(file_path, mode)==-1) {
            if (errno!=EEXIST) { *p='/'; return -1; }
          }
          *p='/';
        }
        return 0;
      }
}
