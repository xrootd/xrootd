/******************************************************************************/
/*                                                                            */
/*                       X r d C S 2 D C M 2 c s . c c                        */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdCS2DCM2csCVSID = "$Id$";

#include <unistd.h>
#include <fcntl.h>
#include <iostream.h>
#include <string.h>
#include <stdio.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "Xrd/XrdScheduler.hh"
#include "Xrd/XrdTrace.hh"

#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdCS2/XrdCS2DCM.hh"

/******************************************************************************/
/*           G l o b a l   C o n f i g u r a t i o n   O b j e c t            */
/******************************************************************************/

extern XrdScheduler      XrdSched;

extern XrdOucError       XrdLog;

extern XrdOucLogger      XrdLogger;

extern XrdOucTrace       XrdTrace;
 
extern XrdCS2DCM         XrdCS2d;

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

class XrdCS2Job : XrdJob
{
public:

     void DoIt() {if (Pfn) XrdCS2d.Stage(Tid, FileID, Mode, Lfn, Pfn);
                     else  XrdCS2d.Event(Tid, FileID, Mode, Lfn);
                  delete this;
                 }

          XrdCS2Job(char *tid, char *fileid, char *mode,
                    char *lfn, char *pfn)
                   {Tid    = tid;
                    FileID = fileid;
                    strcpy(Mode,  mode);
                    Lfn = lfn; Pfn = pfn;
                   }
         ~XrdCS2Job() {free(Tid); free(FileID); free(Lfn);
                       if (Pfn) free(Pfn);
                      }
private:
char *Tid;
char *FileID;
char  Mode[8];
char *Lfn;
char *Pfn;
};

/******************************************************************************/
/*                              d o E v e n t s                               */
/******************************************************************************/
  
void XrdCS2DCM::doEvents()
{
   const char *Miss = 0, *TraceID = "doEvents";
   char *Eid, *tp, *Tid, *Lfn;

// Each request comes in as
// <traceid> {closer | closew | fwrite} <lfn>
//
   while((tp = Events.GetLine()))
        {TRACE(DEBUG, "Event: '" <<tp <<"'");
         Tid = Eid = 0;
               if (!(tp = Events.GetToken())) Miss = "traceid";
         else {Tid = strdup(tp);
               if (!(tp = Events.GetToken())) Miss = "eventid";
         else {Eid = strdup(tp);
               if (!(tp = Events.GetToken())) Miss = "lfn";
         else {Lfn = strdup(tp);
               Miss = 0;
              } } }

         if (Miss) {XrdLog.Emsg("doEvents", "Missing", Miss, "in event.");
                    if (Tid) free(Tid);
                    continue;
                   }

         XrdSched.Schedule((XrdJob *)new XrdCS2Job(Tid,Eid,(char *)"",Lfn,0));
        }

// If we exit then we lost the connection
//
   XrdLog.Emsg("doEvents", "Exiting; lost event connection to xrootd!");
   exit(8);
}

/******************************************************************************/
/*                            d o R e q u e s t s                             */
/******************************************************************************/
  
void XrdCS2DCM::doRequests()
{
   const char *Miss = 0, *TraceID = "doRequests";
   char *Fid, Fsize[24], Mode[8], *tp, *Tid, *Lfn, *Pfn;

   memset(Fsize, 0, sizeof(Fsize));
   Mode[sizeof(Mode)-1] = '\0';

// Each request comes in as
// <traceid> <fileid> {r|w[c][t]} <lfn> <pfn>
//
   while((tp = Request.GetLine()))
        {TRACE(DEBUG, "Request: '" <<tp <<"'");
         Tid = Fid = Lfn = 0;
               if (!(tp = Request.GetToken())) Miss = "traceid";
         else {Tid = strdup(tp);
               if (!(tp = Request.GetToken())) Miss = "file id";
         else {Fid = strdup(tp);
               if (!(tp = Request.GetToken())) Miss = "file size";
         else {strncpy(Fsize, tp, sizeof(Fsize)-1);
               if (!(tp = Request.GetToken())) Miss = "mode";
         else {strncpy(Mode, tp, sizeof(Mode)-1);
               if (!(tp = Request.GetToken())) Miss = "lfn";
         else {Lfn = strdup(tp);
               if (!(tp = Request.GetToken())) Miss = "pfn";
         else {Pfn = strdup(tp);
               Miss = 0;
              } } } } } }

         if (Miss) {XrdLog.Emsg("doReq", "Missing", Miss, "in request.");
                    if (Tid) free(Tid);
                    if (Fid) free(Fid);
                    if (Lfn) free(Lfn);
                    continue;
                   }

         XrdSched.Schedule((XrdJob *)new XrdCS2Job(Tid,Fid,Mode,Lfn,Pfn));
        }

// If we exit then we lost the connection
//
   XrdLog.Emsg("doRequests", "Exiting; lost request connection to xrootd!");
   exit(8);
}

/******************************************************************************/
/*                                 E v e n t                                  */
/******************************************************************************/
  
void XrdCS2DCM::Event(const char *Tid, char *Eid, char *Mode, char *Lfn)
{
   char thePath[2048];

// Construct the filename of where the RequestID is recorded
//
   makeFname(thePath, Lfn);

// Process the event
//
        if (!strcmp("closer", Eid)
        ||  !strcmp("closew", Eid)) Release(Tid, thePath, Lfn);
   else if (!strcmp("fwrite", Eid))
           {struct utimbuf times;
            times.actime = times.modtime = 0;
            if (utime(thePath, (const struct utimbuf *)&times))
               XrdLog.Emsg("Event", errno, "update time for file", thePath);
           }
   else XrdLog.Emsg("Event", "Received unknown event -", Tid, Eid);
}
  
/******************************************************************************/
/*                                 S t a g e                                  */
/******************************************************************************/
  
void XrdCS2DCM::Stage(const char *Tid, char *Fid, char *Mode,
                            char *Lfn, char *Pfn)
{
   const char *TraceID = "Stage";
   char thePath[2048], xeqPath[1024], *op;
   char *ofsEvent = getenv("XRDOFSEVENTS");

                                  //12345678901234
   struct iovec iox[12]= {{(char *)"#!/bin/sh\n",          10}, // 0
                          {(char *)"echo $2 >>",           10}, // 1
                          {thePath,   makeFname(thePath, Lfn)}, // 2
                          {(char *)"\n",                    1}, // 3
                          {(char *)"/bin/ln -s $1 ",       14}, // 4
                          {Pfn,                   strlen(Pfn)}, // 5
                          {(char *)"\n",                    1}, // 6
                          {(char *)"echo stage OK ",       14}, // 7
                          {Lfn,                   strlen(Lfn)}, // 9
                          {(char *)" >> ",                  4}, // 9
                          {ofsEvent,         strlen(ofsEvent)}, // 10
                          {(char *)"\n",                    1}};// 11

   struct iovec iov[3] = {{Mode,                            1}, // 0
                          {Pfn,                   strlen(Pfn)}, // 1
                          {(char *)"\n",                    1}};// 2
   int Oflags, fnfd, rc;

// Convert mode to open type flags
//
   op = Mode; Oflags = 0;
   while(*op)
        {switch(*op++)
               {case 'r': Oflags  = O_RDONLY; break;
                case 'w': Oflags |= O_RDWR;   break;
                case 'c': Oflags |= O_CREAT;  break;
                case 't': Oflags |= O_TRUNC;  break;
                case 'x': Oflags |= O_EXCL;   break;
                default:  XrdLog.Emsg("Stage", "Invalid mode:", Mode, Lfn);
                          failRequest(Pfn);
                          return;
               }
       }

// Make the directory structure for the upcomming symlink
//
   if ((rc = XrdOucUtils::makePath(Pfn,0770)))
      {XrdLog.Emsg("Stage", rc, "create directory path for", Pfn);
       return;
      }

// Create a file that will hold this information
//
   LockDir();
   do {fnfd = open(thePath, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);}
      while( fnfd < 0 && errno == EINTR);
   if (fnfd < 0)
      {XrdLog.Emsg("Stage", errno, "create file", thePath);
       failRequest(Pfn);
       UnLockDir();
       return;
      }

// Write the information into the file
//
   if (writev(fnfd, iov, 3) < 0)
      {XrdLog.Emsg("Stage", errno, "write file", thePath);
       failRequest(Pfn);
       UnLockDir();
       return;
      }

// All done here
//
   close(fnfd);

// Construct name of the script that will create the symlink and append the
// the subreqid to the database file we just created.
//
   strcpy(xeqPath,        MPath);
   strcpy(xeqPath+MPlen,  "files/");
   strcpy(xeqPath+MPlen+6,Fid);

// Construct the script that will create the symlink to the physical file
//
   do {fnfd = open(xeqPath, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IXUSR);}
      while( fnfd < 0 && errno == EINTR);
   if (fnfd < 0)
      {XrdLog.Emsg("Stage", errno, "create script", xeqPath);
       failRequest(Pfn);
       UnLockDir();
       return;
      }

// Write the information into the file
//
   if (writev(fnfd, iox, 12) < 0)
      {XrdLog.Emsg("Stage", errno, "write script", xeqPath);
       failRequest(Pfn);
       UnLockDir();
       return;
      }

// All done here
//
   close(fnfd);
   UnLockDir();

// Now we can schedule the I/O
//
   TRACE(DEBUG, Tid <<" open mode " << Mode <<" file " <<Fid <<' ' <<Lfn);
   if (!CS2_Open(Tid, Fid, Lfn, Oflags, 0))
      {failRequest(Pfn);
       return;
      }
}

/******************************************************************************/
/*                           f a i l R e q u e s t                            */
/******************************************************************************/
  
void XrdCS2DCM::failRequest(char *Pfn)
{
   char buff[2048];
   int rc, fd, PfnLen = strlen(Pfn);

// Construct a fail file name
//
   strcpy(buff, Pfn);
   strcpy(buff+PfnLen, ".fail");

// Add a fail file to keep staging this file at bay
//
   if ((rc = XrdOucUtils::makePath(Pfn,0770)))
      XrdLog.Emsg("failRequest", rc, "create directory path for", Pfn);
      else {do {fd = open(buff, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);}
               while(fd < 0 && errno == EINTR);
            if (fd > 0) close(fd);
           }
}

/******************************************************************************/
/*                             m a k e F n a m e                              */
/******************************************************************************/
  
int XrdCS2DCM::makeFname(char *thePath, const char *fn)
{
   static const char subdir[] = "0123456789abcdef";
   static const int  subnum   = 16;
   char *tp; int khash = 0;

// Construct the filename of where we will record the RequestID and pfn
//
   strcpy(thePath, MPath); 
   tp = thePath+MPlen+2;
   while(*fn) {*tp = (*fn == '/' ? '%' : *fn); khash ^= *fn; tp++; fn++;}
   *tp = '\0';

// Insert the directory we really want
//
   khash %= subnum;
   thePath[MPlen]   = subdir[khash];
   thePath[MPlen+1] = '/';
   return strlen(thePath);
}

/******************************************************************************/
/*                               R e l e a s e                                */
/******************************************************************************/
  
void XrdCS2DCM::Release(const char *Tid, char *thePath, char *Lfn)
{
   const char *TraceID = "Release";
   struct stat buf;
   char *np, *lp = 0, *Pfn=0, fileData[2048];
   int fd, isMod, len;
   unsigned long long reqID;

// Read the contents of the file
//
   do {fd = open(thePath, O_RDONLY);} while(fd < 0 && errno == EINTR);
   if (fd < 0)
      {if (errno == ENOENT)
          {TRACE(DEBUG, "Release file gone Tid=" <<Tid <<" path=" <<thePath);}
          else XrdLog.Emsg("Release", errno, "open file", thePath);
       return;
      }
   if (fstat(fd, &buf))
       {XrdLog.Emsg("Release", errno, "stat", thePath);
        close(fd); unlink(thePath);
        return;
       }
   if ((len = read(fd, fileData, sizeof(fileData)-1)) < 0)
      {XrdLog.Emsg("Release", errno, "read file", thePath);
       unlink(thePath);
       return;
      }

// Prepare to process the subreqid's
//
   close(fd); 
   Pfn = fileData+1;
   if (len < 2 || !(lp = index(fileData, '\n')))
      {XrdLog.Emsg("Release", "Invalid file data in", thePath);
       return;
      }
   *lp = '\0'; lp++;
   isMod = (buf.st_mtime == 0); // Indicates file modified
   fileData[len] = '\0';

// Issue putDone() or getDone() for each subreqid
//
   while(*lp)
        {reqID = strtoull(lp, &np, 10);
         if (*np != '\n')
            {XrdLog.Emsg("Release", "Invalid reqID", lp, thePath);
             break;
            }
         if (isMod) CS2_wDone(Tid, reqID, Lfn);
            else    CS2_rDone(Tid, reqID, Pfn);
         lp = np+1;
        }

// Delete the file
//
   unlink(thePath);
}
