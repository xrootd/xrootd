/******************************************************************************/
/*                                                                            */
/*                    X r d C S 2 D C M C o n f i g . c c                     */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdCS2DCMConfigCVSID = "$Id$";

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "Xrd/XrdScheduler.hh"
#include "Xrd/XrdTrace.hh"
#include "XrdNet/XrdNetOpts.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdNet/XrdNetLink.hh"
#include "XrdNet/XrdNetWork.hh"

#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucTimer.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdCS2/XrdCS2DCM.hh"
#include "XrdCS2/XrdCS2DCMFile.hh"

/******************************************************************************/
/*           G l o b a l   C o n f i g u r a t i o n   O b j e c t            */
/******************************************************************************/

extern XrdScheduler      XrdSched;

extern XrdOucError       XrdLog;

extern XrdOucLogger      XrdLogger;

extern XrdOucTrace       XrdTrace;

       XrdNetWork        XrdCS2Net(&XrdLog);

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

class XrdLogWorker : XrdJob
{
public:

     void DoIt() {XrdLog.Say(0, "XrdCS2d - Castor2 Disk Cache Manager");
                  midnite += 86400;
                  XrdSched.Schedule((XrdJob *)this, midnite);
                 }

          XrdLogWorker() : XrdJob("midnight runner")
                         {midnite = XrdOucTimer::Midnight() + 86400;
                          XrdSched.Schedule((XrdJob *)this, midnite);
                         }
         ~XrdLogWorker() {}
private:
time_t midnite;
};

class XrdCleanup : XrdJob
{
public:

     void DoIt() {extern XrdCS2DCM XrdCS2d;
                  XrdCS2d.Cleanup();
                  delete this;
                 }

          XrdCleanup() : XrdJob("cleanup") {}
         ~XrdCleanup() {}
};

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCS2DCM::XrdCS2DCM(void) : Request(&XrdLog)
{

// Preset all variables with common defaults
//
   APath    = 0;    // Path to active files
   APlen    = 0;
   CPath    = 0;    // Path to closed files
   CPlen    = 0;
   EPath    = 0;    // Path to our event FIFO
   EPlen    = 0;
   MPath    = strdup("/tmp/XrdCS2d/");
   MPlen    = strlen(MPath);
   PPath    = 0;    // Path to pending files
   PPlen    = 0;
   Parent   = getppid();
   UpTime   = time(0);
   QLim     = 100;
}
  
/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdCS2DCM::Configure(int argc, char **argv)
{
/*
  Function: Establish configuration at start up time.

  Input:    None.

  Output:   1 upon success or 0 otherwise.
*/
   struct stat buf;
   XrdNetSocket *EventSock;
   int n, EventFD, retc, NoGo = 0;
   const char *inP;
   char c, buff[2048], *olbP, *logfn = 0;
   extern char *optarg;
   extern int optind, opterr;

// Process the options
//
   opterr = 0;
   if (argc > 1 && '-' == *argv[1]) 
      while ((c = getopt(argc,argv,"dl:q:")) && ((unsigned char)c != 0xff))
     { switch(c)
       {
       case 'd': XrdTrace.What = TRACE_ALL;
                 XrdOucThread::setDebug(&XrdLog);
                 break;
       case 'l': if (logfn) free(logfn);
                 logfn = strdup(optarg);
                 break;
       case 'q': if (!(QLim = atoi(optarg)))
                    {XrdLog.Emsg("Config", "Invalid -q value -",optarg);NoGo=1;}
                 break;
       default:  if (index("lq", (int)(*(argv[optind-1]+1))))
                    XrdLog.Emsg("Config", argv[optind-1], "parameter not specified.");
                    else XrdLog.Emsg("Config", "Invalid option -", argv[optind-1]);
                 NoGo = 1;
       }
     }

// Get the directory where the meta information is to go
//
   if (optind >= argc)
      XrdLog.Emsg("Config", "Using default recording directory", MPath);
      else {strcpy(buff, argv[optind]);
            n = strlen(buff);
            if (buff[n-1] != '/') {buff[n] = '/'; buff[n+1] = '\0';}
            MPath = strdup(buff); MPlen = strlen(buff);
           }

// Verify that the meta directory exists
//
   if (stat(MPath,&buf))
      if (errno == ENOENT)
         {if ((retc = XrdOucUtils::makePath(MPath,0770)))
             {XrdLog.Emsg("Config", retc, "create recording directory", MPath);
              return 0;
             }
         } else {
          XrdLog.Emsg("Config", errno, "process recording directory", MPath);
          return 0;
         }

   if (!(buf.st_mode & S_IFDIR))
      {XrdLog.Emsg("Config","Recording path",MPath,"is not a directory");
        return 0;
      }
   if (access(MPath, W_OK))
      {XrdLog.Emsg("Config", errno, "access path", MPath);
       return 0;
      }

// Create the "active" directory for known active files
//
   strcpy(buff, MPath); 
   strcat(buff, "active/");
   if ((retc = XrdOucUtils::makePath(buff,0770)))
      {XrdLog.Emsg("Config", retc, "create active file cache", buff);
       return 0;
      }
   APath = strdup(buff);
   APlen = strlen(buff);

// Create the "closed" directory for known closed files
//
   strcpy(buff, MPath); 
   strcat(buff, "closed/");
   if ((retc = XrdOucUtils::makePath(buff,0770)))
      {XrdLog.Emsg("Config", retc, "create closed file cache", buff);
       return 0;
      }
   CPath = strdup(buff);
   CPlen = strlen(buff);

// Create the "pending" directory for known pending files
//
   strcpy(buff, MPath); 
   strcat(buff, "pending/");
   if ((retc = XrdOucUtils::makePath(buff,0770)))
      {XrdLog.Emsg("Config", retc, "create pending file cache", buff);
       return 0;
      }
   PPath = strdup(buff);
   PPlen = strlen(buff);

// Check if we should continue
//
   if (NoGo) return 0;

// Bind the log file if we have one
//
   if (logfn)
      {XrdLogger.Bind(logfn, 24*60*60);
       free(logfn);
      }

// Put out the herald
//
   XrdLog.Emsg("Config", "XrdCS2d initialization started.");

// Start the Scheduler
//
   XrdSched.Start();

// Create our notification path (r/w for us and our group)
//
   if (!(EventSock = XrdNetSocket::Create(&XrdLog, MPath, "CS2.events",
                                   0660, XRDNET_FIFO))) NoGo = 1;
      else {EventFD = EventSock->Detach();
            delete EventSock;
            Events.Attach(EventFD, 32*1024);
            sprintf(buff, "%sCS2.events", MPath);
            EPath = strdup(buff); EPlen = strlen(buff);
           }

// Create the notification path to the local olbd
//
   if ((olbP = getenv("XRDOLBPATH")) && *olbP)
      {sprintf(buff, "%solbd.notes", olbP);
       if (!(olbdLink = XrdCS2Net.Relay(buff, XRDNET_SENDONLY))) NoGo = 1;
      } else olbdLink = 0;

// Attach standard-in to our stream
//
   Request.Attach(STDIN_FILENO, 32*1024);

// Perform proper warm/cold startup
//
   if (!NoGo) NoGo = Setup();

// All done
//
   inP = (NoGo ? "failed." : "completed.");
   XrdLog.Emsg("Config", "XrdCS2d initialization ", inP);
   if (logfn) new XrdLogWorker();
   return !NoGo;
}

/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*                               C l e a n u p                                */
/******************************************************************************/

void XrdCS2DCM::Cleanup()
{
   const int oneHour = 60*60*1000;
   struct dirent *dir;
   DIR *DFD;
   char *cp, buff[16], thePath[2048];
   int fnum = 0;
   time_t Deadline;

// Open the directory that records active files
//
    if (!(DFD = opendir(APath)))
       {XrdLog.Emsg("Config", errno, "open active files directory", APath);
        return;
       }

// For each file in this directory we must issue a getdone or putfail
//
    errno = 0;
    while((dir = readdir(DFD)))
         {if (dir->d_name[0] != '%') continue;
          cp = dir->d_name;
          while(*cp) {if (*cp == '%') *cp = '/'; cp++;}
          fnum += Release("CS2d", dir->d_name, 1);
          errno = 0;
         }

    if (errno)
        XrdLog.Emsg("Config",errno,"cleaning up active file directory", APath);
        else {sprintf(buff, "%d", fnum);
              XrdLog.Emsg("config", buff, "file(s) cleaned up in", APath);
             }

    closedir(DFD);

// Now, we will scan through the list of closed files every hour to see if we
// can delete the symlink that points to the file. We also scan through the
// pending directory and delete any stale files.
//
   do {XrdOucTimer::Wait(oneHour);
       if (!(DFD = opendir(CPath)))
          {XrdLog.Emsg("Config", errno, "open closed files directory", APath);
           continue;
          }
       errno = 0;
       strcpy(thePath, CPath);
       while((dir = readdir(DFD)))
            {if (dir->d_name[0] != '%') continue;
             strcpy(thePath+CPlen, dir->d_name);
             Cleanup(thePath);
             errno = 0;
            }
       if (errno)
           XrdLog.Emsg("Config",errno,"cleaning up closed file directory", CPath);
       closedir(DFD);

       if (!(DFD = opendir(PPath)))
          {XrdLog.Emsg("Config", errno, "open pending files directory", PPath);
           continue;
          }
       errno = 0;
       strcpy(thePath, PPath);
       Deadline = time(0) - (60*60);
       while((dir = readdir(DFD)))
            {if (*(dir->d_name) != '.')
                {strcpy(thePath+PPlen, dir->d_name);
                 rmStale(thePath, Deadline);
                 errno = 0;
                }
            }
       if (errno)
           XrdLog.Emsg("Config",errno,"cleaning up pending file directory", PPath);
       closedir(DFD);
      } while(1);
}
  
/******************************************************************************/

void XrdCS2DCM::Cleanup(const char *thePath)
{
   const char *TraceID = "Cleanup";
   XrdCS2DCMFile theFile;
   struct stat buf;
   struct iovec iov[3];

// Read the contents of the file and notify the olbd
//
   if (!theFile.Init(thePath))
      {if (!stat(theFile.Pfn(), &buf)) return;
       TRACE(DEBUG, "Removed symlink " <<theFile.Pfn());
       unlink(theFile.Pfn());
       if (olbdLink)
          {iov[0].iov_base = (char *)"gone "; iov[0].iov_len = 5;
           iov[1].iov_base = theFile.Pfn();   iov[1].iov_len = strlen(theFile.Pfn());
           iov[2].iov_base = (char *)"\n";    iov[0].iov_len = 1;
           olbdLink->Send(iov, 3);
           unlink(thePath);
          }
      }
}

/******************************************************************************/
/*                               r m S t a l e                                */
/******************************************************************************/
  
void XrdCS2DCM::rmStale(const char *thePath, time_t Deadline)
{
   const char *TraceID = "rmStale";
   struct stat buf;

   if (!stat(thePath, &buf) && buf.st_atime > Deadline) return;

   TRACE(DEBUG, "Removed file " <<thePath);
   unlink(thePath);
}

/******************************************************************************/
/*                                 S e t u p                                  */
/******************************************************************************/
  
int XrdCS2DCM::Setup()
{
   char thePath[1024];
   int fnfd, plen;
   pid_t prevParent;

// Initialize Castor
//
   if (!CS2_Init()) return 1;

// At this point a warm start would be signified by our parent pid being the
// same as when we last started. Otherwise, this is a cold start and we need
// to go through all files that we had opened and either tell Castor to
// release them or to prepare them for migration.
//
   strcpy(thePath, MPath); strcat(thePath, "CS2d.ppid");
   do {fnfd = open(thePath, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);}
      while(fnfd < 0 && errno == EINTR);
   if (fnfd < 0)
      {XrdLog.Emsg("Config", errno, "open file", thePath);
       return 0;
      }

   if ((plen = read(fnfd, &prevParent, sizeof(prevParent))) < 0)
      {XrdLog.Emsg("Config", errno, "read file", thePath); plen = 0;}

   if (plen && prevParent != Parent)
      {XrdJob *jp = (XrdJob *)new XrdCleanup();
       XrdSched.Schedule(jp);
       plen = 0;
      }

   if (!plen && write(fnfd, &Parent, sizeof(Parent)) < 0)
       XrdLog.Emsg("Config", errno, "write file", thePath);

   close(fnfd);
   return 0;
}
