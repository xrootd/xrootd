/******************************************************************************/
/*                                                                            */
/*                             X r d O f s . c c                              */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*               DE-AC03-76-SFO0515 with the Deprtment of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdOfsCVSID = "$Id$";

/* Available compile-time define symbols:

   -DAIX       mangles some includes to accomodate AIX.

   -DNODEBUG   suppresses inline dbugging statement.

   -DNOSEC     suppresses security code generation.

   Note: This is a C++ mt-safe 64-bit clean program and must be compiled with:

         Solaris: -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_REENTRANT

         AIX:     -D_THREAD_SAFE
*/

#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <iostream.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "XrdVersion.hh"

#include "XrdOfs/XrdOfs.hh"
#include "XrdOfs/XrdOfsConfig.hh"
#include "XrdOfs/XrdOfsTrace.hh"
#include "XrdOfs/XrdOfsOpaque.hh"
#include "XrdOfs/XrdOfsSecurity.hh"

#include "XrdOss/XrdOssApi.hh"
#include "XrdOss/XrdOssProxy.hh"

#include "XrdNet/XrdNetDNS.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLock.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSfs/XrdSfsInterface.hh"

#ifdef AIX
#include <sys/mode.h>
#endif
// IOS_USING_DECLARATION_MARKER - BaBar iostreams migration, do not touch this line!

/******************************************************************************/
/*                       C u r i o u s   D e f i n e s                        */
/******************************************************************************/
  
#ifndef S_IAMB
#define S_IAMB  0x1FF
#endif

/******************************************************************************/
/*                  E r r o r   R o u t i n g   O b j e c t                   */
/******************************************************************************/

XrdOucError      OfsEroute(0);

XrdOucTrace      OfsTrace(&OfsEroute);

/******************************************************************************/
/*                    F i l e   S y s t e m   O b j e c t                     */
/******************************************************************************/
  
XrdOfs XrdOfsFS;

/******************************************************************************/
/*                 S t o r a g e   S y s t e m   O b j e c t                  */
/******************************************************************************/
  
extern XrdOssSys XrdOssSS;

/******************************************************************************/
/*                    E x t e r n a l   F u n c t i o n s                     */
/******************************************************************************/
  
extern unsigned long XrdOucHashVal(const char *);

/******************************************************************************/
/*           F i l e   H a n d l e   M a n a g e m e n t   A r e a            */
/******************************************************************************/

// The following are anchors for filehandles.
//
XrdOfsHandleAnchor XrdOfsOrigin_RO("r/o",0);          // Files open r/o.
XrdOfsHandleAnchor XrdOfsOrigin_RW("r/w",1);          // Files open r/w.

// The following mutexes are used to serialize open processing
//
XrdOucMutex XrdOfsOpen_RO;
XrdOucMutex XrdOfsOpen_RW;

// Functions that manage idle file handles
//
void         *XrdOfsIdleScan(void *);
void          XrdOfsIdleCheck(XrdOfsHandleAnchor &);
int           XrdOfsIdleXeq(XrdOfsHandle *, void *);

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define LOCK(x) \
        if (!x) return XrdOfsFS.Emsg(epname,error,XrdOfsENOTOPEN, "");x->Lock()

#define UNLOCK(x) x->UnLock()

#define UNLK_RETURN(x,y) {UNLOCK(x); return y;}

#define Max(a,b) (a >= b ? a : b)

#define REOPENandHOLD(x) if (x->flags & OFS_TCLOSE && !Unclose()) \
                            UNLK_RETURN(x,SFS_ERROR); \
                         TimeStamp(); x->optod = tod.tv_sec; x->activ++;

#define RELEASE(x) x->Lock(); x->activ--; x->UnLock();

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOfs::XrdOfs()
{
   int i;

// Establish defaults
//
   FDConn        = 0;
   FDOpen        = 0;
   FDOpenMax     = XrdOfsFDOPENMAX;
   FDMinIdle     = XrdOfsFDMINIDLE;
   FDMaxIdle     = XrdOfsFDMAXIDLE;
   LockTries     = XrdOfsLOCKTRIES;
   LockWait      = XrdOfsLOCKWAIT;
   MaxDelay      = 60;
#ifdef __SECURITY__
   Authorization = 0;
#endif
   Finder        = 0;
   Google        = 0;
   Reporter      = 0;
   Balancer      = 0;

// Establish our hostname
//
   HostName      = XrdNetDNS::getHostName();
   for (i = 0; HostName[i] && HostName[i] != '.'; i++);
   HostName[i] = '\0';
   HostPref = strdup(HostName);
   HostName[i] = '.';

// Set the configuration file name.
//
   ConfigFN = 0;
}
  
/******************************************************************************/
/*                         G e t F i l e S y s t e m                          */
/******************************************************************************/
  
extern "C"
{
XrdSfsFileSystem *XrdSfsGetFileSystem(XrdSfsFileSystem *native_fs, 
                                      XrdOucLogger     *lp,
                                      const char       *configfn)
{
// extern XrdOfs XrdOfsFS;
 pthread_t tid;
 int retc;

// Do the herald thing
//
   OfsEroute.SetPrefix("ofs_");
   OfsEroute.logger(lp);
   OfsEroute.Emsg("Init", "(c) 2004 Stanford University/SLAC, Ofs Version "
                          XrdVSTRING);

// Initialize the subsystems
//
   XrdOfsFS.ConfigFN = (configfn && *configfn ? strdup(configfn) : 0);
   if ( XrdOfsFS.Configure(OfsEroute) ) return 0;
   XrdOfsFS.Config_Display(OfsEroute);

// Initialize the target storage system
//
   if (XrdOssSS.Init(lp, configfn)) return 0;

// Start a thread to periodically scan for idle file handles
//
   if ((retc = XrdOucThread::Run(&tid, XrdOfsIdleScan, (void *)0)))
      OfsEroute.Emsg("XrdOfsinit", retc, "create idle scan thread");

// All done, we can return the callout vector to these routines.
//
   return &XrdOfsFS;
}
}

/******************************************************************************/
/*                                                                            */
/*           D i r e c t o r y   O b j e c t   I n t e r f a c e s            */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/*                                  o p e n                                   */
/******************************************************************************/

int XrdOfsDirectory::open(const char              *dir_path, // In
                          const XrdSecClientName  *client)   // In
/*
  Function: Open the directory `path' and prepare for reading.

  Input:    path      - The fully qualified name of the directory to open.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success, otherwise SFS_ERROR.

  Notes: 1. The code here assumes that directory file descriptors are never
            shared. Hence, no locks need to be obtained. It works out that
            lock overhead is worse than have a duplicate file descriptor for
            very short durations.
*/
{
   static const char *epname = "opendir";
   int retc;

// Iniialize tracing now
//
   tident = client->tident;
   error.setErrUser(tident);
   XTRACE(opendir, dir_path, "");

// Verify that this object is not already associated with an open directory
//
   if (dp) return
      XrdOfsFS.Emsg(epname, error, EADDRINUSE, "open directory", dir_path);

// Apply security, as needed
//
   if (XrdOfsFS.VPlist.NotEmpty() && !XrdOfsFS.VPlist.Find(dir_path))
      return XrdOfsFS.Emsg(epname, error, EACCES, "list", dir_path);
   AUTHORIZE(client,AOP_Readdir,"opening directory",dir_path,error,SFS_ERROR);

// Open the directory and allocate a handle for it
//
   if (!(dp = new XrdOssDir((const char *)tident))) retc = -ENOMEM;
      else if (!(retc = dp->Opendir(dir_path)))
              {fname = strdup(dir_path);
               return SFS_OK;
              }
              else {delete dp; dp = 0;}

// Encountered an error
//
   return XrdOfsFS.Emsg(epname, error, retc, "open directory", dir_path);
}

/******************************************************************************/
/*                             n e x t E n t r y                              */
/******************************************************************************/

const char *XrdOfsDirectory::nextEntry()
/*
  Function: Read the next directory entry.

  Input:    n/a

  Output:   Upon success, returns the contents of the next directory entry as
            a null terminated string. Returns a null pointer upon EOF or an
            error. To differentiate the two cases, getErrorInfo will return
            0 upon EOF and an actual error code (i.e., not 0) on error.

  Notes: 1. The code here assumes that idle directory file descriptors are
            *not* closed. This needs to be the case because we need to return
            non-duplicate directory entries. Anyway, the xrootd readdir protocol
            is handled internally so directories should never be idle.
         2. The code here assumes that directory file descriptors are never
            shared. Hence, no locks need to be obtained. It works out that
            lock overhead is worse than have a duplicate file descriptor for
            very short durations.
*/
{
   static const char *epname = "readdir";
   int retc;

// Check if this directory is actually open
//
   if (!dp) {XrdOfsFS.Emsg(epname, error, XrdOfsENOTOPEN, "read directory");
             return (const char *)0;
            }

// Check if we are at EOF (once there we stay there)
//
   if (atEOF) return (const char *)0;

// Read the next directory entry
//
   if ((retc = dp->Readdir(dname, sizeof(dname))) < 0)
      {XrdOfsFS.Emsg(epname, error, retc, "read directory", fname);
       return (const char *)0;
      }

// Check if we have reached end of file
//
   if (!*dname)
      {atEOF = 1;
       error.clear();
       XTRACE(readdir, fname, "<eof>");
       return (const char *)0;
      }

// Return the actual entry
//
   XTRACE(readdir, fname, dname);
   return (const char *)(dname);
}

/******************************************************************************/
/*                                 c l o s e                                  */
/******************************************************************************/
  
int XrdOfsDirectory::close()
/*
  Function: Close the directory object.

  Input:    n/a

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.

  Notes: 1. The code here assumes that directory file descriptors are never
            shared. Hence, no locks need to be obtained. It works out that
            lock overhead is worse than have a duplicate file descriptor for
            very short durations.
*/
{
   static const char *epname = "closedir";
   int retc;

// Check if this directory is actually open
//
   if (!dp) {XrdOfsFS.Emsg(epname, error, EBADF, "close directory");
             return SFS_ERROR;
            }
   XTRACE(closedir, fname, "");

// Close this directory
//
    if ((retc = dp->Close()))
       retc = XrdOfsFS.Emsg(epname, error, retc, "close", fname);
       else retc = SFS_OK;

// All done
//
   delete dp;
   dp = 0;
   free(fname);
   fname = 0;
   return retc;
}

/******************************************************************************/
/*                                                                            */
/*                F i l e   O b j e c t   I n t e r f a c e s                 */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/*                                  o p e n                                   */
/******************************************************************************/

int XrdOfsFile::open(const char          *path,      // In
                     XrdSfsFileOpenMode   open_mode, // In
                     mode_t               Mode,      // In
               const XrdSecClientName    *client,    // In
               const char                *info)      // In
/*
  Function: Open the file `path' in the mode indicated by `open_mode'.  

  Input:    path      - The fully qualified name of the file to open.
            open_mode - One of the following flag values:
                        SFS_O_RDONLY - Open file for reading.
                        SFS_O_WRONLY - Open file for writing.
                        SFS_O_RDWR   - Open file for update
                        SFS_O_CREAT  - Create the file open in RW mode
                        SFS_O_TRUNC  - Trunc  the file open in RW mode
            Mode      - The Posix access mode bits to be assigned to the file.
                        These bits correspond to the standard Unix permission
                        bits (e.g., 744 == "rwxr--r--"). Additionally, Mode
                        may contain SFS_O_MKPTH to force creation of the full
                        directory path if it does not exist. This parameter is
                        ignored unless open_mode = SFS_O_CREAT.
            client    - Authentication credentials, if any.
            info      - Opaque information to be used as seen fit.

  Output:   Returns SFS_OK upon success, otherwise SFS_ERROR is returned.
*/
{
   static const char *epname = "open";
   const char        *hostname = 0;
   int retc, odc_mode, open_flag = 0, port = 0;
   int mkpath = Mode & SFS_O_MKPTH;
   unsigned long hval = XrdOucHashVal(path);
   XrdOucMutex        *mp;
   XrdOfsHandleAnchor *ap;
   XrdOssDF           *fp;
   XrdOucEnv Open_Env(info);

// Setup tracing
//
   tident = client->tident;
   error.setErrUser(tident);
   ZTRACE(open, std::hex <<open_mode <<"-" <<std::oct <<Mode <<std::dec <<" fn=" <<path);

// Verify that this object is not already associated with an open file
//
   if (oh) return XrdOfsFS.Emsg(epname,error,EADDRINUSE,"open file",path);

// Verify that we can use this path
//
   if (XrdOfsFS.VPlist.NotEmpty() && !XrdOfsFS.VPlist.Find(path))
      return XrdOfsFS.Emsg(epname, error, EACCES, "open", path);
   error.setErrInfo(0, (char *)"");

// Set the actual open mode
//
   odc_mode = (open_mode & SFS_O_RESET ? O_EXCL : 0);
   if (open_mode & SFS_O_CREAT) open_mode = SFS_O_CREAT;
      else if (open_mode & SFS_O_TRUNC) open_mode = SFS_O_TRUNC;

   switch(open_mode & (SFS_O_RDONLY | SFS_O_WRONLY | SFS_O_RDWR |
                       SFS_O_CREAT  | SFS_O_TRUNC))
   {
   case SFS_O_CREAT:  open_flag   = O_EXCL;
   case SFS_O_TRUNC:  open_flag  |= O_RDWR | O_CREAT | O_TRUNC;
                      ap = &XrdOfsOrigin_RW; mp = &XrdOfsOpen_RW;
                      break;
   case SFS_O_RDONLY: open_flag = O_RDONLY;
                      ap = &XrdOfsOrigin_RO; mp = &XrdOfsOpen_RO;
                      break;
   case SFS_O_WRONLY: open_flag = O_WRONLY;
                      ap = &XrdOfsOrigin_RW; mp = &XrdOfsOpen_RW;
                      break;
   case SFS_O_RDWR:   open_flag = O_RDWR;
                      ap = &XrdOfsOrigin_RW; mp = &XrdOfsOpen_RW;
                      break;
   default:           open_flag = O_RDONLY;
                      ap = &XrdOfsOrigin_RO; mp = &XrdOfsOpen_RO;
                      break;
   }


// If we have a finder object, use it to direct the client. The final
// destination will apply the security that is needed
//
   if (XrdOfsFS.Finder && (retc = XrdOfsFS.Finder->Locate(error, path,
                odc_mode|(open_flag & ~O_EXCL))))
      return XrdOfsFS.fsError(error, retc);

// Create the file if so requested o/w try to attach the file
//
   if (open_mode & SFS_O_CREAT)
      {// Apply security, as needed
       //
       AUTHORIZE(client,AOP_Create,"creating",path,error,SFS_ERROR);
       OOIDENTENV(client, Open_Env);

       // Create the file
       //
       open_flag  = O_RDWR; mp = &XrdOfsOpen_RW;
       if ((retc = XrdOssSS.Create(path, Mode & S_IAMB, Open_Env, mkpath)))
          return XrdOfsFS.Emsg(epname, error, retc, "create", path);
       if (XrdOfsFS.Balancer) XrdOfsFS.Balancer->Added(path);
       mp->Lock();

      } else {

       // Apply security, as needed
       //
       AUTHORIZE(client, (open_flag == O_RDONLY ? AOP_Read : AOP_Update),
                         "opening", path, error, SFS_ERROR);
       OOIDENTENV(client, Open_Env);

       // First try to attach the file
       //
       mp->Lock();
       if ((oh = ap->Attach(path)))
          {ZTRACE(open, "attach lnk=" <<oh->links <<" pi=" <<(oh->PHID()) <<" fn=" << (oh->Name()));
           mp->UnLock();
           oh->Lock();  // links > 1 -> handle cannot be deleted; hp is valid
           if (oh->flags & OFS_INPROG)
              {retc = (oh->ecode ? oh->ecode : -ENOMSG);
               XrdOfsFS.Close(oh); oh = 0;
               if (retc > 0) return XrdOfsFS.Stall(error, retc, path);
               return XrdOfsFS.Emsg(epname, error, retc, "attach", path);
              }
           if (oh->cxrsz) setCXinfo(open_mode);
           oh->UnLock();
           return SFS_OK;
          }
      }

// Open the file and allocate a handle for it. Insert into the full chain
// prior to opening, which may take quite a bit of time.
//
// By default, load the XrdOssFile class that is capable of reading the file
// from the local disk as well as stage it. If the data server acts as a proxy,
// load the XrdOssProxy class that can also retrieve data from a remote data 
// server
//
   if (XrdOfsFS.Google) {

      // Check if the file is available locally. If so, open it locally. 
      // Otherwise, search for a remote data server
      // if not local, try to get proxy and retrieve the remote file
      //
      struct stat fstat;
     
      if (XrdOssSS.Stat(path, &fstat) == 0) {
         ZTRACE(open, "The file is found locally.");
         fp = (XrdOssDF *)new XrdOssFile((const char *)tident);
      } else {
         retc = XrdOfsFS.Google->Locate(error, path, odc_mode|open_flag);
      
         if (retc == -EREMOTE) {
            ZTRACE(open, "Found remote data server");
            hostname = error.getErrText();
            port     = error.getErrInfo();

            // If no port number is assigned, use the default port 1094
            //
            if (port == 0) port = XrdDEFAULTPORT;
         }
         else {
               mp->UnLock();
               return retc;
              }

         fp = (XrdOssDF *)new XrdOssProxy(hostname, port);
         ZTRACE(open, "Using " <<hostname <<" as a proxy for " <<path);
      }
   }
   else {fp = (XrdOssDF *)new XrdOssFile((const char *)tident);}

   if ( fp && (oh = new XrdOfsHandle(hval,path,open_flag,tod.tv_sec,ap,fp)) )
      {mp->UnLock();  // Handle is now locked so allow new opens
       if (XrdOfsFS.Reporter) XrdOfsFS.Reporter->UpdateFD(1);

       if ((retc = fp->Open(path, open_flag, Mode, Open_Env)))
          {oh->ecode = retc; XrdOfsFS.Close(oh); oh = 0;
           if (retc > 0) return XrdOfsFS.Stall(error, retc, path);
          } else {


           if ((oh->cxrsz = fp->isCompressed(oh->cxid))) setCXinfo(open_mode);
           oh->Activate(); oh->UnLock();
           return SFS_OK;
          }
      } else {
       mp->UnLock();
       if (fp) delete fp;
       retc = ENOMEM;
      }

// Return an error
//
   return XrdOfsFS.Emsg(epname, error, retc, "open", path);
}

/******************************************************************************/
/*                                 c l o s e                                  */
/******************************************************************************/

int XrdOfsFile::close()  // In
/*
  Function: Close the file object.

  Input:    n/a

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "close";
   XrdOfsHandle *myoh;

// Lock the handle and perform required tracing
//
    LOCK(oh);
    FTRACE(close, "lnks=" <<oh->links); // Unreliable trace, no origin lock

// Release the handle and return
//
    myoh = oh;
    oh = (XrdOfsHandle *)0;
    if (XrdOfsFS.Close(myoh)) {oh = myoh; return SFS_ERROR;}
    return SFS_OK;
}

/******************************************************************************/
/*                                  r e a d                                   */
/******************************************************************************/

int            XrdOfsFile::read(XrdSfsFileOffset  offset,    // In
                                XrdSfsXferSize    blen)      // In
/*
  Function: Preread `blen' bytes at `offset'

  Input:    offset    - The absolute byte offset at which to start the read.
            blen      - The amount to preread.

  Output:   Returns SFS_OK upon success and SFS_ERROR o/w.
*/
{
   static const char *epname = "read";
   int retc;

// Perform required tracing
//
   FTRACE(read, "preread " <<blen <<"@" <<offset);

// Make sure the offset is not too large
//
#if _FILE_OFFSET_BITS!=64
   if (offset >  0x000000007fffffff)
      return  XrdOfsFS.Emsg(epname, error, EFBIG, "read", oh->Name());
#endif

// Reopen the handle if it has been closed
//
   LOCK(oh);
   REOPENandHOLD(oh);
   UNLOCK(oh);

// Now preread the actual number of bytes
//
   retc   = oh->Select().Read((off_t)offset, (size_t)blen);
   RELEASE(oh);
   if (retc < 0)
      return XrdOfsFS.Emsg(epname, error, (int)retc, "preread", oh->Name());

// Return number of bytes read
//
   return retc;
}
  
/******************************************************************************/
/*                                  r e a d                                   */
/******************************************************************************/

XrdSfsXferSize XrdOfsFile::read(XrdSfsFileOffset  offset,    // In
                                char             *buff,      // Out
                                XrdSfsXferSize    blen)      // In
/*
  Function: Read `blen' bytes at `offset' into 'buff' and return the actual
            number of bytes read.

  Input:    offset    - The absolute byte offset at which to start the read.
            buff      - Address of the buffer in which to place the data.
            blen      - The size of the buffer. This is the maximum number
                        of bytes that will be read from 'fd'.

  Output:   Returns the number of bytes read upon success and SFS_ERROR o/w.
*/
{
   static const char *epname = "read";
   XrdSfsXferSize nbytes;

// Perform required tracing
//
   FTRACE(read, blen <<"@" <<offset);

// Make sure the offset is not too large
//
#if _FILE_OFFSET_BITS!=64
   if (offset >  0x000000007fffffff)
      return  XrdOfsFS.Emsg(epname, error, EFBIG, "read", oh->Name());
#endif

// Reopen the handle if it has been closed
//
   LOCK(oh);
   REOPENandHOLD(oh);
   UNLOCK(oh);

// Now read the actual number of bytes
//
   nbytes = (dorawio ?
            (XrdSfsXferSize)(oh->Select().ReadRaw((void *)buff,
                            (off_t)offset, (size_t)blen))
          : (XrdSfsXferSize)(oh->Select().Read((void *)buff,
                            (off_t)offset, (size_t)blen)));
   RELEASE(oh);
   if (nbytes < 0)
      return XrdOfsFS.Emsg(epname, error, (int)nbytes, "read", oh->Name());

// Return number of bytes read
//
   return nbytes;
}
  
/******************************************************************************/
/*                              r e a d   A I O                               */
/******************************************************************************/
  
// For now, this reverts to synchronous I/O
//
int XrdOfsFile::read(XrdSfsAio *aiop)
{
   aiop->Result = this->read((XrdSfsFileOffset)aiop->sfsAio.aio_offset,
                                       (char *)aiop->sfsAio.aio_buf,
                               (XrdSfsXferSize)aiop->sfsAio.aio_nbytes);
   aiop->doneRead();
   return 0;
}

/******************************************************************************/
/*                                 w r i t e                                  */
/******************************************************************************/

XrdSfsXferSize XrdOfsFile::write(XrdSfsFileOffset  offset,    // In
                                 const char       *buff,      // Out
                                 XrdSfsXferSize    blen)      // In
/*
  Function: Write `blen' bytes at `offset' from 'buff' and return the actual
            number of bytes written.

  Input:    offset    - The absolute byte offset at which to start the write.
            buff      - Address of the buffer from which to get the data.
            blen      - The size of the buffer. This is the maximum number
                        of bytes that will be written to 'fd'.

  Output:   Returns the number of bytes written upon success and SFS_ERROR o/w.

  Notes:    An error return may be delayed until the next write(), close(), or
            sync() call.
*/
{
   static const char *epname = "write";
   XrdSfsXferSize nbytes;

// Perform any required tracing
//
   FTRACE(write, blen <<"@" <<offset);

// Make sure the offset is not too large
//
#if _FILE_OFFSET_BITS!=64
   if (offset >  0x000000007fffffff)
      return  XrdOfsFS.Emsg(epname, error, EFBIG, "read", oh->Name());
#endif

// Reopen the file handle if it has been closed
//
   LOCK(oh);
   REOPENandHOLD(oh);
   oh->flags |= OFS_PENDIO;
   UNLOCK(oh);

// Write the requested bytes
//
   nbytes = (XrdSfsXferSize)(oh->Select().Write((const void *)buff,
                            (off_t)offset, (size_t)blen));
   RELEASE(oh);
   if (nbytes < 0)
      return XrdOfsFS.Emsg(epname, error, (int)nbytes, "write", oh->Name());

// Return number of bytes written
//
   return nbytes;
}

/******************************************************************************/
/*                             w r i t e   A I O                              */
/******************************************************************************/
  
// For now, this reverts to synchronous I/O
//
int XrdOfsFile::write(XrdSfsAio *aiop)
{
   aiop->Result = this->write((XrdSfsFileOffset)aiop->sfsAio.aio_offset,
                                        (char *)aiop->sfsAio.aio_buf,
                                (XrdSfsXferSize)aiop->sfsAio.aio_nbytes);
   aiop->doneWrite();
   return 0;
}

/******************************************************************************/
/*                                  s t a t                                   */
/******************************************************************************/

int XrdOfsFile::stat(struct stat     *buf)         // Out
/*
  Function: Return file status information

  Input:    buf         - The stat structiure to hold the results

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "stat";
   int retc;

// Lock the handle and perform any required tracing
//
   FTRACE(stat, "");

// Reopen the handle if it has been closed
//
   LOCK(oh);
   REOPENandHOLD(oh);
   UNLOCK(oh);

// Perform the function
//
   retc = oh->Select().Fstat(buf);
   RELEASE(oh);
   if (retc)
      return XrdOfsFS.Emsg(epname,error,retc,"get state for",oh->Name());

   return SFS_OK;
}

/******************************************************************************/
/*                                  s y n c                                   */
/******************************************************************************/

int XrdOfsFile::sync()  // In
/*
  Function: Commit all unwritten bytes to physical media.

  Input:    n/a

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "sync";
   int retc;

// Perform any required tracing
//
   FTRACE(sync, "");

// We can test the pendio flag w/o a lock because the person doing this
// sync must have done the previous write. Causality is the synchronizer.
//
   if (!(oh->flags & OFS_PENDIO)) return SFS_OK;
   TimeStamp();

// We can also skip the sync if the file is closed. However, we need a file
// object lock in order to test the flag. We can also reset the PENDIO flag.
//
   LOCK(oh);
   if(!(retc = (oh->flags & OFS_TCLOSE))) oh->activ++;
   oh->flags &= ~OFS_PENDIO;
   oh->optod = tod.tv_sec;
   UNLOCK(oh);
   if (retc) return SFS_OK;

// Perform the function
//
   if ((retc = oh->Select().Fsync()))
      {LOCK(oh);  oh->flags |= OFS_PENDIO; oh->activ--; UNLOCK(oh);
       return XrdOfsFS.Emsg(epname, error, retc, "synchronize", oh->Name());
      }

// Unlock the file handle and indicate all went well
//
   RELEASE(oh);
   return SFS_OK;
}


/******************************************************************************/
/*                              s y n c   A I O                               */
/******************************************************************************/
  
// For now, reverts to synchronous case
//
int XrdOfsFile::sync(XrdSfsAio *aiop)
{
   aiop->Result = this->sync();
   aiop->doneWrite();
   return 0;
}
/******************************************************************************/
/*                              t r u n c a t e                               */
/******************************************************************************/

int XrdOfsFile::truncate(XrdSfsFileOffset  flen)  // In
/*
  Function: Set the length of the file object to 'flen' bytes.

  Input:    flen      - The new size of the file.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.

  Notes:    If 'flen' is smaller than the current size of the file, the file
            is made smaller and the data past 'flen' is discarded. If 'flen'
            is larger than the current size of the file, a hole is created
            (i.e., the file is logically extended by filling the extra bytes 
            with zeroes).
*/
{
   static const char *epname = "trunc";
   int retc;

// Lock the file handle and perform any tracing
//
   FTRACE(truncate, "len=" <<flen);

// Make sure the offset is not too large
//
#if _FILE_OFFSET_BITS!=64
   if (flen >  0x000000007fffffff)
      return  XrdOfsFS.Emsg(epname, error, EFBIG, "read", oh->Name());
#endif

// Check if we should reopen this handle
//
   LOCK(oh);
   REOPENandHOLD(oh);
   oh->flags |= OFS_PENDIO;
   UNLOCK(oh);

// Perform the function
//
   retc = oh->Select().Ftruncate(flen);
   RELEASE(oh);
   if (retc)
      return XrdOfsFS.Emsg(epname, error, retc, "truncate", oh->Name());

// Unlock the file and indicate success
//
   return SFS_OK;
}

/******************************************************************************/
/*                             g e t C X i n f o                              */
/******************************************************************************/
  
int XrdOfsFile::getCXinfo(char cxtype[4], int &cxrsz)
/*
  Function: Set the length of the file object to 'flen' bytes.

  Input:    n/a

  Output:   cxtype - Compression algorithm code
            cxrsz  - Compression region size

            Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   const char *epname = "getCXinfo";

// Lock the handle to make sure we have an open file
//
   LOCK(oh);

// Copy out the info
//
   cxtype[0] = oh->cxid[0]; cxtype[1] = oh->cxid[1];
   cxtype[2] = oh->cxid[2]; cxtype[3] = oh->cxid[3];
   cxrsz = oh->cxrsz;

// All done
//
   UNLOCK(oh);
   return SFS_OK;
}

/******************************************************************************/
/*            P r i v a t e   X r d O f s F i l e   M e t h o d s             */
/******************************************************************************/
/******************************************************************************/
/*                             s e t C X I n f o                              */
/******************************************************************************/
  
void XrdOfsFile::setCXinfo(XrdSfsFileOpenMode mode)
{
    EPNAME("setCXinfo")
    if (mode & SFS_O_RAWIO)
       {char cxtype[5], buffer[OUC_MAX_ERROR_LEN];
        dorawio = 1;
        strncpy(cxtype, (const char *)oh->cxid, sizeof(cxtype)-1);
        cxtype[4] = '\0';
        sprintf(buffer,"!attn C=%s R=%d", cxtype, oh->cxrsz);
        error.setErrInfo(0, buffer);
        FTRACE(open, "raw i/o on; resp=" <<buffer);
       } else FTRACE(open, "raw i/o off");
}

/******************************************************************************/
/*                               U n c l o s e                                */
/******************************************************************************/

int XrdOfsFile::Unclose()
{   int retc;
    static const char *epname = "unclose";
    XrdOucEnv dummyenv;

// Reopen the file object as needed
//
   if ((retc = oh->Select().Open(oh->Name(),oh->oflag,(mode_t)0,dummyenv))<0)
      {XrdOfsFS.Emsg(epname,*new XrdOucErrInfo,retc,"open",oh->Name());
       return 0;
      }

// Insert file handle into open chain
//
   oh->Activate();

// Trace the unclose
//
   FTRACE(open, "unclose n=" <<XrdOfsFS.FDOpen);
   return 1;
}

/******************************************************************************/
/*                                                                            */
/*         F i l e   S y s t e m   O b j e c t   I n t e r f a c e s          */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/*                                 c h m o d                                  */
/******************************************************************************/

int XrdOfs::chmod(const char             *path,    // In
                        XrdSfsMode        Mode,    // In
                        XrdOucErrInfo    &einfo,   // Out
                  const XrdSecClientName *client)  // In
/*
  Function: Change the mode on a file or directory.

  Input:    path      - Is the fully qualified name of the file to be removed.
            einfo     - Error information object to hold error details.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "chmod";
   mode_t acc_mode = Mode & S_IAMB;
   char *tident = client->tident;
   einfo.setErrUser(tident);
   int retc;
   XTRACE(chmod, path, "");

// Apply security, as needed
//
   if (XrdOfsFS.VPlist.NotEmpty() && !XrdOfsFS.VPlist.Find(path))
      return XrdOfsFS.Emsg(epname, einfo, EACCES, "change", path);
   AUTHORIZE(client,AOP_Chmod,"chmod",path,einfo,SFS_ERROR);

// Find out where we should chmod this file
//
   if (Finder && Finder->isRemote())
      if (Options & XrdOfsFWDCHMOD)
         {char buff[8];
          sprintf(buff, "%o", acc_mode);
          if ((retc = Finder->Forward(einfo,"chmod",buff,(char *)path)))
             return fsError(einfo, retc);
         }
      else if ((retc = Finder->Locate(einfo,path,O_RDWR)))
              return fsError(einfo, retc);

// Now try to find the file or directory
//
   if (!(retc = XrdOssSS.Chmod(path, acc_mode))) return SFS_OK;

// An error occured, return the error info
//
   return XrdOfsFS.Emsg(epname, einfo, retc, "change", path);
}

/******************************************************************************/
/*                                e x i s t s                                 */
/******************************************************************************/

int XrdOfs::exists(const char                *path,        // In
                         XrdSfsFileExistence &file_exists, // Out
                         XrdOucErrInfo       &einfo,       // Out
                   const XrdSecClientName    *client)      // In
/*
  Function: Determine if file 'path' actually exists.

  Input:    path        - Is the fully qualified name of the file to be tested.
            file_exists - Is the address of the variable to hold the status of
                          'path' when success is returned. The values may be:
                          XrdSfsFileExistsIsDirectory - file not found but path is valid.
                          XrdSfsFileExistsIsFile      - file found.
                          XrdSfsFileExistsIsNo        - neither file nor directory.
            einfo       - Error information object holding the details.
            client      - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.

  Notes:    When failure occurs, 'file_exists' is not modified.
*/
{
   static const char *epname = "exists";
   struct stat fstat;
   int retc;
   char *tident = client->tident;
   einfo.setErrUser(tident);
   XTRACE(exists, path, "");

// Apply security, as needed
//
   if (XrdOfsFS.VPlist.NotEmpty() && !XrdOfsFS.VPlist.Find(path))
      return XrdOfsFS.Emsg(epname, einfo, EACCES, "locate", path);
   AUTHORIZE(client,AOP_Stat,"locating",path,einfo,SFS_ERROR);

// Find out where we should stat this file
//
   if (Finder && Finder->isRemote() 
   &&  (retc = Finder->Locate(einfo, path, O_RDONLY)))
      return fsError(einfo, retc);

// Now try to find the file or directory
//
   retc = XrdOssSS.Stat(path, &fstat);
   if (!retc)
      {     if (S_ISDIR(fstat.st_mode)) file_exists=XrdSfsFileExistIsDirectory;
       else if (S_ISREG(fstat.st_mode)) file_exists=XrdSfsFileExistIsFile;
       else                             file_exists=XrdSfsFileExistNo;
       return SFS_OK;
      }
   if (retc == -ENOENT)
      {file_exists=XrdSfsFileExistNo;
       return SFS_OK;
      }

// An error occured, return the error info
//
   return XrdOfsFS.Emsg(epname, einfo, retc, "locate", path);
}

/******************************************************************************/
/*                            g e t V e r s i o n                             */
/******************************************************************************/
  
const char *XrdOfs::getVersion() {return XrdVSTRING;}

/******************************************************************************/
/*                                 m k d i r                                  */
/******************************************************************************/

int XrdOfs::mkdir(const char             *path,    // In
                        XrdSfsMode        Mode,    // In
                        XrdOucErrInfo    &einfo,   // Out
                  const XrdSecClientName *client)  // In
/*
  Function: Create a directory entry.

  Input:    path      - Is the fully qualified name of the file to be removed.
            Mode      - Is the POSIX mode value the directory is to have.
                        Additionally, Mode may contain SFS_O_MKPTH if the
                        full dircectory path should be created.
            einfo     - Error information object to hold error details.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "mkdir";
   mode_t acc_mode = Mode & S_IAMB;
   int retc, mkpath = Mode & SFS_O_MKPTH;
   char *tident = client->tident;
   einfo.setErrUser(tident);
   XTRACE(mkdir, path, "");

// Apply security, as needed
//
   if (XrdOfsFS.VPlist.NotEmpty() && !XrdOfsFS.VPlist.Find(path))
      return XrdOfsFS.Emsg(epname, einfo, EACCES, "create", path);
   AUTHORIZE(client,AOP_Mkdir,"mkdir",path,einfo,SFS_ERROR);

// Find out where we should remove this file
//
   if (Finder && Finder->isRemote())
      if (Options & XrdOfsFWDMKDIR)
         {char buff[8];
          sprintf(buff, "%o", acc_mode);
          return ((retc = Finder->Forward(einfo, (mkpath ? "mkpath" : "mkdir"),
                                  buff,(char *)path)) ? 
                 fsError(einfo, retc) : SFS_OK);
         }
         else if ((retc = Finder->Locate(einfo,path,O_WRONLY)))
                 return fsError(einfo, retc);

// Perform the actual operation
//
    if ((retc = XrdOssSS.Mkdir(path, acc_mode, mkpath)))
       return XrdOfsFS.Emsg(epname, einfo, retc, "mkdir", path);
    return SFS_OK;
}

/******************************************************************************/
/*                               p r e p a r e                                */
/******************************************************************************/

int XrdOfs::prepare(      XrdSfsPrep       &pargs,      // In
                          XrdOucErrInfo    &out_error,  // Out
                    const XrdSecClientName *client)     // In
{
   XrdOucTList *tp = pargs.paths;
   int retc;

// Run through the paths to make sure client can read each one
//
   while(tp)
        {AUTHORIZE(client, AOP_Read, "preparing", tp->text, error, SFS_ERROR);
         tp = tp->next;
        }

// If we have a finder object, use it to prepare the paths. Otherwise,
// ignore this prepare request (we may change this in the future).
//
   if (XrdOfsFS.Finder && (retc = XrdOfsFS.Finder->Prepare(out_error, pargs)))
      return fsError(out_error, retc);
   return 0;
}
  
/******************************************************************************/
/*                                r e m o v e                                 */
/******************************************************************************/

int XrdOfs::remove(const char              type,    // In
                   const char             *path,    // In
                         XrdOucErrInfo    &einfo,   // Out
                   const XrdSecClientName *client)  // In
/*
  Function: Delete a file from the namespace and release it's data storage.

  Input:    type      - 'f' for file and 'd' for directory.
            path      - Is the fully qualified name of the file to be removed.
            einfo     - Error information object to hold error details.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   int retc;
   static const char *epname = "remove";
   char *tident = client->tident;
   einfo.setErrUser(tident);
   XTRACE(remove, path, "");

// Apply security, as needed
//
   if (XrdOfsFS.VPlist.NotEmpty() && !XrdOfsFS.VPlist.Find(path))
      return XrdOfsFS.Emsg(epname, einfo, EACCES, "remove", path);
   AUTHORIZE(client,AOP_Delete,"removing",path,einfo,SFS_ERROR);

// Find out where we should remove this file
//
   if (Finder && Finder->isRemote())
      if (Options & (type == 'd' ? XrdOfsFWDRMDIR : XrdOfsFWDRM))
         return ((retc = Finder->Forward(einfo, (type == 'd' ? "rmdir":"rm"),
                         (char *)path)) ? fsError(einfo, retc) : SFS_OK);
         else if ((retc = Finder->Locate(einfo,path,O_WRONLY)))
                 return fsError(einfo, retc);

// Perform the actual deletion
//
    if ((retc = XrdOssSS.Unlink(path)))
       return XrdOfsFS.Emsg(epname, einfo, retc, "remove", path);
    if (type == 'f')
       {XrdOfsFS.Detach_Name(path);
        if (Balancer) Balancer->Removed(path);
       }
    return SFS_OK;
}

/******************************************************************************/
/*                                r e n a m e                                 */
/******************************************************************************/

int XrdOfs::rename(const char             *old_name,  // In
                   const char             *new_name,  // In
                         XrdOucErrInfo    &einfo,     //Out
                   const XrdSecClientName *client)    // In
/*
  Function: Renames a file with name 'old_name' to 'new_name'.

  Input:    old_name  - Is the fully qualified name of the file to be renamed.
            new_name  - Is the fully qualified name that the file is to have.
            einfo     - Error information structure, if an error occurs.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "rename";
   int retc;
   char *tident = client->tident;
   einfo.setErrUser(tident);
   XTRACE(rename, new_name, "old fn=" <<old_name <<" new ");

// Apply security, as needed
//
   if (XrdOfsFS.VPlist.NotEmpty())
      {if (XrdOfsFS.VPlist.Find(old_name))
          return XrdOfsFS.Emsg(epname, einfo, EACCES, "rename", old_name);
       if (XrdOfsFS.VPlist.Find(new_name))
          return XrdOfsFS.Emsg(epname, einfo, EACCES, "rename to", new_name);
      }
   AUTHORIZE2(client, einfo, SFS_ERROR,
              AOP_Rename, "renaming",    old_name,
              AOP_Insert, "renaming to", new_name);

// Find out where we should rename this file
//
   if (Finder && Finder->isRemote())
      if (Options & XrdOfsFWDMV)
         return ((retc = Finder->Forward(einfo,"mv",(char *)old_name,
                                                    (char *)new_name))
                ? fsError(einfo, retc) : SFS_OK);
         else if ((retc = Finder->Locate(einfo,old_name,O_RDWR)))
                 return fsError(einfo, retc);

// Perform actual rename operation
//
   if ((retc = XrdOssSS.Rename(old_name, new_name)))
      return XrdOfsFS.Emsg(epname, einfo, retc, "rename", old_name);
   XrdOfsFS.Detach_Name(old_name);
   if (Balancer) {Balancer->Removed(old_name);
                  Balancer->Added(new_name);
                 }
   return SFS_OK;
}

/******************************************************************************/
/*                                  s t a t                                   */
/******************************************************************************/

int XrdOfs::stat(const char             *path,        // In
                       struct stat      *buf,         // Out
                       XrdOucErrInfo    &einfo,       // Out
                 const XrdSecClientName *client)      // In
/*
  Function: Return file status information

  Input:    path      - The path for which status is wanted
            buf       - The stat structure to hold the results
            einfo     - Error information structure, if an error occurs.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "stat";
   int retc;
   char *tident = client->tident;
   einfo.setErrUser(tident);
   XTRACE(stat, path, "");

// Apply security, as needed
//
   if (XrdOfsFS.VPlist.NotEmpty() && !XrdOfsFS.VPlist.Find(path))
      return XrdOfsFS.Emsg(epname, einfo, EACCES, "locate", path);
   AUTHORIZE(client,AOP_Stat,"locating",path,einfo,SFS_ERROR);

// Find out where we should stat this file
//
   if (Finder && Finder->isRemote()
   &&  (retc = Finder->Locate(einfo, path, O_RDONLY)))
      return fsError(einfo, retc);

// Now try to find the file or directory
//
   if ((retc = XrdOssSS.Stat(path, buf)))
      return XrdOfsFS.Emsg(epname, einfo, retc, "locate", path);
   return SFS_OK;
}

/******************************************************************************/

int XrdOfs::stat(const char             *path,        // In
                       mode_t           &mode,        // Out
                       XrdOucErrInfo    &einfo,       // Out
                 const XrdSecClientName *client)      // In
/*
  Function: Return file status information (resident files only)

  Input:    path      - The path for which status is wanted
            mode      - The stat mode entry (faked -- do not trust it)
            einfo     - Error information structure, if an error occurs.
            client    - Authentication credentials, if any.

  Output:   Always returns SFS_ERROR if a delay needs to be imposed. Otherwise,
            SFS_OK is returned and mode is appropriately, if inaccurately, set.
            If file residency cannot be determined, mode is set to -1.
*/
{
   static const char *epname = "stat";
   struct stat buf;
   int retc;
   char *tident = client->tident;
   einfo.setErrUser(tident);
   XTRACE(stat, path, "");

// Apply security, as needed
//
   if (XrdOfsFS.VPlist.NotEmpty() && !XrdOfsFS.VPlist.Find(path))
      return XrdOfsFS.Emsg(epname, einfo, EACCES, "locate", path);
   AUTHORIZE(client,AOP_Stat,"locating",path,einfo,SFS_ERROR);
   mode = (mode_t)-1;

// Find out where we should stat this file
//
   if (Finder && Finder->isRemote()
   &&  (retc = Finder->Locate(einfo, path, O_RDONLY | O_NDELAY)))
      return fsError(einfo, retc);

// Now try to find the file or directory
//
   if (!(retc = XrdOssSS.Stat(path, &buf, 1))) mode = buf.st_mode;
      else if (ENOMSG != retc) return XrdOfsFS.Emsg(epname, einfo, retc,
                                                    "locate", path);
   return SFS_OK;
}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

// Warning: The caller must have the object but *not* the anchor locked. This
//          method returns with the object deleted (hence unlocked).
//
int XrdOfs::Close(XrdOfsHandle *oh)
{

// If this is a real close, then decrement link count. However, we need to
// obtain the anchor lock before touching the links field.
//
    oh->LockAnchor();
    oh->links--;

// Return if there are still active links to this object
//
    if (oh->links) {oh->UnLockAnchor(); oh->UnLock(); return 0;}

// Close the file appropriately if it's really open
//
    Unopen(oh);

// Remove the object from all chains and release the anchor and object locks
//
   oh->Retire(0);
   oh->UnLockAnchor();
   oh->UnLock();

// Update stats
//
   if (Reporter) Reporter->UpdateFD(-1);

// Free up the storage and return
//
   delete oh;
   return 0;  // We ignore errors here since they are immaterial
}

/******************************************************************************/
/*                                U n o p e n                                 */
/******************************************************************************/
  
// Warning: The caller must have both the object and the anchor locked.
//          This method returns with the object *still* locked.
//
int XrdOfs::Unopen(XrdOfsHandle *oh)
{
    static const char *epname = "Unopen";
#ifndef NODEBUG
    static const char *tident = "";
#endif
    int retc = 0;

// Close the file appropriately if it's really open
//
    if (!(oh->flags & OFS_TCLOSE))
       {if (oh->Select().Close()) retc = XrdOfsFS.Emsg(epname, 
                             *new XrdOucErrInfo, retc, "close", oh->Name());
           else retc = SFS_OK;

        // Unchain this filehandle from the active chain
        //
        oh->Deactivate(0);
        FTRACE(open, "numfd=" <<FDOpen);
       }

// Simply return, the caller must unlock the object
//
   return retc;
}

/******************************************************************************/
/*                                  E m s g                                   */
/******************************************************************************/

int XrdOfs::Emsg(const char    *pfx,    // Message prefix value
                 XrdOucErrInfo &einfo,  // Place to put text & error code
                 int            ecode,  // The error code
                 const char    *op,     // Operation being performed
                 const char    *target) // The target (e.g., fname)
{
   char *etext, buffer[OUC_MAX_ERROR_LEN], unkbuff[64];

// Get the reason for the error
//
   if (!(etext = OfsEroute.ec2text(ecode))) 
      {sprintf(unkbuff, "reason unknown (%d)", ecode); etext = unkbuff;}

// Format the error message
//
    snprintf(buffer,sizeof(buffer),"Unable to %s %s; %s", op, target, etext);

// Print it out if debugging is enabled
//
#ifndef NODEBUG
    OfsEroute.Emsg(pfx, einfo.getErrUser(), buffer);
#endif

// Place the error message in the error object and return
//
    einfo.setErrInfo(ecode, buffer);

    return SFS_ERROR;
}

/******************************************************************************/
/*                     P R I V A T E    S E C T I O N                         */
/******************************************************************************/
/******************************************************************************/
/*                           D e t a c h _ N a m e                            */
/******************************************************************************/

void XrdOfs::Detach_Name(const char *fname)
{
     const unsigned long hval = XrdOucHashVal(fname);

// Hide all matches in r/o and r/w queues to prevent future attaches
//
   XrdOfsOrigin_RO.Hide(hval, fname);
   XrdOfsOrigin_RW.Hide(hval, fname);
}

/******************************************************************************/
/*                                 F n a m e                                  */
/******************************************************************************/

const char *XrdOfs::Fname(const char *path)
{
   int i = strlen(path)-1;
   while(i) if (path[i] == '/') return &path[i+1];
               else i--;
   return path;
}

/******************************************************************************/
/*                               f s E r r o r                                */
/******************************************************************************/
  
int XrdOfs::fsError(XrdOucErrInfo &myError, int rc)
{

// Translate the error code
//
   if (rc == -EREMOTE) return SFS_REDIRECT;
   if (rc > 0)         return rc;
                       return SFS_ERROR;
}

/******************************************************************************/
/*                                 S t a l l                                  */
/******************************************************************************/
  
int XrdOfs::Stall(XrdOucErrInfo   &einfo, // Error text & code
                  int              stime, // Seconds to stall
                  const char      *path)  // The path to stall on
{
    const char *msgfmt = "File %s is being staged; "
                         "estimated time to completion %s";
    EPNAME("Stall")
#ifndef NODEBUG
    const char *tident = (char *)"";
#endif
    char Mbuff[2048], Tbuff[32];

// Format the stall message
//
    snprintf(Mbuff, sizeof(Mbuff)-1, msgfmt,
             Fname(path), WaitTime(stime, Tbuff, sizeof(Tbuff)));
    ZTRACE(delay, "Stall " <<stime <<": " <<Mbuff <<" for " <<path);

// Place the error message in the error object and return
//
    einfo.setErrInfo(0, Mbuff);

// All done
//
   return (stime > MaxDelay ? MaxDelay : stime);
}
  
/******************************************************************************/
/*                              W a i t T i m e                               */
/******************************************************************************/

char *XrdOfs::WaitTime(int stime, char *buff, int blen)
{
   int mlen, hr, min, sec;

// Compute hours, minutes, and seconds
//
   min = stime / 60;
   sec = stime % 60;
   hr  = min   / 60;
   min = min   % 60;

// Now format the message based on time duration
//
        if (!hr && !min)
           mlen = snprintf(buff,blen,"%d second%s",sec,(sec > 1 ? "s" : ""));
   else if (!hr)
          {if (sec > 10) min++;
           mlen = snprintf(buff,blen,"%d minute%s",min,(min > 1 ? "s" : ""));
          }
   else   {if (hr == 1)
              if (min <= 30)
                      mlen = snprintf(buff,blen,"%d minutes",min+60);
                 else mlen = snprintf(buff,blen,"%d hour and %d minutes",hr,min);
              else {if (min > 30) hr++;
                      mlen = snprintf(buff,blen,"%d hours",hr);
                   }
          }

// Complete the message
//
   buff[blen-1] = '\0';
   return buff;
}
  
/******************************************************************************/
/*                      I d l e   F D   H a n d l i n g                       */
/******************************************************************************/
/******************************************************************************/
/*                        X r d O f s I d l e S c a n                         */
/******************************************************************************/
  
void *XrdOfsIdleScan(void *noargs)
{
   EPNAME("IdleScan")
#ifndef NODEBUG
   const char *tident = "";
#endif
   int num_closed;
   struct timeval tod;
   struct timespec naptime = {XrdOfsFS.FDMinIdle, 0};

// This thread never stops unless we are not supposed to do this
//
   if (XrdOfsFS.FDMinIdle) while(1) {

   // Wait until the right time
   //
      do {nanosleep(&naptime, 0);} while(XrdOfsFS.FDOpen <= XrdOfsFS.FDOpenMax);

   // Process each queue for idle handles. Do NOT process the directory queue!
   //
      num_closed = XrdOfsFS.FDOpen;
      XrdOfsIdleCheck(XrdOfsOrigin_RO);
      XrdOfsIdleCheck(XrdOfsOrigin_RW);
      num_closed = num_closed - XrdOfsFS.FDOpen;

   // Get absolute minimum wait time for the next scan
   //
      gettimeofday(&tod, 0);
      naptime.tv_sec =
              (XrdOfsOrigin_RO.IdleDeadline < XrdOfsOrigin_RW.IdleDeadline
              ? XrdOfsOrigin_RO.IdleDeadline : XrdOfsOrigin_RW.IdleDeadline)
              - tod.tv_sec;

   // Perform ending trace
   //
      ZTRACE(qscan, "closed " <<num_closed <<" active " <<XrdOfsFS.FDOpen
                    <<" rescan " <<naptime.tv_sec
                    <<" r/o=" <<(XrdOfsOrigin_RO.IdleDeadline-tod.tv_sec)
                    <<" r/w=" <<(XrdOfsOrigin_RW.IdleDeadline-tod.tv_sec));
      }

// Exit normally if we should ever get here (rather unlikely)
//
   return (void *)0;
}

/******************************************************************************/
/*                       X r d O f s I d l e C h e c k                        */
/******************************************************************************/

void XrdOfsIdleCheck(XrdOfsHandleAnchor &anchor)
{
   struct timeval tod;
   time_t NextTime;

   // Get current time of day
   //
      gettimeofday(&tod, 0);

   // Check if we should scan this queue for idle handles
   //
     if (tod.tv_sec >= anchor.IdleDeadline)
        {anchor.IdleDeadline = 0;
         anchor.Apply2Open(XrdOfsIdleXeq, (void *)tod.tv_sec);

         // Caclculate the next time we really need to do a queue scan
         //
         NextTime = XrdOfsFS.FDMaxIdle - anchor.IdleDeadline;
         if (NextTime > XrdOfsFS.FDMinIdle) anchor.IdleDeadline=NextTime+tod.tv_sec;
            else anchor.IdleDeadline = XrdOfsFS.FDMinIdle+tod.tv_sec;
        }
}
  
/******************************************************************************/
/*                         X r d O f s I d l e X e q                          */
/******************************************************************************/

int XrdOfsIdleXeq(XrdOfsHandle *op, void *tsecarg)
{
    EPNAME("IdleXeq")
#ifndef NODEBUG
    const char *tident = "";
#endif
    time_t tsec = (time_t)tsecarg;
    XrdOfsHandleAnchor *anchor = &(op->Anchor());
    int IdleTime;
    char *act = (char *)" ";

 // Ceck if this handle is past the idle deadline, if so, close it. Note
 // that we already have the anchor locked so we don't ask for it to be locked.
 // However, we need to lock the file handle which may cause a deadlock. So,
 // we skip processing this handle is we can't get the lock immediately.
 //
    IdleTime = tsec - op->optod;
    if (IdleTime <= XrdOfsFS.FDMaxIdle)
       anchor->IdleDeadline = Max(IdleTime, anchor->IdleDeadline);
       else {if (op->CondLock())
                {if (!op->activ) XrdOfsFS.Unopen(op);
                    else act = (char *)" active ";
                 op->UnLock();
                }
                else act = (char *)" unlockable ";
             XTRACE(qscan,op->Name(),"idle=" <<IdleTime <<act <<op->Qname());
            }
    return 0;
}
