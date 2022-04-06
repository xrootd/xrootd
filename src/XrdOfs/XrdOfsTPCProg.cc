/******************************************************************************/
/*                                                                            */
/*                      X r d O f s T P C P r o g . c c                       */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <cstdio>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
  
#include "XrdNet/XrdNetIdentity.hh"
#include "XrdOfs/XrdOfsTPC.hh"
#include "XrdOfs/XrdOfsTPCConfig.hh"
#include "XrdOfs/XrdOfsTPCJob.hh"
#include "XrdOfs/XrdOfsTPCProg.hh"
#include "XrdOfs/XrdOfsTrace.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucCallBack.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysHeaders.hh"

#include "XrdXrootd/XrdXrootdTpcMon.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
extern XrdSysError  OfsEroute;
extern XrdSysTrace  OfsTrace;
extern XrdOss      *XrdOfsOss;

namespace XrdOfsTPCParms
{
extern XrdOfsTPCConfig Cfg;
}

using namespace XrdOfsTPCParms;

/******************************************************************************/
/*                      S t a t i c   V a r i a b l e s                       */
/******************************************************************************/
  
XrdSysMutex        XrdOfsTPCProg::pgmMutex;
XrdOfsTPCProg     *XrdOfsTPCProg::pgmIdle  = 0;

/******************************************************************************/
/*                     E x t e r n a l   L i n k a g e s                      */
/******************************************************************************/
  
void *XrdOfsTPCProgRun(void *pp)
{
     XrdOfsTPCProg *theProg = (XrdOfsTPCProg *)pp;
     theProg->Run();
     return (void *)0;
}

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

namespace
{
class credFile
{
public:

char *Path;
char  pEnv[MAXPATHLEN+65];

     credFile(XrdOfsTPCJob *jP)
             {if (jP->Info.Csz > 0 && jP->Info.Crd && jP->Info.Env)
                 {int n;
                  csMutex.Lock(); n = cSeq++; csMutex.UnLock();
                  snprintf(pEnv, sizeof(pEnv), "%s=%s%s#%d.creds",
                           jP->Info.Env, jP->credPath(), jP->Info.Org, n);
                  Path = index(pEnv,'=')+1;
                 } else Path = 0;
             }

    ~credFile() {if (Path) unlink(Path);}

private:
static XrdSysMutex csMutex;
static int         cSeq;
};

XrdSysMutex credFile::csMutex;
int         credFile::cSeq = 0;
}
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOfsTPCProg::XrdOfsTPCProg(XrdOfsTPCProg *Prev, int num, int errMon)
             : Prog(&OfsEroute, errMon),
               JobStream(&OfsEroute),
               Next(Prev), Job(0)
             {snprintf(Pname, sizeof(Pname), "TPC job %d: ", num);
              Pname[sizeof(Pname)-1] = 0;
             }
  
/******************************************************************************/
/*                           E x p o r t C r e d s                            */
/******************************************************************************/

int XrdOfsTPCProg::ExportCreds(const char *path)
{
static const int    oOpts = (O_CREAT | O_TRUNC | O_WRONLY);
static const mode_t oMode = (S_IRUSR | S_IWUSR);

int fd, rc;

// Open the file as if it were new
//
   fd = XrdSysFD_Open(path, oOpts, oMode);
   if (fd < 0)
      {rc = errno;
       OfsEroute.Emsg("TPC", rc, "create credentials file", path);
       return -rc;
      }

// Write out the credentials
//
   if (write(fd, Job->Info.Crd, Job->Info.Csz) < 0)
      {rc = errno;
       OfsEroute.Emsg("TPC", rc, "write credentials file", path);
      } else rc = 0;

// Close the file and return (we ignore close errors)
//
   close(fd);
   return rc;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
int XrdOfsTPCProg::Init()
{
   int n;

// Allocate copy program objects
//
   for (n = 0; n < Cfg.xfrMax; n++)
       {pgmIdle = new XrdOfsTPCProg(pgmIdle, n, Cfg.errMon);
        if (pgmIdle->Prog.Setup(Cfg.XfrProg, &OfsEroute)) return 0;
       }

// All done
//
   Cfg.doEcho = Cfg.doEcho || GTRACE(debug);
   return 1;
}

/******************************************************************************/
/*                                   R u n                                    */
/******************************************************************************/

void XrdOfsTPCProg::Run()
{
   XrdXrootdTpcMon::TpcInfo monInfo;
   struct stat Stat;
   const char *clID, *at;
   char *questSrc, *questLfn, *questDst;
   int rc;
   bool isIPv4, doMon = Cfg.tpcMon != 0;
   char clBuff[592];

// Run the current job and indicate it's ending status and possibly getting a
// another job to run. Note "Job" will always be valid.
//
do{if (doMon)
      {monInfo.Init();
       gettimeofday(&monInfo.begT, 0);
      }

   rc = Xeq(isIPv4);

   if (doMon)
      {gettimeofday(&monInfo.endT, 0);
       if ((questSrc = index(Job->Info.Key, '?'))) *questSrc = 0;
       monInfo.srcURL = Job->Info.Key;
       if ((questLfn = index(Job->Info.Lfn, '?'))) *questLfn = 0;
       monInfo.dstURL = Job->Info.Lfn;
       monInfo.endRC  = rc;
       if (Job->Info.Str) monInfo.strm = Job->Info.Str;
       if (isIPv4) monInfo.opts |= XrdXrootdTpcMon::TpcInfo::isIPv4;

       clID = Job->Info.Org;
       if (clID && (at = index(clID, '@')) && !index(at+1, '.'))
          {const char *dName = XrdNetIdentity::Domain();
           if (dName)
              {snprintf(clBuff, sizeof(clBuff), "%s%s", clID, dName);
               clID = clBuff;
              }
          }
       monInfo.clID = clID;

       if ((questDst = index(Job->Info.Dst, '?'))) *questDst = 0;
       if (!XrdOfsOss->Stat(Job->Info.Dst, &Stat)) monInfo.fSize = Stat.st_size;
       if (questDst) *questDst = '?';
       Cfg.tpcMon->Report(monInfo);
       if (questLfn) *questLfn = '?';
       if (questSrc) *questSrc = '?';
      }

   Job = Job->Done(this, eRec, rc);

  } while(Job);

// No more jobs to run. Place us on the idle queue. Upon return this thread
// will end.
//
   pgmMutex.Lock();
   Next = pgmIdle;
   pgmIdle = this;
   pgmMutex.UnLock();
}
  
/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
XrdOfsTPCProg *XrdOfsTPCProg::Start(XrdOfsTPCJob *jP, int &rc)
{
   XrdSysMutexHelper pgmMon(&pgmMutex);
   XrdOfsTPCProg    *pgmP;
   pthread_t         tid;

// Get a new program object, if none left, tell the caller to try later
//
   if (!(pgmP = pgmIdle)) {rc = 0; return 0;}
   pgmP->Job = jP;

// Start a thread to run the job
//
   if ((rc = XrdSysThread::Run(&tid, XrdOfsTPCProgRun, (void *)pgmP, 0,
                                "TPC job")))
      return 0;

// We are all set, return the program being used
//
   pgmIdle = pgmP->Next;
   return pgmP;
}

/******************************************************************************/
/*                                   X e q                                    */
/******************************************************************************/

int XrdOfsTPCProg::Xeq(bool &isIPv4)
{
   EPNAME("Xeq");
   credFile cFile(Job);
   const char *Args[6], *eVec[6], **envArg;
   char *lP, *Colon, *cksVal, sBuff[8], *tident = Job->Info.Org;
   char *Quest = index(Job->Info.Key, '?');
   int i, rc, aNum = 0;

// If we have credentials, write them out to a file
//
   if (cFile.Path && (rc = ExportCreds(cFile.Path)))
      {strcpy(eRec, "Copy failed; unable to pass credentials.");
       return rc;
      }

// Echo out what we are doing if so desired
//
   if (Cfg.doEcho)
      {if (Quest) *Quest = 0;
       OfsEroute.Say(Pname,tident," copying ",Job->Info.Key," to ",Job->Info.Dst);
       if (Quest) *Quest = '?';
      }

// Determine checksum option
//
   cksVal = (Job->Info.Cks ? Job->Info.Cks : Cfg.cksType);
   if (cksVal)
      {Args[aNum++] = "-C";
       Args[aNum++] = cksVal;
      }

// Set streams option if need be
//
   if (Job->Info.Str)
      {sprintf(sBuff, "%d", static_cast<int>(Job->Info.Str));
       Args[aNum++] = "-S";
       Args[aNum++] = sBuff;
      }

// Set remaining arguments
//
   Args[aNum++] = Job->Info.Key;
   Args[aNum++] = Job->Info.Dst;

// Always export the trace identifier of the original issuer
//
   char tidBuff[512];
   snprintf(tidBuff, sizeof(tidBuff), "XRD_TIDENT=%s", tident);
   eVec[0] = tidBuff;
   envArg = eVec;
   i = 1;

// Export source protocol if present
//
   char sprBuff[128];
   if (Job->Info.Spr)
      {snprintf(sprBuff, sizeof(sprBuff), "XRDTPC_SPROT=%s", Job->Info.Spr);
       eVec[i++] = sprBuff;
      }

// Export target protocol if present
//
   char tprBuff[128];
   if (Job->Info.Tpr)
      {snprintf(tprBuff, sizeof(tprBuff), "XRDTPC_TPROT=%s", Job->Info.Tpr);
       eVec[i++] = tprBuff;
      }

// If we need to reproxy, export the path
//
   char rpxBuff[1024];
   if (Job->Info.Rpx)
      {snprintf(rpxBuff, sizeof(rpxBuff), "XRD_CPTARGET=%s", Job->Info.Rpx);
       eVec[i++] = rpxBuff;
      }

// Determine if credentials are being passed, If so, pass where it is.
//
   if (cFile.Path) eVec[i++] = cFile.pEnv;
   eVec[i] = 0;

// Start the job.
//
   if ((rc = Prog.Run(&JobStream, Args, aNum, envArg)))
      {strcpy(eRec, "Copy failed; unable to start job.");
       OfsEroute.Emsg("TPC", Job->Info.Org, Job->Info.Lfn, eRec);
       return rc;
      }

// Now we drain the output looking for an end of run line. This line should
// be printed as an error message should the copy fail.
//
   *eRec = 0;
   isIPv4 = false;
   while((lP = JobStream.GetLine()))
        {if (!strcmp(lP, "!-!IPv4")) isIPv4 = true;
         if ((Colon = index(lP, ':')) && *(Colon+1) == ' ')
            {strncpy(eRec, Colon+2, sizeof(eRec)-1); 
             eRec[sizeof(eRec)-1] = 0;
            }
         if (Cfg.doEcho && *lP) OfsEroute.Say(Pname, lP);
        }

// The job has completed. So, we must get the ending status.
//
   if ((rc = Prog.RunDone(JobStream)) < 0) rc = -rc;
   DEBUG(Pname <<"ended with rc=" <<rc);

// Check if we should generate a message
//
   if (rc && !(*eRec)) sprintf(eRec, "Copy failed with return code %d", rc);

// Log failures and optionally remove the file (Info would do that as well
// but much later on, so we do it now).
//
   if (rc)
      {OfsEroute.Emsg("TPC", Job->Info.Org, Job->Info.Lfn, eRec);
       if (Cfg.autoRM) XrdOfsOss->Unlink(Job->Info.Lfn);
      } else Job->Info.Success();

// All done
//
   return rc;
}
