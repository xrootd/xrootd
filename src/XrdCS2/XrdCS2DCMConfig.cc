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

#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucTimer.hh"
#include "XrdOuc/XrdOucUtils.hh"

#include "XrdCS2/XrdCS2DCM.hh"

/******************************************************************************/
/*           G l o b a l   C o n f i g u r a t i o n   O b j e c t            */
/******************************************************************************/

extern XrdScheduler      XrdSched;

extern XrdOucError       XrdLog;

extern XrdOucLogger      XrdLogger;

extern XrdOucTrace       XrdTrace;

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

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCS2DCM::XrdCS2DCM(void) : Request(&XrdLog)
{

// Preset all variables with common defaults
//
   MPath    = strdup("/tmp/XrdCS2d/");
   MPlen    = strlen(MPath);
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
   const char *subdir = "0123456789abcdef";
   const int   subnum = 16;
   struct stat buf;
   XrdNetSocket *EventSock;
   int n, EventFD, retc, minT, maxT = 100, NoGo = 0;
   const char *inP;
   char c, buff[2048], *logfn = 0;
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
       case 'q': if (!(maxT = atoi(optarg)))
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

// Create the "files" directory for intermediate symlinks
//
   strcpy(buff, MPath); 
   strcat(buff, "files/");
   if ((retc = XrdOucUtils::makePath(buff,0770)))
      {XrdLog.Emsg("Config", retc, "create symlink cache", buff);
       return 0;
      }

// Now create 16 cache directories (0 through f)
//
   buff[MPlen+1] = '/'; buff[MPlen+2] ='\0';
   for (n = 0; n < subnum; n++)
       {buff[MPlen] = subdir[n];
        if ((retc = XrdOucUtils::makePath(buff,0770)))
           {XrdLog.Emsg("Config", retc, "create recording cache", buff);
            return 0;
           }
       }

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

// Set the number of threads we want and start the scheduler
//
   if ((minT = maxT/10) < 10) minT = 10;
   if (minT > maxT) maxT = minT;
   XrdSched.setParms(minT, maxT, minT, maxT);
   XrdSched.Start(minT);

// Create the notification path (r/w for us and our group)
//
   if (!(EventSock = XrdNetSocket::Create(&XrdLog, MPath, "CS2.events",
                                   0660, XRDNET_FIFO))) NoGo = 1;
      else {EventFD = EventSock->Detach();
            delete EventSock;
            Events.Attach(EventFD, 32*1024);
           }

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
/*                                 S e t u p                                  */
/******************************************************************************/
  
int XrdCS2DCM::Setup()
{
// At this point a warm start would be signified by our parent pid being the
// same as when we last started. Otherwise, this is a cold start and we need
// to go through all files that we had opened and either tell Castor to
// release them or to prepare them for migration. For now, we ignore it.
//
   return !CS2_Init();
}
