/******************************************************************************/
/*                                                                            */
/*                          X r d O s s A i o . c c                           */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//         $Id$

const char *XrdOssAioCVSID = "$Id$";

#ifdef _POSIX_ASYNCHRNOUS_IO
#include <aio.h>
#endif
#include <signal.h>
#include <unistd.h>

#include "XrdOss/XrdOssApi.hh"
#include "XrdOss/XrdOssTrace.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdSfs/XrdSfsAio.hh"

// All AIO interfaces are defined here.
 
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
extern XrdOucTrace OssTrace;
#define tident aiop->TIdent;

extern XrdOucError OssEroute;

int   XrdOssFile::AioFailure = 0;

#ifdef _POSIX_ASYNCHRNOUS_IO
#define OSS_AIO_READ_DONE  (SIGRTMIN)
#define OSS_AIO_WRITE_DONE (SIGRTMIN+1)
#endif

/******************************************************************************/
/*                                 F s y n c                                  */
/******************************************************************************/
  
/*
  Function: Async fsync() a file

  Input:    aiop      - A aio request object
*/

int XrdOssFile::Fsync(XrdSfsAio *aiop)
{

#ifdef _POSIX_ASYNCHRNOUS_IO

// Complete the aio request block and do the operation
//
   if (XrdOssSys::AioAllOk)
      {aiop->sfsAio.fildes = fd;
       aiop->sfsAio.aio_sigevent.sigev_signo  = SIG_AIO_WRITE_DONE;
       aiop->TIdent = tident;

      // Start the operation
      //
         if (!(rc = aio_fsync(O_SYNC, &aiop->sfsAio)) return 0;
         if (errno != EAGAIN && errno != ENOSYS) return -errno;

      // Aio failed keep track of the problem (msg every 1024 events). Note
      // that the handling of the counter is sloppy because we do not lock it.
      //
         {int fcnt = AioFailure++;
          if (fcnt & 0x3ff == 1) OssEroute.Emsg("aio", errno, "fsync async");
         }
     }
#endif

// Execute this request in a synchronous fashion
//
   if ((aiop->Result = Fsync())) aiop->Result = -errno;

// Simply call the write completion routine and return as if all went well
//
   aiop->doneWrite();
   return 0;
}

/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/

/*
  Function: Async read `blen' bytes from the associated file, placing in 'buff'

  Input:    aiop      - An aio request object

/* Output:  <0 -> Operation failed, value is negative errno value.
            =0 -> Operation queued
            >0 -> Operation not queued, system resources unavailable or
                                        asynchronous I/O is not supported.
*/
  
int XrdOssFile::Read(XrdSfsAio *aiop)
{

#ifdef _POSIX_ASYNCHRNOUS_IO

// Complete the aio request block and do the operation
//
   if (XrdOssSys::AioAllOk)
      {aiop->sfsAio.fildes = fd;
       aiop->sfsAio.aio_sigevent.sigev_signo  = SIG_AIO_READ_DONE;
       aiop->TIdent = tident;

       // Start the operation
       //
          if (!(rc = aio_read(&aiop->sfsAio)) return 0;
          if (errno != EAGAIN && errno != ENOSYS) return -errno;

      // Aio failed keep track of the problem (msg every 1024 events). Note
      // that the handling of the counter is sloppy because we do not lock it.
      //
         {int fcnt = AioFailure++;
          if (fcnt & 0x3ff == 1) OssEroute.Emsg("aio", errno, "read async");
         }
     }
#endif

// Execute this request in a synchronous fashion
//
   if ((aiop->Result = this->Read((void *)aiop->sfsAio.aio_buf,
                                   (off_t)aiop->sfsAio.aio_offset,
                                  (size_t)aiop->sfsAio.aio_nbytes)) < 0)
      aiop->Result = -errno;

// Simple call the read completion routine and return as if all went well
//
   aiop->doneRead();
   return 0;
}

/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/
  
/*
  Function: Async write `blen' bytes from 'buff' into the associated file

  Input:    aiop      - An aio request object.

/* Output:  <0 -> Operation failed, value is negative errno value.
            =0 -> Operation queued
            >0 -> Operation not queued, system resources unavailable or
                                        asynchronous I/O is not supported.
*/
  
int XrdOssFile::Write(XrdSfsAio *aiop)
{
#ifdef _POSIX_ASYNCHRNOUS_IO

// Complete the aio request block and do the operation
//
   if (XrdOssSys::AioAllOk)
      {aiop->sfsAio.fildes = fd;
       aiop->sfsAio.aio_sigevent.sigev_signo  = SIG_AIO_WRITE_DONE;
       aiop->TIdent = tident;

       // Start the operation
       //
          if (!(rc = aio_write(&aiop->sfsAio)) return 0;
          if (errno != EAGAIN && errno != ENOSYS) return -errno;

       // Aio failed keep track of the problem (msg every 1024 events). Note
       // that the handling of the counter is sloppy because we do not lock it.
       //
          {int fcnt = AioFailure++;
           if (fcnt & 0x3ff == 1) OssEroute.Emsg("Write", errno, "write async");
          }
      }
#endif

// Execute this request in a synchronous fashion
//
   if ((aiop->Result = this->Write((const void *)aiop->sfsAio.aio_buf,
                                          (off_t)aiop->sfsAio.aio_offset,
                                         (size_t)aiop->sfsAio.aio_nbytes)) < 0)
      aiop->Result = -errno;

// Simply call the write completion routine and return as if all went well
//
   aiop->doneWrite();
   return 0;
}

/******************************************************************************/
/*                 X r d O s s S y s   A I O   M e t h o d s                  */
/******************************************************************************/
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

int   XrdOssSys::AioAllOk = 0;
  
/******************************************************************************/
/*                               A i o I n i t                                */
/******************************************************************************/
/*
  Function: Initialize for AIO processing.

  Return:   True if successful, false otherwise.
*/

int XrdOssSys::AioInit()
{
#ifdef _POSIX_ASYNCHRNOUS_IO
   EPNAME("AioInit");
   extern void *XrdOssAioWait(void *carg);
   pthread_t tid;
   int retc;

// The AIO signal handler consists of two thread (one for read and one for
// write) that synhronously wait for AIO events. We assume, blithely, that
// the first two real-time signals have been blocked for all threads.
//
   if ((retc = XrdOucThread_Run(&tid, XrdOssAioWait,
                                (void *)OSS_AIO_READ_DONE)) < 0)
      {Eroute.Emsg("AioInit", retc, "creating AIO read signal thread; "
                                    "AIO support terminated.");
       DEBUG("started AIO read signal thread; tid=" <<(unsigned int)tid);
      } else {
   if ((retc = XrdOucThread_Run(&tid, XrdOssAioWait,
                                (void *)OSS_AIO_WRITE_DONE)) < 0)
      {Eroute.Emsg("AioInit", retc, "creating AIO write signal thread; "
                                    "AIO support terminated.");
       DEBUG("started AIO write signal thread; tid=" <<(unsigned int)tid);
      } else AioAllOK = 1;

// All done
//
   return AioAllOk;
#else
   return 0;
#endif
}

/******************************************************************************/
/*                               A i o W a i t                                */
/******************************************************************************/
  
void *XrdOssAioWait(int mySignum)
{
#ifdef _POSIX_ASYNCHRNOUS_IO
   EPNAME("AioWait");
   const char *sigType = (mySignum == OSS_AIO_READ_DONE ? "read" : "write");
   const int  isRead   = (mySignum == OSS_AOI_READ_DONE);
   sigset_t  mySig;
   siginfo_t myInfo;
   XrdSfsAio *aiop;
   int rc, numsig;
   ssize_t retval;

// Initialize the signal we will be waiting for
//
   sigemptyset(&mySig);
   sigaddset(&mySig, mySignum);

// Simply wait for events and requeue the completed AIO operation
//
   do {do {numsig = sigwaitinfo(&mySig, &myInfo);}
          while (numsig < 0 && errno == EINTR);
       if (numsig < 0)
          {Eroute.Emsg("AioWait", errno, sigType, "wait for AIO signal");
           XrdOssSys::AioAllOK = 0;
           break;
          }
       if (numsig != mySig || myInfo.si_code != SI_ASYNCIO)
          {char buff[32];
           sprintf(buff, "%d", numsig);
           Eroute.Emsg("AioWait", "received unexpected signal", buff);
           continue;
          }

       aiop = (XrdSfsAio *)signalInfo.si_value.sival_ptr;

       while ((rc = aio_error(aiop->sfsAio)) == EINPROGRESS);
       retval = (ssize_t)aio_return(aiop->sfsAio);

       TRACE(AIO, sigType <<" completed; rc=" <<rc <<" result=" <<result);

       if (retval < 0) aiop->Result = -rc;
          else         aiop->Result = retval;

       if (isRead) aiop->doneRead();
          else     aiop->doneWrite();
      }
#endif
   return (void *)0;
}
