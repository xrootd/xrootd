/******************************************************************************/
/*                                                                            */
/*                       X r d O d c F i n d e r . c c                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//          $Id$

const char *XrdOdcFinderCVSID = "$Id$";

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/wait.h>
  
#include "XrdOdc/XrdOdcConfig.hh"
#include "XrdOdc/XrdOdcFinder.hh"
#include "XrdOdc/XrdOdcManager.hh"
#include "XrdOdc/XrdOdcMsg.hh"
#include "XrdOdc/XrdOdcTrace.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucNetwork.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucReqID.hh"
#include "XrdOuc/XrdOucSocket.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTimer.hh"
#include "XrdSfs/XrdSfsInterface.hh"

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/

#define odcTICSTART 13

#define odcBADPORT  1
#define odcDEDSRVR  2
#define odcSLOSRVR  3
#define odcPIDSRVR  4
#define odcNEWSRVR  5
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

XrdOucError  OdcEDest(0, "odc_");
  
XrdOucTrace  OdcTrace(&OdcEDest);

char        *XrdOdcFinder::OLBPath = 0;

/******************************************************************************/
/*          L o c a l   M o n i t o r   D a t a   S t r u c t u r e           */
/******************************************************************************/

struct XrdOdcData
       {         pid_t  pid;
        unsigned int    port;
        unsigned char   torp[8];   // Area for exclusive use of inspector
        unsigned int    clock;     // Psuedo clock
                 int    numFD;     // Number open files
                 int    cpuLoad;   // cpu_time*100 / elpsd_time
                 int    cpuTime;   // CPU used in 100ths of a second
                 char   pad[128];  // Slop to allow for growth
       };

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOdcFinder::XrdOdcFinder(XrdOucLogger *lp, Persona acting)
{
   OdcEDest.logger(lp);
   myPersona = acting;
}

/******************************************************************************/
/*                          L o c a l   F i n d e r                           */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOdcFinderLCL::XrdOdcFinderLCL(XrdOucLogger *lp, int port)
               : XrdOdcFinder(lp, XrdOdcFinder::amLocal)
{
    myHost       = 0;
    myPort       = port;
    repint       = 0;
    repsad       = 0;
    repdata      = 0;
    pselPort     = selByFD;
    nument       = 0;
    nextent      = -1;
}
 
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOdcFinderLCL::~XrdOdcFinderLCL()
{
   if (repsad) shmdt((SHMDT_t) repsad);
   if (myHost) free(myHost);
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdOdcFinderLCL::Configure(char *cfn)
{
   const char *epname = "Config";
   XrdOdcConfig config(&OdcEDest, myPort);
   int repsid;

// Set the error dest and simply call the configration object
//
   if (config.Configure(cfn, "Local")) return 0;
   OLBPath = config.OLBPath;

// Attach shared memory
//
   if ((repsid = shmget(config.pselSkey, sizeof(XrdOdcData)*maxPORTS,
                        IPC_CREAT | 0744)) < 0)
      {OdcEDest.Emsg("Config", errno, "shmget()"); return 0;}
    repsad = (XrdOdcData *)shmat(repsid, NULL, 0);
    if ((void *)repsad == (void *)-1)
       {OdcEDest.Emsg("Config", errno, "shmat()"); repsad = 0; return 0;}
    DEBUG("Port balancing shared memory allocated; ID=" <<repsid);

// Set configured values and start the managers
//
   if (myPort != config.portVec[0]) 
      return StartMonitor(0, config.pselMint);

// We will be redirecting as well, establish locate policy
//
   switch (config.pselType)
          {case selByFD: doLocate = &XrdOdcFinderLCL::LocbyFD; break;
           case selByLD: doLocate = &XrdOdcFinderLCL::LocbyLD; break;
           default:      doLocate = &XrdOdcFinderLCL::LocbyRR;
          }

// Initialize the selection table
//
   nextent = 0; nument = 0;
   while((altserv[nument].port = config.portVec[nument]))
        {altserv[nument].pid  = 0; altserv[nument].numfd = 0;
         altserv[nument].load = 0; altserv[nument].ok = 0;
         altserv[nument].tics = 0;
         nument++;
        }
   DEBUG("Port balancing set for " <<nument <<" other servers");

// Start the inspection thread
//
   myHost = strdup(XrdOucNetwork::FullHostName(0));
   return StartMonitor(1, config.pselMint);
}
 
/******************************************************************************/
/*                                L o c a t e                                 */
/******************************************************************************/
  
int XrdOdcFinderLCL::Locate(XrdOucErrInfo &Resp, const char *path, int flags)
{
   int selent;

// Slect the proper port or defulat to ourselves
//
   if (!nument || !(selent = (this->*doLocate)())) return 0;

// Construct the redirection message
//
   Resp.setErrInfo(altserv[selent].port, myHost);
   return -EAGAIN;
}

int XrdOdcFinderLCL::LocbyFD()
{
    int i, j = 0;

    myData.Lock();
    for (i = 1; i < nument; i++)
        if (altserv[j].ok && altserv[i].numfd < altserv[j].numfd) j = i;
    altserv[j].numfd++;
    myData.UnLock();
    return j;
}

int XrdOdcFinderLCL::LocbyLD()
{
    int i, j = 0, minload;

    myData.Lock();
    minload = altserv[0].numfd + altserv[0].load;
    for (i = 1; i < nument; i++)
        if (altserv[j].ok
        && (altserv[i].numfd+altserv[i].load) < minload) 
           {minload = altserv[i].numfd + altserv[i].load; j = i;}
    altserv[j].numfd++;
    myData.UnLock();
    return j;
}

int XrdOdcFinderLCL::LocbyRR()
{
    int j;
    myData.Lock();
    if (nextent >= nument) j = nextent = 0;
       else j = nextent;
    nextent++;
    altserv[j].numfd++;
    myData.UnLock();
    return j;
}
  
/******************************************************************************/
/*                           I n s p e c t L o a d                            */
/******************************************************************************/
  
void *XrdOdcFinderLCL::InspectLoad()
{
   const char *epname = "InspectLoad";
   int i, load, port, sport, sleeptime;
   pid_t spid;
   long cputime;
   unsigned long stic;
   XrdOdcData *dp;

// Calculate sleep time
//
   if ((sleeptime = repint + repint/2) == repint) sleeptime = repint*2;
   sleeptime = sleeptime*1000;

// Simply grab whatever info we happen to have here
//
   while(1)
        {XrdOucTimer::Wait(sleeptime);
         calcLoad(load, cputime);
         myData.Lock();
         altserv[0].load = load; altserv[0].numfd = myFDcnt;
         for (i = 1; i < nument; i++)
             {port = altserv[i].port;
              dp = &repdata[port % maxPORTS];
              spid = dp->pid; stic = dp->clock; sport = dp->port;

              if (sport != port)
                 {if (altserv[i].ok) probLoad(odcBADPORT, i, sport, spid);
                  continue;
                 }
              if (kill(spid, 0))
                 {if (altserv[i].ok) probLoad(odcDEDSRVR, i, sport, spid);
                  continue;
                 }
              if (stic < odcTICSTART || stic == altserv[i].tics)
                 {if (altserv[i].ok) probLoad(odcSLOSRVR, i, sport, spid);
                  continue;
                 }

              if (altserv[i].ok)
                 {if (altserv[i].pid != spid)
                     {probLoad(odcPIDSRVR, i, sport, spid);
                      altserv[i].pid = spid;
                     }
                 } else if (!altserv[i].tics) {altserv[i].tics = 1; continue;}
                           else if (altserv[i].tics == 1)
                                   {probLoad(odcNEWSRVR,port,0,spid);
                                    altserv[i].ok = 1;
                                   }
              altserv[i].tics = stic;
              altserv[i].numfd = dp->numFD;
              altserv[i].load  = dp->cpuLoad;
              DEBUG("Updated server at port " <<port <<" pid=" <<spid <<" fd="
                     <<altserv[i].numfd <<" load=" <<altserv[i].load);
             }
         myData.UnLock();
        }
   return (void *)0;
}
  
/******************************************************************************/
/*                            R e p o r t L o a d                             */
/******************************************************************************/
  
void *XrdOdcFinderLCL::ReportLoad()
{
   unsigned long tics = odcTICSTART;
   int load;
   long totcpu;

// Simply calculate load ever interval
//
   calcLoad(load, totcpu); // Setup the values
   while(1)
        {XrdOucTimer::Wait(repint*1000);
         calcLoad(load, totcpu);
         repdata->cpuLoad = load;
         repdata->cpuTime = totcpu;
         repdata->clock   = tics++;
        }
   return (void *)0;
}

/******************************************************************************/
/*          P r i v a t e   L o c a l   F i n d e r   M e t h o d s           */
/******************************************************************************/
/******************************************************************************/
/*                              c a l c L o a d                               */
/******************************************************************************/
  
void XrdOdcFinderLCL::calcLoad(int &load, long &totcpu)
{
   const char *epname = "calcLoad";
   static clock_t lastcpu = 0;
   static clock_t lasttod = 0;
   static int     cpu = sysconf(_SC_NPROCESSORS_CONF);
   static int     tps = sysconf(_SC_CLK_TCK);
   static int     tph = (tps >= 100 ? tps / 100 : 1);
   static int     do60 = 60;
   struct tms     buff;
          int     tpi;
          clock_t nowtod;

// Get the cpu used
//
   if ((nowtod = times(&buff)) < 0)
      {OdcEDest.Emsg("calcLoad", errno, "get times()");
       load = 50; totcpu = 1;
       return;
      }

// Simply calculate load over interval
//
   tpi = (nowtod - lasttod) * tps * cpu;
   totcpu = buff.tms_cutime + buff.tms_cstime;
   if ((load = totcpu - lastcpu) < 0) load = 0;
   lastcpu = totcpu; totcpu = totcpu / tph;
   if ((load = (load * 100)/tpi) > 100) load = 100;
   lasttod = nowtod;

   if(QTRACE(Debug) && (do60 -= repint) <= 0)
     {DEBUG("Finder cpu time=" <<totcpu <<" load=" <<load);
      do60 = 60;
     }
}

/******************************************************************************/
/*                          S t a r t M o n i t o r                           */
/******************************************************************************/
  
extern "C"
{
void *XrdOdcReportLoad(void *carg)
      {XrdOdcFinderLCL *rp = (XrdOdcFinderLCL *)carg;
       return rp->ReportLoad();
      }

void *XrdOdcInspectLoad(void *carg)
      {XrdOdcFinderLCL *rp = (XrdOdcFinderLCL *)carg;
       return rp->InspectLoad();
      }
}

int XrdOdcFinderLCL::StartMonitor(int Inspect, int tint)
{
    const char *epname = "StartMonitor";
    pthread_t tid;
    int retc;

// Perform some rudimentary checks
//
    if (repint)
       {OdcEDest.Emsg("Finder","duplicate call; monitor already started.");
        return 0;
       }

    if (tint <= 0)
       {OdcEDest.Emsg("Finder", "invalid interval; monitor not started");
        return 0;
       }

    if (!repsad)
       {OdcEDest.Emsg("Finder", "no shared memory; monitor not started");
        return 0;
       }

// Compute the port umber being used here and the offset into the area
//
   if (!myPort)
       {OdcEDest.Emsg("Finder", "no port number; monitor not started");
        return 0;
       }
   repdata = &repsad[myPort % maxPORTS];
   DEBUG("Server port is " <<myPort <<" entry " <<myPort % maxPORTS);

// Initialiaze our little piece of shared memory
//
   memset(repdata, 0, sizeof(XrdOdcData));
   repdata->pid  = getpid(); repdata->port = myPort;
   repdata->cpuLoad = 50;    repdata->cpuTime = 0;
   repdata->clock = odcTICSTART;

// Start the recorder
//
   repint = tint;
   if (Inspect) retc = XrdOucThread_Run(&tid,XrdOdcInspectLoad,(void *)this);
      else retc = XrdOucThread_Run(&tid,XrdOdcReportLoad,(void *)this);
   if (retc)
      {OdcEDest.Emsg("Finder", errno, "start load monitor");
       return repint = 0;
      }

// All done
//
   DEBUG("Thread " <<(unsigned int)tid <<" monitoring load");
   return 1;
}

/******************************************************************************/
/*                              p r o b L o a d                               */
/******************************************************************************/
  
void XrdOdcFinderLCL::probLoad(int probnum, int sent, int sport, pid_t spid)
{
   char buff[128];
   int port = altserv[sent].port;

// Do problem selection
//
   switch (probnum)
          {case odcBADPORT:
                sprintf(buff,"server is using the wrong; port=%d (%d) pid=%d",
                             sport, port, spid);
                break;
           case odcDEDSRVR:
                sprintf(buff,"server has died; port=%d pid=%d",sport,spid);
                break;
           case odcSLOSRVR:
                sprintf(buff,"server not reporting; port=%d pid=%d",sport,spid);
                break;
           case odcPIDSRVR:
                sprintf(buff,"server changed pids; port=%d pid=%d->%d",
                              port,sport,spid);
                break;
           case odcNEWSRVR:
                sprintf(buff,"server activited; port=%d pid=%d",port,spid);
                break;
           default:
                sprintf(buff,"unknown server problem; port=%d (%d) pid=%d",
                              port,sport,spid);
                break;
          }
   OdcEDest.Emsg("probLoad",(const char *)buff);
   altserv[sent].ok = 0; altserv[sent].tics = 0;
}

/******************************************************************************/
/*                         R e m o t e   F i n d e r                          */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOdcFinderRMT::XrdOdcFinderRMT(XrdOucLogger *lp, int isProxy)
               : XrdOdcFinder(lp, (isProxy ? XrdOdcFinder::amProxy
                                           : XrdOdcFinder::amRemote))
{
     myManagers  = 0;
     myManCount  = 0;
     SMode       = 0;
}
 
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOdcFinderRMT::~XrdOdcFinderRMT()
{
    XrdOdcManager *mp, *nmp = myManagers;

    while((mp = nmp)) {nmp = mp->nextManager(); delete mp;}
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdOdcFinderRMT::Configure(char *cfn)
{
   XrdOdcConfig config(&OdcEDest, 0);

// Set the error dest and simply call the configration object
//
   if (config.Configure(cfn, (myPersona == XrdOdcFinder::amProxy ?
                             "Proxy" : "Remote"))) return 0;

// Set configured values and start the managers
//
   OLBPath    = config.OLBPath;
   RepDelay   = config.RepDelay;
   RepWait    = config.RepWait;
   ConWait    = config.ConWait;
   if (myPersona == XrdOdcFinder::amProxy)
           {SMode = config.SModeP; StartManagers(config.PanList);}
      else {SMode = config.SMode;  StartManagers(config.ManList);}

// All done
//
   return 1;
}

/******************************************************************************/
/*                               F o r w a r d                                */
/******************************************************************************/

int XrdOdcFinderRMT::Forward(XrdOucErrInfo &Resp, const char *cmd, 
                             char *arg1, char *arg2)
{
   int  i;
   XrdOdcManager *Manp;
   struct iovec xmsg[8];

// Make sure we are configured
//
   if (!myManagers)
      {OdcEDest.Emsg("Finder", "Forward() called prior to Configure().");
       Resp.setErrInfo(EPROTO, "Internal error locating file.");
       return -EPROTO;
      }

// Select the right manager for this request
//
   if (!(Manp = SelectManager(Resp, (char *)'/'))) return 1;

// Construct a message to be sent to the manager
//
              xmsg[0].iov_base = (char *)"0 "; xmsg[0].iov_len = 2;
              xmsg[1].iov_base = (char *)cmd;  xmsg[1].iov_len = strlen(cmd);
              i = 2;
   if (arg1) {xmsg[i].iov_base = (char *)" ";  xmsg[i++].iov_len = 1;
              xmsg[i].iov_base = arg1;         xmsg[i++].iov_len = strlen(arg1);
             }
   if (arg2) {xmsg[i].iov_base = (char *)" ";  xmsg[i++].iov_len = 1;
              xmsg[i].iov_base = arg2;         xmsg[i++].iov_len = strlen(arg2);
             }
              xmsg[i].iov_base = (char *)"\n"; xmsg[i++].iov_len = 1;

// Send message and simply wait for the reply
//
   if (Manp->Send(xmsg, i)) return 0;

// Indicate client should retry later
//
   Resp.setErrInfo(RepDelay, (const char *)"");
   return RepDelay;
}
  
/******************************************************************************/
/*                                L o c a t e                                 */
/******************************************************************************/
  
int XrdOdcFinderRMT::Locate(XrdOucErrInfo &Resp, const char *path, int flags)
{
   const char *epname = "Locate";
   int  val, retc, mlen, noresp = 0;
   char *colon, *tinfo, *msg, stype, ptype, mbuff[64];
   XrdOdcMsg *mp;
   XrdOdcManager *Manp;
   struct iovec xmsg[3];

// Make sure we are configured
//
   if (!myManagers)
      {OdcEDest.Emsg("Finder", "Locate() called prior to Configure().");
       Resp.setErrInfo(EPROTO, "Internal error locating file.");
       return -EPROTO;
      }

// Compute mode
//
   if (flags & (O_CREAT | O_WRONLY | O_RDWR)) ptype = 'w';
      else if (flags & O_NDELAY) ptype = 'x';
              else ptype = 'r';
   stype = (flags & O_SYNC ? 's' : ' ');

// Select the right manager for this request
//
   if (!(Manp = SelectManager(Resp, (char *)path))) return ConWait;

// Construct a message to be sent to the manager
//
   mp = XrdOdcMsg::Alloc(&Resp);
   mlen = sprintf(mbuff, "%d select%c %c ", mp->ID(), stype, ptype);
   xmsg[0].iov_base = mbuff;        xmsg[0].iov_len = mlen;
   xmsg[1].iov_base = (char *)path; xmsg[1].iov_len = strlen(path);
   xmsg[2].iov_base = (char *)"\n"; xmsg[2].iov_len = 1;

// Send message and simply wait for the reply
//
   if (!Manp->Send(xmsg, 3) || (noresp = mp->Wait4Reply(RepWait)))
      {Resp.setErrInfo(RepDelay, ""); 
       val = retc = RepDelay;
       tinfo = (char *)" No response from ";
       msg = Manp->Name();
      }
      else {msg = (char *)Resp.getErrText(retc);
                 if (retc == -EREMOTE)
                    {tinfo = (char *)" redirected to ";
                     if (!(colon = index(msg, (int)':'))) val = 0;
                        else {*colon = '\0';
                              val = atoi((const char *)(colon+1));
                             }
                     Resp.setErrCode(val);
                    }
            else if (retc == -EAGAIN)
                    {tinfo = (char *)" wait ";
                     if (!(retc = atoi((const char *)msg))) retc = RepDelay;
                     val = retc;
                     Resp.setErrInfo(val, "");
                    }
            else if (retc == -EINVAL)
                    {val = 0; tinfo = (char *)" error: ";}
            else    {sprintf(mbuff, "error %d: ", retc);
                     tinfo = mbuff;
                     val = retc;
                     retc = -EINVAL;
                    }
           }

// Do a trace
//
   TRACE(Redirect, "user=" <<Resp.getErrUser() <<tinfo <<msg <<' ' <<val
                           <<" path=" <<path);

// All done
//
   mp->Recycle();
   return retc;
}
  
/******************************************************************************/
/*                               P r e p a r e                                */
/******************************************************************************/
  
int XrdOdcFinderRMT::Prepare(XrdOucErrInfo &Resp, XrdSfsPrep &pargs)
{
   const char *epname = "Prepare";
   char mbuff1[32], mbuff2[32], *mode;
   XrdOucTList *tp;
   int allok, mint, pathloc, plenloc = 0;
   XrdOdcManager *Manp, *Womp;
   struct iovec iodata[8];

// Make sure we are configured
//
   if (!myManagers)
      {OdcEDest.Emsg("Finder", "Prepare() called prior to Configure().");
       Resp.setErrInfo(EPROTO, "Internal error preparing files.");
       return -EPROTO;
      }

// Check for a cancel request
//
   if (!(tp = pargs.paths))
      {if (!(Manp = SelectManager(Resp, 0))) return ConWait;
       iodata[0].iov_base = (char *)"0 prepdel ";
       iodata[0].iov_len  = 10;    //1234567890
       iodata[1].iov_base = pargs.reqid;
       iodata[1].iov_len  = strlen(pargs.reqid);
       iodata[2].iov_base = (char *)"\n ";
       iodata[2].iov_len  = 1;
       if (Manp->Send((const struct iovec *)&iodata, 3)) return 0;
          else {Resp.setErrInfo(RepDelay, (const char *)"");
                DEBUG("Finder: Failed to send prepare cancel to " <<Manp->Name()
                      <<" reqid=" <<pargs.reqid);
                return RepDelay;
               }
      }

// Decode the options and preset iovec. The format of the message is:
// 0 prepsel <reqid> <notify>-n <prty> <mode> <path>\n
//
   iodata[0].iov_base = (char *)"0 prepadd ";
   iodata[0].iov_len  = 10;       //1234567890
   iodata[1].iov_base = pargs.reqid;
   iodata[1].iov_len  = strlen(pargs.reqid);
   iodata[2].iov_base = (char *)" ";
   iodata[2].iov_len  = 1;
   if (!pargs.notify || !(pargs.opts & Prep_SENDACK))
      {iodata[3].iov_base = (char *)"*";
       iodata[3].iov_len  = 1;
       mode = (char *)" %d %cq ";
      } else {
       iodata[3].iov_base = pargs.notify;
       iodata[3].iov_len  = strlen(pargs.notify);
       plenloc = 4;         // Where the msg is in iodata
       mode = (pargs.opts & Prep_SENDERR ? (char *)"-%%d %d %cn "
                                         : (char *)"-%%d %d %cnq ");
      }
   iodata[4].iov_len  = sprintf(mbuff1, mode, (pargs.opts & Prep_PMASK),
                                (pargs.opts & Prep_WMODE ? 'w' : 'r'));
   iodata[4].iov_base = (plenloc ? mbuff2 : mbuff1);
   pathloc = 5;
   iodata[6].iov_base = (char *)"\n";
   iodata[6].iov_len  = 1;

// Distribute out paths to the various managers
//
   while(tp)
        {mint = (SMode == ODC_ROUNDROB
                ? XrdOucReqID::Index(myManCount, tp->text) : 0);
         Womp = Manp = myManTable[mint];
         do {if ((allok = (Manp->isActive()))) break;
            } while((Manp = Manp->nextManager()) != Womp);
         if (!allok) {SelectManFail(Resp); break;}

         iodata[pathloc].iov_base = tp->text;
         iodata[pathloc].iov_len  = strlen(tp->text);
         if (plenloc) iodata[plenloc].iov_len = 
                      sprintf(mbuff2, mbuff1, tp->val);

         DEBUG("Finder: Sending " <<Manp->Name() <<' ' <<iodata[0].iov_base
                      <<' ' <<iodata[1].iov_base <<' ' <<iodata[3].iov_base
                      <<' ' <<iodata[5].iov_base);

         if (!Manp->Send((const struct iovec *)&iodata, 7)) break;
         tp = tp->next;
        }

// Check if all went well
//
   if (!tp) return 0;
   Resp.setErrInfo(RepDelay, (const char *)"");
   DEBUG("Finder: Failed to send prepare to " <<Manp->Name()
                  <<" reqid=" <<pargs.reqid);
   return RepDelay;
}

/******************************************************************************/
/*                         S e l e c t M a n a g e r                          */
/******************************************************************************/
  
XrdOdcManager *XrdOdcFinderRMT::SelectManager(XrdOucErrInfo &Resp, char *path)
{
   XrdOdcManager *Womp, *Manp;

// Get where to start
//
   if (SMode != ODC_ROUNDROB || !path) Womp = Manp = myManagers;
      else Womp = Manp = myManTable[XrdOucReqID::Index(myManCount, path)];

// Find the next active server
//
   do {if (Manp->isActive()) return Manp;
      } while((Manp = Manp->nextManager()) != Womp);

// All managers are dead
//
   SelectManFail(Resp);
   return (XrdOdcManager *)0;
}
  
/******************************************************************************/
/*                         S e l e c t M a n F a i l                          */
/******************************************************************************/
  
void XrdOdcFinderRMT::SelectManFail(XrdOucErrInfo &Resp)
{
   const char *epname = "SelectManFail";
   static time_t nextMsg = 0;
   time_t now;

// All servers are dead, indicate so every minute
//
   now = time(0);
   myData.Lock();
   if (nextMsg < now)
      {nextMsg = now + 60;
       myData.UnLock();
       OdcEDest.Emsg("Finder", "All managers are disfunctional.");
      } else myData.UnLock();
   Resp.setErrInfo(ConWait, (const char *)"");
   TRACE(Redirect, "user=" <<Resp.getErrUser() <<" No managers available; wait " <<ConWait);
}

/******************************************************************************/
/*                         S t a r t M a n a g e r s                          */
/******************************************************************************/
  
extern "C"
{
void *XrdOdcStartManager(void *carg)
      {XrdOdcManager *mp = (XrdOdcManager *)carg;
       return mp->Start();
      }
}

int XrdOdcFinderRMT::StartManagers(XrdOucTList *myManList)
{
   const char *epname = "StartManagers";
   XrdOucTList *tp;
   XrdOdcManager *mp, *firstone = 0;
   int i = 0;
   pthread_t tid;
   char buff[128];

// Clear manager table
//
   memset((void *)myManTable, 0, sizeof(myManTable));

// For each manager, start a thread to handle it
//
   tp = myManList;
   while(tp && i < XRDODCMAXMAN)
        {mp = new XrdOdcManager(&OdcEDest, tp->text, tp->val, ConWait);
         myManTable[i] = mp;
         if (myManagers) mp->setNext(myManagers);
            else firstone = mp;
         myManagers = mp;
         if (XrdOucThread_Run(&tid, XrdOdcStartManager, (void *)mp))
            OdcEDest.Emsg("Config", errno, "start manager");
            else {mp->setTID(tid);
                  DEBUG("Config: Thread " <<(unsigned int)tid <<" manages " <<tp->text);
                 }
         tp = tp->next; i++;
        }

// Check if we exceeded maximum manager count
//
   if (tp) while(tp)
                {OdcEDest.Emsg("Config", "Too many managers;", tp->text,
                                         (char *)"ignored.");
                 tp = tp->next;
                }

// Make this a circular chain
//
   if (firstone) firstone->setNext(myManagers);

// Indicate how many managers have been started
//
   sprintf(buff, "%d manager(s) started.", i);
   OdcEDest.Emsg("Config", (const char *)buff);
   myManCount = i;

// All done
//
   return 0;
}
 
/******************************************************************************/
/*                         T a r g e t   F i n d e r                          */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOdcFinderTRG::XrdOdcFinderTRG(XrdOucLogger *lp, int isprime, int port)
               : XrdOdcFinder(lp, XrdOdcFinder::amTarget)
{
   char buff [256];
   Primary = isprime;
   OLBPath = 0;
   OLBp    = new XrdOucStream(&OdcEDest);
   Active  = 0;
   myPort  = port;
   sprintf(buff, "login %c %d port %d\n", (Primary ? 'p' : 's'), getpid(), port);
   Login = strdup(buff);
}
 
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOdcFinderTRG::~XrdOdcFinderTRG()
{
  if (OLBp)  delete OLBp;
  if (Login) free(Login);
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
extern "C"
{
void *XrdOdcStartOlb(void *carg)
      {XrdOdcFinderTRG *mp = (XrdOdcFinderTRG *)carg;
       return mp->Start();
      }
}
  
int XrdOdcFinderTRG::Configure(char *cfn)
{
   const char *epname = "Config";
   XrdOdcConfig config(&OdcEDest, 0);
   pthread_t tid;

// Set the error dest and simply call the configration object
//
   if (config.Configure(cfn, "Target")) return 0;
   if (!(OLBPath = config.OLBPath))
      {OdcEDest.Emsg("Config", "Unable to determine olb admin path"); return 0;}

// Start a thread to connect with the local olb
//
   if (XrdOucThread_Run(&tid, XrdOdcStartOlb, (void *)this))
      OdcEDest.Emsg("Config", errno, "start olb interface");
      else DEBUG("Config: olb i/f assigned to thread " <<(unsigned int)tid);

// All done
//
   return 1;
}
  
/******************************************************************************/
/*                               R e m o v e d                                */
/******************************************************************************/
  
void XrdOdcFinderTRG::Removed(const char *path)
{
   char *data[3];
   int   dlen[3];

// Set up to notify the olb domain that a file has been removed
//
   data[0] = (char *)"0 rmdid "; dlen[0] = 8;
   data[1] = (char *)path;       dlen[1] = strlen(path);
   data[2] = 0;                  dlen[2] = 0;

// Now send the notification
//
   myData.Lock();
   if (Active && OLBp->Put((const char **)data, (const int *)dlen))
      {OLBp->Close(); Active = 0;}
   myData.UnLock();
}

/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
void *XrdOdcFinderTRG::Start()
{
   int   retc;

// First step is to connect to the local server olb
//
   while(1)
        {do {Hookup();

             // Login to the olb
             //
             myData.Lock();
             retc = OLBp->Put(Login);
             myData.UnLock();
             if (retc) break;

             // Put up a read. We don't expect any responses but should the
             // olb die, we will notice and try to reconnect.
             //
             while(OLBp->GetLine()) {}
             break;
            } while(1);
         // The olb went away
         //
         myData.Lock();
         OLBp->Close();
         Active = 0;
         myData.UnLock();
         OdcEDest.Emsg("olb", "Lost contact with olb via", OLBPath);
         XrdOucTimer::Wait(10*1000);
        }

// We should never get here
//
   return (void *)0;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                H o o k u p                                 */
/******************************************************************************/
  
void XrdOdcFinderTRG::Hookup()
{
   struct stat buf;
   XrdOucSocket Sock(&OdcEDest);
   int tries = 6;

// Wait for the olb path to be created
//
   while(stat(OLBPath, &buf)) 
        {if (!tries--)
            {OdcEDest.Emsg("olb", "Waiting for olb path", OLBPath); tries=6;}
         XrdOucTimer::Wait(10*1000);
        }

// We can now ty to connect
//
   tries = 0;
   while(Sock.Open(OLBPath) < 0)
        {if (!tries--)
            {OdcEDest.Emsg("olb",Sock.LastError(),"connect to olb");
             tries = 6;
            }
         XrdOucTimer::Wait(10*1000);
        };

// Transfer the socket FD to a stream
//
   myData.Lock();
   Active = 1;
   OLBp->Attach(Sock.Detach());
   myData.UnLock();

// Tell the world
//
   OdcEDest.Emsg("olb", "Connected to olb via", OLBPath);
}
