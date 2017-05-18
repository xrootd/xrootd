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
#include "XrdCl/XrdClPostMaster.hh"
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
         jmngr = DefaultEnv::GetPostMaster()->GetJobManager();
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

            return QueueTask( new XRootDStatus( stError, errErrorResponse, errno ),
                                                0, handler );
         }
         //---------------------------------------------------------------------
         // Stat File and cache statInfo in openInfo
         //---------------------------------------------------------------------
         struct stat ssp;
         if( fstat( fd, &ssp ) == -1 )
         {
            log->Error( FileMsg, "Unable to stat in Open" );
            return QueueTask( new XRootDStatus( stError, errErrorResponse, errno ), 
                                                0, handler );
         }

         std::ostringstream data;
         data<<ssp.st_dev <<" "<< ssp.st_size <<" "<<ssp.st_mode<<" "<<ssp.st_mtime;

         StatInfo *statInfo = new StatInfo();
         if( !statInfo->ParseServerResponse( data.str().c_str() ) ){
            log->Error( FileMsg, "Unable to ParseServerResponse for Local File Stat Open" );
            delete statInfo;
            return QueueTask( new XRootDStatus( stError, errErrorResponse, errno ), 
                                                0, handler );
         }
         //All went well
         uint8_t ufd = fd;
         OpenInfo *openInfo = new OpenInfo( &ufd, 1, statInfo );
         AnyObject *obj = new AnyObject();
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
         if( close( fd ) == -1 )
         {
            log->Error( FileMsg, "Unable to close file fd: %i", fd );
            return QueueTask( new XRootDStatus( stError, errErrorResponse, errno ), 
                                                0, handler );
         }
         else{
            return QueueTask( new XRootDStatus( stOK ), 0, handler );
         }
      }
      //------------------------------------------------------------------------
      // Stat
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::Stat( ResponseHandler* handler,
                                                   uint16_t timeout )
      {
         Log *log = DefaultEnv::GetLog();

         struct stat ssp;
         if( fstat( fd, &ssp ) == -1 )
         {
            log->Error( FileMsg, "Unable to stat fd: %i in lFileHandler", fd );
            return QueueTask( new XRootDStatus( stError, errErrorResponse, errno ), 
                                                0, handler );
         }
         std::ostringstream data;
         data<< ssp.st_dev << " " << ssp.st_size << " " << ssp.st_mode << " " << ssp.st_mtime;
         log->Debug( FileMsg, data.str().c_str() );

         StatInfo *statInfo = new StatInfo();
         if( !statInfo->ParseServerResponse(data.str().c_str()) ){
            log->Error(FileMsg, "Unable to ParseServerResponse for lFileHandler statinfo" );
            delete statInfo;
            return QueueTask( new XRootDStatus( stError, errErrorResponse, errno ), 
                                                0, handler );
         }
         else{
            AnyObject *obj = new AnyObject();
            obj->Set(statInfo);
            return QueueTask( new XRootDStatus( stOK ), obj, handler );
         }
      }
      //------------------------------------------------------------------------
      // Read
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::Read( uint64_t offset, uint32_t size,
               void* buffer, ResponseHandler* handler, uint16_t timeout )
      {
         Log *log = DefaultEnv::GetLog();
         int read = 0;
         if( ( read = pread( fd, buffer, size, offset ) ) == -1 ){
            log->Error( FileMsg, "Unable to read local file" );
            return QueueTask( new XRootDStatus( stError, errErrorResponse, errno ), 
                                                0, handler );
         }
         else{
            ChunkInfo *resp;
            resp = new ChunkInfo( offset, read, buffer );
            AnyObject *obj = new AnyObject();
            obj->Set(resp);
            log->Dump( FileMsg, "Chunkinfo: size: %i, offset: %i, Buffer: %s",
                              resp->length, resp->offset, resp->buffer );
            return QueueTask( new XRootDStatus( stOK ), obj, handler );
         }
      }
      //------------------------------------------------------------------------
      // Write
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::Write( uint64_t offset, uint32_t size,
            const void* buffer, ResponseHandler* handler, uint16_t timeout )
      {
         Log *log = DefaultEnv::GetLog();
         std::string toBeWritten( (char*) buffer, '\0' );
         int written = -1;
         if(( written = pwrite( fd, buffer, size, offset ) ) < (int)size ){
            log->Error( FileMsg, "Retrying to write to localfile, wrote %i bytes", written );

            while( written != -1 && size != 0 ){
                std::string partialWrite = toBeWritten.substr( offset += written, size -= written );
                written = pwrite( fd, partialWrite.c_str(), size, offset );
            }
            if( size != 0 ){
                log->Error( FileMsg, "Retrying to write to localfile, wrote %i bytes", written );
                return QueueTask( new XRootDStatus( stError, errErrorResponse, errno ), 
                                                0, handler );
            }
            else{
                log->Dump( FileMsg, "Write succeeded, wrote %i bytes", offset );
                return QueueTask( new XRootDStatus( stOK ), 0, handler );
            }
         }
         else{
            log->Dump( FileMsg, "Write succeeded, wrote %i bytes", written );
            return QueueTask( new XRootDStatus( stOK ), 0, handler );
         }
      }
      //------------------------------------------------------------------------
      // Sync
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::Sync( ResponseHandler* handler,
                                                   uint16_t timeout )
      {
         Log *log = DefaultEnv::GetLog();

         if( syncfs( fd ) == -1 ){
            log->Error( FileMsg, "Unable to Sync, filedescriptor: %i", fd);
            return QueueTask( new XRootDStatus( stError, errErrorResponse, errno ), 
                                                0, handler );
         }
         else{
            return QueueTask( new XRootDStatus( stOK ), 0, handler );
         }
      }
      //------------------------------------------------------------------------
      // Truncate
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::Truncate(uint64_t size,
               ResponseHandler* handler, uint16_t timeout)
      {
         Log *log = DefaultEnv::GetLog();

         if( ftruncate( fd, size ) == -1 ){
            log->Error( FileMsg, "Unable to Truncate, filedescriptor: %i", fd);
            return QueueTask( new XRootDStatus( stError, errErrorResponse, errno ), 
                                                0, handler );
         }
         else{
            return QueueTask( new XRootDStatus( stOK ), 0, handler );
         }
      }
      //------------------------------------------------------------------------
      // VectorRead
      //------------------------------------------------------------------------
      XRootDStatus LocalFileHandler::VectorRead( const ChunkList& chunks,
               void* buffer, ResponseHandler* handler, uint16_t timeout )
      {
         Log *log = DefaultEnv::GetLog();

         int iovcnt;
         struct iovec iov[chunks.size()];
         iovcnt = sizeof(iov) / sizeof(struct iovec);
         for( unsigned int i = 0; i < chunks.size(); i++ ){
            iov[i].iov_base = chunks[i].buffer;
            iov[i].iov_len = chunks[i].length;
         }

         if( readv( fd, iov, iovcnt ) == -1 ){
            log->Error( FileMsg, "Unable to VectorRead, filedescriptor: %i", fd);
            return QueueTask( new XRootDStatus( stError, errErrorResponse, errno ), 
                                                0, handler );
         }
         else{
            VectorReadInfo *info = new VectorReadInfo();
            info->GetChunks() = chunks;
            AnyObject *obj = new AnyObject();
            obj->Set( info );
            for( uint32_t i = 0; i < chunks.size(); i++ ){
            log->Dump( FileMsg, "Chunkinfo: size: %i, offset: %i, Buffer: %s",
               chunks[i].length, chunks[i].offset, chunks[i].buffer );
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
            HostList *hosts = new HostList();
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
        char *p;
        for ( p = strchr( file_path + 1, '/' ); p; p = strchr( p + 1, '/' ) ) {
          *p='\0';
          if ( mkdir( file_path, mode ) == -1 ) {
            if ( errno != EEXIST ) { *p = '/'; return -1; }
          }
          *p = '/';
        }
        return 0;
      }
}
