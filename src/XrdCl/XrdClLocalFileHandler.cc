#include "XrdCl/XrdClLocalFileHandler.hh"
#include <string>
#include <iostream>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h> 
#include <sys/stat.h>
namespace XrdCl{
    LocalFileHandler::LocalFileHandler()
    {
        Log *log = DefaultEnv::GetLog();
        log->Debug( 0x150, "lFileHandler Constructor");
        jmngr=new JobManager(3);
        jmngr->Initialize();
        jmngr->Start();
    }

    LocalFileHandler::~LocalFileHandler()
    {
    }

    XRootDStatus LocalFileHandler::Open(const std::string& url, uint16_t flags, uint16_t mode, ResponseHandler* handler, uint16_t timeout)
    {
        Log *log = DefaultEnv::GetLog();
        HostList* hosts=new HostList();
        URL *pUrl=new URL("localhost");
        HostInfo info( *pUrl, true );
        hosts->push_back(info);
        mode_t openmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        log->Debug(0x150, "URL in Open is: %s",url.c_str());
        std::string fileurl=url;
        fileurl.erase(0,7);
        log->Debug(0x150, "URL in Open is: %s",fileurl.c_str());
        fd = open( fileurl.c_str(), O_RDWR | O_CREAT, openmode );
        if( fd == -1 )
        {
          log->Debug( 0x150, "Unable to open %s: %s",
                                  fileurl.c_str(), strerror( errno ) );
          return XRootDStatus( stError, errOSError, errno );
        }
        uint8_t ufd=fd;
        OpenInfo *openInfo=new OpenInfo(&ufd,1);
        AnyObject *obj= new AnyObject();
        obj->Set(openInfo);
        LocalFileTask *task= new LocalFileTask(new XRootDStatus(stOK), obj, hosts, handler );
        if(jmngr){
            jmngr->QueueJob(task);
        }
        return XRootDStatus(stOK);
    }

    XRootDStatus LocalFileHandler::Close(ResponseHandler* handler, uint16_t timeout)
    {
        Log *log = DefaultEnv::GetLog();
        log->Debug( 0x150, "In lFileHandler->Close()");
        AnyObject *obj= new AnyObject();
        HostList* hosts=new HostList();
        URL *pUrl=new URL("localhost");
        HostInfo info( *pUrl, true );
        hosts->push_back(info);
        close(fd);
        LocalFileTask *task= new LocalFileTask(new XRootDStatus(stOK), obj, hosts, handler );
        if(jmngr){
            jmngr->QueueJob(task);
        }
        return XRootDStatus(stOK);
    }

    XRootDStatus LocalFileHandler::Stat(bool force, ResponseHandler* handler, uint16_t timeout)
    {
        Log *log = DefaultEnv::GetLog();
        log->Debug( 0x150, "In lFileHandler->Stat()");
        
        struct ::stat ssp;
        if( fstat( fd, &ssp ) == -1 )
        {
          log->Debug( 0x150, "Unable to stat" );
          close( fd );
          return XRootDStatus( stError, errOSError, errno );
        }
        StatInfo *statInfo=new StatInfo();
        std::ostringstream data;
        data<<ssp.st_dev <<" "<< ssp.st_size <<" "<<ssp.st_mode<<" "<<ssp.st_mtime ;
        log->Debug(0x150, data.str().c_str());
        AnyObject *obj= new AnyObject();
  
        if(!statInfo->ParseServerResponse(data.str().c_str())){
            log->Debug(0x150, "ParseServerResponse failed in Stat");
        }
        else{
            log->Debug(0x150, "Set obj->statinfo");
            obj->Set(statInfo);
        }
        HostList* hosts=new HostList();
        URL *pUrl=new URL("localhost");
        HostInfo info( *pUrl, true );
        hosts->push_back(info);
        LocalFileTask *task= new LocalFileTask(new XRootDStatus(stOK), obj, hosts, handler );
        if(jmngr){
            jmngr->QueueJob(task);
        }
        return XRootDStatus(stOK);
    }

    XRootDStatus LocalFileHandler::Read(uint64_t offset, uint32_t size, void* buffer, ResponseHandler* handler, uint16_t timeout)
    {
        Log *log = DefaultEnv::GetLog();
        AnyObject *obj= new AnyObject();
        HostList* hosts=new HostList();
        XRootDStatus* st=new XRootDStatus(stOK);
        URL *pUrl=new URL("localhost");
        HostInfo info( *pUrl, true );
        hosts->push_back(info);
        log->Debug( 0x150, "LocalFileHandler::Read()");
        
        if(pread(fd, buffer, size, offset)==-1)
            log->Debug(0x150, "Read failed in Read()");     
        ChunkInfo *resp=new ChunkInfo(offset, size, buffer);
        obj->Set(resp);
        log->Debug(0x150, "LocalFileHandler::Read(): Chunkinfo: size: %i, offset: %i, Buffer: %s",  
                            resp->length, resp->offset, resp->buffer);

        log->Debug( 0x150, "LocalFileHandler::Read(): creating task");
        LocalFileTask *task= new LocalFileTask(st, obj, hosts, handler );
        if(jmngr){
            log->Debug( 0x150, "LocalFileHandler::Read(): Queue Job");
            jmngr->QueueJob(task);
        }
        return XRootDStatus(XrdCl::stOK,0,0,"");
    }

    XRootDStatus LocalFileHandler::Write(uint64_t offset, uint32_t size, const void* buffer, ResponseHandler* handler, uint16_t timeout)
    {
        Log *log = DefaultEnv::GetLog();
        log->Debug( 0x150, "In lFileHandler->Write()");
        AnyObject *obj= new AnyObject();
        
        log->Debug( 0x150, "fd: %i", fd);
        log->Debug( 0x150, "size: %i", size);
        int written=-1;
        if((written = write(fd,buffer,size))<=0){
            log->Debug(0x150, "write failed, wrote %i", written);
        }
        else{
            log->Debug(0x150, "write succeeded, wrote %i", written);
        }
        
        HostList* hosts=new HostList();
        URL *pUrl=new URL("localhost");
        HostInfo info( *pUrl, true );
        hosts->push_back(info);
        LocalFileTask *task= new LocalFileTask(new XRootDStatus(stOK), obj, hosts, handler );
        if(jmngr){
            jmngr->QueueJob(task);
        }
        return XRootDStatus(stOK);
    }

    XRootDStatus LocalFileHandler::Sync(ResponseHandler* handler, uint16_t timeout)
    {
        Log *log = DefaultEnv::GetLog();
        log->Debug( 0x150, "In lFileHandler->Sync()");
        return XRootDStatus(stError, errNotImplemented);
    }

    XRootDStatus LocalFileHandler::Truncate(uint64_t size, ResponseHandler* handler, uint16_t timeout)
    {
        Log *log = DefaultEnv::GetLog();
        log->Debug( 0x150, "In lFileHandler->Truncate()");
        return XRootDStatus(stError, errNotImplemented);
    }

    XRootDStatus LocalFileHandler::VectorRead(const ChunkList& chunks, void* buffer, ResponseHandler* handler, uint16_t timeout)
    {
        Log *log = DefaultEnv::GetLog();
        log->Debug( 0x150, "In lFileHandler->VectorRead()");
        return XRootDStatus(stError, errNotImplemented);
    }

}
