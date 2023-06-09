
#include <string>
#include <vector>

#include <stdio.h>
#include <unistd.h>

#include "Xrd/XrdJob.hh"
#include "Xrd/XrdScheduler.hh"

#include "XrdOfs/XrdOfsPrepare.hh"

#include "XrdOss/XrdOss.hh"

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucGatherConf.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucTList.hh"

#include "XrdSec/XrdSecEntity.hh"

#include "XrdSfs/XrdSfsInterface.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTrace.hh"

#include "XrdVersion.hh"

/******************************************************************************/
/*                                M a c r o s                                 */
/******************************************************************************/

#define DEBUG(usr,x) if (Debug) SYSTRACE(SysTrace.,usr,EPName,0,x)

#define EPNAME(x) const char *EPName=x
  
/******************************************************************************/
/*                     X r d O f s P r e p G P I R e a l                      */
/******************************************************************************/
/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/

namespace XrdOfsPrepGPIReal
{
XrdSysMutex      gpiMutex;

XrdOucBuffPool  *bPool    = 0;;

XrdOss          *ossP     = 0;
XrdScheduler    *schedP   = 0;
XrdSysError     *eLog     = 0;
XrdOucProg      *pgmObj   = 0;

XrdSysCondVar    qryCond(0, "prepG query");
int              qryAllow = 8;   // Protected by the condition variable above
int              qryWait  = 0;   // Ditto
static const int qryMaxWT = 33;  // Maximum wait time

int              maxFiles = 48;
int              maxResp  = XrdOucEI::Max_Error_Len;

bool             addCGI   = false;
bool             Debug    = false;
bool             usePFN   = false;
char             okReq    = 0;

static const int okCancel = 0x01;
static const int okEvict  = 0x02;
static const int okPrep   = 0x04;
static const int okQuery  = 0x08;
static const int okStage  = 0x10;
static const int okAll    = 0x1f;

XrdSysTrace SysTrace("PrepGPI");
}
  
using namespace XrdOfsPrepGPIReal;
  
/******************************************************************************/
/*                           P r e p R e q u e s t                            */
/******************************************************************************/

namespace XrdOfsPrepGPIReal
{
struct PrepRequest
      {       PrepRequest *next;
       static PrepRequest *First;
       static PrepRequest *Last;


       static const int envSZ = 4;
       static const int argSZ = 12;

       const char  *argVec[argSZ];
       int          argCnt;
       int          envCnt;
       const char  *envVec[envSZ];

             char  *reqID;
       const char  *reqName;
       const char  *tID;

       std::vector<std::string> argMem;
       std::vector<std::string> envMem;

       const char *Info(char *bP, int bL)
                   {snprintf(bP, bL, "%s %s %s", tID, reqName, reqID);
                    return bP;
                   }

       PrepRequest() : next(0), argCnt(0), envCnt(0), reqID(0),
                       reqName("?"), tID("anon") {}
      ~PrepRequest() {if (reqID) free(reqID);}
};

PrepRequest *PrepRequest::First = 0;
PrepRequest *PrepRequest::Last  = 0;

}
  
/******************************************************************************/
/*                              P r e p G R u n                               */
/******************************************************************************/
  
namespace XrdOfsPrepGPIReal
{
class PrepGRun : public XrdJob
{
public:

void      DoIt() override;

int       Run(PrepRequest &req, char *bP=0, int bL=0);

void      Sched(PrepRequest *rP) {reqP = rP;
                                  schedP->Schedule(this);
                                 }

          PrepGRun(XrdOucProg &pgm) : prepProg(pgm) {}

       PrepGRun *next;
static PrepGRun *Q;

private:
         ~PrepGRun() {}  // Never gets deleted

int       Capture(PrepRequest &req, XrdOucStream &cmd, char *bP, int bL);
void      makeArgs(PrepRequest &req, const char *argVec[]);

PrepRequest *reqP;
XrdOucProg  &prepProg;
};

PrepGRun *PrepGRun::Q = 0;
}

/******************************************************************************/
/* Private:            P r e p G R u n : : C a p t u r e                      */
/******************************************************************************/

namespace XrdOfsPrepGPIReal
{
int PrepGRun::Capture(PrepRequest &req, XrdOucStream &cmd, char *bP, int bL)
{
   EPNAME("Capture");
   static const int bReserve = 40;
   char *lp, *bPNow = bP, *bPEnd = bP+bL-bReserve;
   int len;
   bool isTrunc = false;

// Make sure the buffer length is minimum we need
//
   if (bL < 256)
      {char ib[512];
       eLog->Emsg("PrepGRun","Prep exec for",req.Info(ib,sizeof(ib)),
                             "failed; invalid buffer size.");
       return -1;
      }

// Place all lines that will fit into the suplied buffer
//
   while((lp = cmd.GetLine()))
        {len = strlen(lp) + 1;
         if (bPNow + len >= bPEnd) {isTrunc = true; break;}
         if (len > 1)
            {strcpy(bPNow, lp);
             bPNow[len-1] = '\n';
             bPNow += len;
             DEBUG(req.tID, " +=> " <<lp);
            }
        }

// Take care of overflow lines
//
   while(lp)
        {DEBUG(req.tID, " -=> " <<lp);
         lp = cmd.GetLine();
        }

// Change last line to end with a null byte and compute total length
//
   if (bPNow == bP) len = snprintf(bP, bL, "No information available.") + 1;
      else {if (isTrunc) bPNow += snprintf(bPNow, bReserve,
                                    "***response has been truncated***");
               else *(bPNow-1) = 0;
            len = bPNow - bP + 1;
           }

// Return number of bytes in buffer
//
   return len;
}
}
  
/******************************************************************************/
/*                        P r e p G R u n : : D o I t                         */
/******************************************************************************/
  
namespace XrdOfsPrepGPIReal
{
void PrepGRun::DoIt()
{
  
// Run as many requests as we can
//
do{Run(*reqP);
   delete reqP;
   gpiMutex.Lock();
   if ((reqP = PrepRequest::First))
      {if (PrepRequest::First == PrepRequest::Last)
               PrepRequest::First = PrepRequest::Last = 0;
          else PrepRequest::First = PrepRequest::First->next;
      } else {
       next = Q;
       Q    = this;
      }
    gpiMutex.UnLock();
   } while(reqP);
}
}

/******************************************************************************/
/* Private:           P r e p G R u n : : m a k e A r g s                     */
/******************************************************************************/
  
namespace XrdOfsPrepGPIReal
{
void PrepGRun::makeArgs(PrepRequest &req, const char *argVec[])
{

// Copy front arguments
//
   memcpy(argVec, req.argVec, req.argCnt*sizeof(char*));

// Copy over all the allocated arguments
//
   int j = req.argCnt;
   for (int i = 0; i < (int)req.argMem.size(); i++)
       argVec[j++] = req.argMem[i].c_str();
}
}
  
/******************************************************************************/
/*                         P r e p G R u n : : R u n                          */
/******************************************************************************/
  
namespace XrdOfsPrepGPIReal
{
int PrepGRun::Run(PrepRequest &req, char *bP, int bL)
{
   EPNAME("Run");
   XrdOucStream cmd;
   char *lp;
   int rc, bytes = 0;

// Allocate a arg vector of appropriate size
//
   int n = req.argCnt + req.argMem.size();
   const char **argVec = (const char **)alloca((n+2) * sizeof(char*));

// Fill the vector
//
   makeArgs(req, argVec);

// Do some debugging
//
   DEBUG(req.tID,"Starting prep for "<<req.reqName<<' '<<req.reqID);

// Invoke the program
//
   rc = prepProg.Run(&cmd, argVec, n, req.envVec);

// Drain or capture any output
//
   if (!rc)
      {if (Debug)
          {DEBUG(req.tID, req.reqName<<' '<<req.reqID<<" output:");}
       if (!bP) while((lp = cmd.GetLine())) {DEBUG(req.tID," ==> "<<lp);}
          else bytes = Capture(req, cmd, bP, bL);
       rc = prepProg.RunDone(cmd);
      }

// Document unsuccessful end
//
   if (rc)
      {char ib[512];
       eLog->Emsg("PrepGRun","Prep exec for",req.Info(ib,sizeof(ib)),"failed.");
      }

// Return the error, success or number of bytes
//
   if (bP) return bytes;
   return (rc ? -1 : 0);
}
}

/******************************************************************************/
/*                               P r e p G P I                                */
/******************************************************************************/
  
namespace XrdOfsPrepGPIReal
{
class PrepGPI : public XrdOfsPrepare
{
public:

int            begin(      XrdSfsPrep      &pargs,
                           XrdOucErrInfo   &eInfo,
                     const XrdSecEntity    *client = 0) override;

int            cancel(      XrdSfsPrep     &pargs,
                            XrdOucErrInfo  &eInfo,
                      const XrdSecEntity   *client = 0) override;

int            query(      XrdSfsPrep      &pargs,
                           XrdOucErrInfo   &eInfo,
                     const XrdSecEntity    *client = 0) override;

               PrepGPI(PrepGRun &gRun) : qryRunner(gRun) {}

virtual       ~PrepGPI() {}

private:
const char  *ApplyN2N(const char *tid, const char *path, char *buff, int blen);
PrepRequest *Assemble(int &rc, const char *tid, const char *reqName,
                      XrdSfsPrep &pargs, const char *xOpt);
bool         reqFind(const char *reqid, PrepRequest *&rPP, PrepRequest *&rP,
                     bool del=false, bool locked=false);
int          RetErr(XrdOucErrInfo &eInfo, int rc, const char *txt1,
                                                  const char *txt2);
int          Xeq(PrepRequest *rP);

PrepGRun    &qryRunner;
};
}

/******************************************************************************/
/* Private:            P r e p G P I : : A p p l y N 2 N                      */
/******************************************************************************/

namespace XrdOfsPrepGPIReal
{
const char *PrepGPI::ApplyN2N(const char *tid, const char *path, char *buff,
                              int blen)
{
   int rc;

// Translate this path
//
   const char *pfn = ossP->Lfn2Pfn(path, buff, blen, rc);

// Diagnose any errors
//
   if (rc)
      {char buff[1024];
       snprintf(buff, sizeof(buff), "handle %s path", tid);
       eLog->Emsg("PrepGPI", rc, buff, path);
       pfn = 0;
      }

// Return result
//
   return pfn;
}
}
  
/******************************************************************************/
/* Private:            P r e p G P I : : A s s e m b l e                      */
/******************************************************************************/
  
namespace XrdOfsPrepGPIReal
{
PrepRequest *PrepGPI::Assemble(int &rc, const char *tid, const char *reqName,
                               XrdSfsPrep &pargs, const char *xOpt)
{
   PrepRequest *rP = new PrepRequest;
   int n;
   char buff[1024];

// Count the number of paths and size the vector to hold all of them
//
   XrdOucTList *pP = pargs.paths;
   n = 0;
   while(pP) {n++; pP = pP->next;}

// Make sure we don't have too many files here
//
   if (n > maxFiles) {rc = E2BIG; return 0;}
   rc = 0;

// Size the vector to accomodate the file arguments
//
   rP->argMem.reserve(n);

// Add trace ID to the environment, this is always the first element.
//                               0123456789012
   snprintf(buff, sizeof(buff), "XRDPREP_TID=%s", tid);
   rP->envMem.emplace_back(buff);

// Insert options and envars for request:
//  -a -C -n {fin | rdy} -p <n> -w -- <rid> <reqname> [<file> [...]]
//   1  2  3  4           5 6   7  8  9     10
//
//  Envars: XRDPREP_COLOC XRDPREP_NOTIFY XRDPREP_TID
//
   while(*xOpt)
        {switch(*xOpt)
            {case 'a': if (pargs.opts & Prep_FRESH)
                          rP->argVec[rP->argCnt++] = "-a";
                       break;
             case 'C': if (!(pargs.opts & Prep_COLOC)
                       ||  !pargs.paths || !pargs.paths->next) break;
                       snprintf(buff, sizeof(buff), "XRDPREP_COLOC=%s",
                                      pargs.paths->text);
                       rP->envMem.emplace_back(buff);
                       rP->argVec[rP->argCnt++] = "-C";
                       break;
             case 'n': if (!pargs.notify || !*pargs.notify) break;
                       snprintf(buff, sizeof(buff), "XRDPREP_NOTIFY=%s",
                                      pargs.notify);
                       rP->envMem.emplace_back(buff);
                       rP->argVec[rP->argCnt++] = "-n";
                       rP->argVec[rP->argCnt++] = (pargs.opts & Prep_SENDERR ?
                                                                "fin" : "rdy");
                       break;
             case 'p': n = pargs.opts & Prep_PMASK;
                       rP->argVec[rP->argCnt++] = "-p";
                            if (n == 0) rP->argVec[rP->argCnt++] = "0";
                       else if (n == 1) rP->argVec[rP->argCnt++] = "1";
                       else if (n == 2) rP->argVec[rP->argCnt++] = "2";
                       else             rP->argVec[rP->argCnt++] = "3";
                       break;
             case 'w': if (pargs.opts & Prep_WMODE)
                          rP->argVec[rP->argCnt++] = "-w";
                       break;
             default:  break;
            }
         xOpt++;
        }

// Complete the envVec as now no objects will be relocated in envMem
//
   for (n = 0; n < (int)rP->envMem.size(); n++)
       rP->envVec[n] = rP->envMem[n].c_str();
   rP->envVec[n] = 0;

// Extract out the trace ID
//
   rP->tID = rP->envMem[0].c_str() + 12;

// Add request id to the argument list
//
   rP->argVec[rP->argCnt++] = "--";
   rP->reqID = strdup(pargs.reqid);
   rP->argVec[rP->argCnt++] = rP->reqID;

// Add request name in the argument list
//
   rP->reqName = reqName;
   rP->argVec[rP->argCnt++] = reqName;

// Handle the path list, we have two generic cases with cgi or no cgi
//
   if ((pP = pargs.paths))
      {const char *path;
       if (addCGI)
          {XrdOucTList *cP = pargs.oinfo;
           char pBuff[8192];
           do {path = (usePFN ? ApplyN2N(tid,pP->text,buff,sizeof(buff)):pP->text);
               if (!path) continue;
               if (cP->text && *cP->text)
                  {snprintf(pBuff, sizeof(pBuff), "%s?%s", path, cP->text);
                   path = pBuff;
                  }
               rP->argMem.emplace_back(path);
               pP = pP->next;
              } while(pP);
          } else {
           while(pP)
           do {path = (usePFN ? ApplyN2N(tid,pP->text,buff,sizeof(buff)):pP->text);
               if (!path) continue;
               rP->argMem.emplace_back(path);
               pP = pP->next;
              } while(pP);
          }
      }

// All done return the request object
//
   return rP;
}
}

/******************************************************************************/
/*                        P r e p G P I : : b e g i n                         */
/******************************************************************************/

namespace XrdOfsPrepGPIReal
{
int PrepGPI::begin(      XrdSfsPrep      &pargs,
                         XrdOucErrInfo   &eInfo,
                   const XrdSecEntity    *client)
{
   const char *reqName, *reqOpts, *tid = (client ? client->tident : "anon");
   int  rc;
   bool ignore;

// Establish the actual request
//
        if (pargs.opts & Prep_EVICT)
           {reqName = "evict";
            reqOpts = "";
            ignore  = (okReq & okEvict) == 0;
           }
   else if (pargs.opts & Prep_STAGE)
           {reqName = "stage";
            reqOpts = "Cnpw";
            ignore  = (okReq & okStage) == 0;
           }
   else    {reqName = "prep";
            reqOpts = "Cnpw";
            ignore  = (okReq & okPrep)  == 0;
           }

// Check if this operation is supported
//
   if (ignore) return RetErr(eInfo, ENOTSUP, "process", reqName);

// Get a request request object
//
   PrepRequest *rP = Assemble(rc, tid, reqName, pargs, reqOpts);

// If we didn't get one or if there are no paths selected, complain
//
   if (!rP || rP->argMem.size() == 0)
      return RetErr(eInfo, (rc ? rc : EINVAL), reqName, "files");

// Either run or queue this request and return
//
   return Xeq(rP);
}
}

/******************************************************************************/
/*                       P r e p G P I : : c a n c e l                        */
/******************************************************************************/

namespace XrdOfsPrepGPIReal
{
int PrepGPI::cancel(      XrdSfsPrep      &pargs,
                          XrdOucErrInfo   &eInfo,
                    const XrdSecEntity    *client)
{
   const char *tid = (client ? client->tident : "anon");
   int rc;

// If the attached program does no know how to handle cancel, do the minimal
// thing and remove the request from the waiting queue if it is there.
//
   if (!(okReq & okCancel))
      {PrepRequest *rPP, *rP;
       int bL;
       char *bP = eInfo.getMsgBuff(bL);
       if (reqFind(pargs.reqid, rPP, rP, true))
          {bL = snprintf(bP, bL, "Request %s cancelled.", pargs.reqid);
          } else {
           bL = snprintf(bP, bL, "Request %s not cancellable.", pargs.reqid);
          }
       eInfo.setErrCode(bL);
       return SFS_DATA;
      }

// Get a request request object
//
   PrepRequest *rP = Assemble(rc, tid, "cancel", pargs, "n");

// If we didn't get one or if there are no paths selected, complain
//
   if (!rP) return RetErr(eInfo, (rc ? rc : EINVAL), "cancel", "files");

// Either run or queue this request and return
//
   return Xeq(rP);
}
}
  
/******************************************************************************/
/*                        P r e p G P I : : q u e r y                         */
/******************************************************************************/

namespace XrdOfsPrepGPIReal
{
int PrepGPI::query(      XrdSfsPrep      &pargs,
                         XrdOucErrInfo   &eInfo,
                   const XrdSecEntity    *client)
{
   EPNAME("Query");
   struct OucBuffer {XrdOucBuffer *pBuff;
                                   OucBuffer() : pBuff(0) {}
                                  ~OucBuffer() {if (pBuff) pBuff->Recycle();}
                    } OucBuff;
   const char *tid = (client ? client->tident : "anon");
   int rc, bL;
   char *bP = eInfo.getMsgBuff(bL);

// If the attached program does no know how to handle cancel, do the minimal
// thing and remove the request from the waiting queue if it is there.
//
   if (!(okReq & okQuery))
      {PrepRequest *rPP, *rP;
       if (reqFind(pargs.reqid, rPP, rP))
          {bL = snprintf(bP, bL, "Request %s queued.", pargs.reqid)+1;
          } else {
           bL = snprintf(bP, bL, "Request %s not queued.", pargs.reqid)+1;
          }
       eInfo.setErrCode(bL);
       return SFS_DATA;
      }

// Allocate a buffer if need be
//
   if (bPool)
      {OucBuff.pBuff = bPool->Alloc(maxResp);
       if (OucBuff.pBuff)
          {bP = OucBuff.pBuff->Buffer();
           bL = maxResp;
          }
      }

// Get a request request object
//
   PrepRequest *rP = Assemble(rc, tid, "query", pargs, "");

// If we didn't get one or if there are no paths selected, complain
//
   if (!rP) return RetErr(eInfo, (rc ? rc : EINVAL), "query", "request");

// Wait for our turn if need be. This is sloppy and spurious wakeups may
// cause us to exceed the allowed limit.
//
   qryCond.Lock();
   if (qryAllow) qryAllow--;
      else {qryWait++;
            DEBUG(tid, "Waiting to launch query "<<rP->reqID);
            rc = qryCond.Wait(qryMaxWT);
            qryWait--;
            if (!rc) qryAllow--;
               else  {qryCond.UnLock();
                      return RetErr(eInfo, ETIMEDOUT, "query", "request");
                     }
           }
    qryCond.UnLock();

// Run the query
//
   *bP = 0;
   rc = qryRunner.Run(*rP, bP, bL);

// Let the next query run
//
   qryCond.Lock();
   qryAllow++;
   if (qryWait) qryCond.Signal();
   qryCond.UnLock();

// See if this ended in an error
//
   if (rc <= 0) return RetErr(eInfo, ECANCELED, "query", "request");

// Return response
//
   if (!OucBuff.pBuff) eInfo.setErrCode(rc);
      else {OucBuff.pBuff->SetLen(rc);
            eInfo.setErrInfo(rc, OucBuff.pBuff);
            OucBuff.pBuff = 0;
           }
   return SFS_DATA;
}
}

/******************************************************************************/
/* Private:             P r e p G P I : : r e q F i n d                       */
/******************************************************************************/

namespace XrdOfsPrepGPIReal
{
bool PrepGPI::reqFind(const char *reqid, PrepRequest *&rPP, PrepRequest *&rP,
                    bool del, bool locked)
{
// Even thougth "*' requestid's can be queued they cannot be subject to find
//
   if (!strcmp("*", reqid)) return false;

// Handle locking
//
   if (!locked) gpiMutex.Lock();

// Find the element
//
   rPP = 0;
   rP = PrepRequest::First;
   while(rP && strcmp(reqid, rP->reqID)) {rPP = rP; rP = rP->next;}

// Check if we found the element and if we must delete it
//
   if (rP && del)
      {if (rPP) rPP->next           = rP->next;
          else  PrepRequest::First  = rP->next;
       if (rP == PrepRequest::Last) PrepRequest::Last = rPP;
       delete rP;
      }

// Return result
//
   if (!locked) gpiMutex.UnLock();
   return rP != 0;
}
}
  
/******************************************************************************/
/* Private:              P r e p G P I : : R e t E r r                        */
/******************************************************************************/

namespace XrdOfsPrepGPIReal
{
int PrepGPI::RetErr(XrdOucErrInfo &eInfo, int rc, const char *txt1,
                                                  const char *txt2)
{
   int bL;
   char *bP = eInfo.getMsgBuff(bL);

// Format messages
//
   snprintf(bP, bL, "Unable to %s %s; %s", txt1, txt2, XrdSysE2T(rc));
   eInfo.setErrCode(bL);
   return SFS_ERROR;
}
}
  
/******************************************************************************/
/* Private:                 P r e p G P I : : X e q                           */
/******************************************************************************/

namespace XrdOfsPrepGPIReal
{
int PrepGPI::Xeq(PrepRequest *rP)
{
   EPNAME("Xeq");
   PrepGRun *grP;
   const char *tid = rP->tID, *reqName = rP->reqName;
   char reqID[64];

// If we are debugging we need to copy some stuff before it escapes
//
   if (Debug) snprintf(reqID, sizeof(reqID), "%s", rP->reqID);
      else *reqID = 0;

// Run or queue this request
//
   gpiMutex.Lock();
   if ((grP = PrepGRun::Q))
      {PrepGRun::Q = PrepGRun::Q->next;
       grP->Sched(rP);
      } else {
       if (PrepRequest::First) rP->next = PrepRequest::Last;
          else PrepRequest::First = rP;
       PrepRequest::Last = rP;
     }
   gpiMutex.UnLock();

// Do some debugging
//
   DEBUG(tid, reqName<<" request "<<reqID<<(grP ? " scheduled" : " queued"));

// All Done
//
   return SFS_OK;
}
}
  
/******************************************************************************/
/*                      X r d O f s g e t P r e p a r e                       */
/******************************************************************************/

// Parameters: -admit <reqlist> [-cgi] [-maxfiles <n> [-maxreq <n>]
//             [-maxquery <n>] [-maxresp <sz>] [-pfn] -run <pgm>
//
// <request>: cancel | evict | prep | query | stage
// <reqlist>: <request>[,<request>]
  
extern "C"
{
XrdOfsPrepare *XrdOfsgetPrepare(XrdOfsgetPrepareArguments)
{
   XrdOucGatherConf gpiConf("prepgpi.parms", eDest);
   XrdOucString RunPgm, Token;
   char *tokP;
   int maxReq = 4;

// Save some of the arguments that we may need later
//
   eLog   = eDest;
   ossP   = theOss;
   schedP = (XrdScheduler *)(envP->GetPtr("XrdScheduler*"));

// If parameters specified on the preplib directive, use them. Otherwise,
// get them from the config file.
//
   if (!gpiConf.useData(parms)
   &&  gpiConf.Gather(confg, XrdOucGatherConf::only_body) < 0) return 0;

// Verify we actually have parameters (there is only one line of them).
//
   if (!(tokP = gpiConf.GetLine()) || !*tokP)
      {eLog->Emsg("PrepGPI", "Parameters not specified.");
       return 0;
      }

// Parse the parameters, they are space delimited
//
   while((tokP = gpiConf.GetToken()))
        {Token = tokP;
              if (Token == "-admit")
                 {if (!(tokP = gpiConf.GetToken()) || *tokP == '-')
                     {eLog->Emsg("PrepGPI", "-admit argument not specified.");
                      return 0;
                     }
                  XrdOucString Args(tokP);
                  int argPos = 0;
                  bool argOK = false;
                  while((argPos = Args.tokenize(Token, argPos, ',')) != -1)
                       {     if (Token ==  "cancel") okReq |= okCancel;
                        else if (Token ==  "evict")  okReq |= okEvict;
                        else if (Token ==  "prep")   okReq |= okPrep;
                        else if (Token ==  "query")  okReq |= okQuery;
                        else if (Token ==  "stage")  okReq |= okStage;
                        else if (Token ==  "all")    okReq |= okAll;
                        else {eLog->Emsg("PrepGPI", "Invalid -admit request -",
                                                     Token.c_str());
                              return 0;
                             }
                        argOK = true;
                       }
                  if (!argOK)
                     {eLog->Emsg("PrepGPI", "invalid -admit request list");
                      return 0;
                     }
                 }
         else if (Token == "-cgi")   addCGI= true;
         else if (Token == "-debug") Debug = true;
         else if (Token == "-maxfiles")
                 {if (!(tokP = gpiConf.GetToken()) || *tokP == '-')
                     {eLog->Emsg("PrepGPI", "-maxfiles argument not specified.");
                      return 0;
                     }
                  if (XrdOuca2x::a2i(*eLog, "PrepPGI -maxfiles", tokP,
                                            &maxFiles, 1, 1024)) return 0;
                 }
         else if (Token == "-maxquery")
                 {if (!(tokP = gpiConf.GetToken()) || *tokP == '-')
                     {eLog->Emsg("PrepGPI", "-maxquery argument not specified.");
                      return 0;
                     }
                  if (XrdOuca2x::a2i(*eLog, "PrepPGI -maxquery", tokP,
                                            &qryAllow, 1, 64)) return 0;
                 }
         else if (Token == "-maxreq")
                 {if (!(tokP = gpiConf.GetToken()) || *tokP == '-')
                     {eLog->Emsg("PrepGPI", "-maxreq argument not specified.");
                      return 0;
                     }
                  if (XrdOuca2x::a2i(*eLog, "PrepPGI -maxreq", tokP,
                                            &maxReq, 1, 64)) return 0;
                 }
         else if (Token == "-maxresp")
                 {if (!(tokP = gpiConf.GetToken()) || *tokP == '-')
                     {eLog->Emsg("PrepGPI", "-maxresp argument not specified.");
                      return 0;
                     }
                  long long rspsz;
                  if (XrdOuca2x::a2sz(*eLog, "PrepPGI -maxresp", tokP,
                                            &rspsz, 2048, 16777216)) return 0;
                  maxResp = static_cast<int>(rspsz);
                 }
         else if (Token == "-pfn")   usePFN = true;
         else if (Token == "-run")
                 {if (!(tokP = gpiConf.GetToken()) || *tokP == '-')
                     {eLog->Emsg("PrepGPI", "-run argument not specified.");
                      return 0;
                     }
                  RunPgm = tokP;
                 }
         else {eLog->Emsg("PrepGPI", "Invalid option -", Token.c_str());
               return 0;
              }
        }

// Make sure at least one request was enabled
//
   if (!(okReq & okAll))
      {eLog->Emsg("PrepGPI", "'-admit' was not specified.");
       return 0;
      }

// Make sure the prepare program was specified
//
   if (!RunPgm.length())
      {eLog->Emsg("PrepGPI", "prepare program not specified.");
       return 0;
      }

// Create a buffer pool for query responses if we need to
//
   if (maxResp > (int)XrdOucEI::Max_Error_Len)
      bPool = new XrdOucBuffPool(maxResp, maxResp);

// Set final debug flags
//
   if (!Debug) Debug = getenv("XRDDEBUG") != 0;
   SysTrace.SetLogger(eLog->logger());

// Obtain an instance of the program object for this command. Note that
// all grun object will share this program as it's thread safe in the
// context in which we will use it (i.e. read/only).
//
   pgmObj = new XrdOucProg(eLog, 0); // EFD????
   if (pgmObj->Setup(RunPgm.c_str()))
      {delete pgmObj;
       eLog->Emsg("PrepGPI", "Unable to use prepare program", RunPgm.c_str());
       return 0;
      }

// Create as many run object as we need
//
   PrepGRun *gRun;
   while(maxReq--)
        {gRun = new PrepGRun(*pgmObj);
         gRun->next  = PrepGRun::Q;
         PrepGRun::Q = gRun;
        }

// Create one additional such object for queries to pass to the plugin
//
   gRun = new PrepGRun(*pgmObj);

// Return an instance of the prepare plugin
//
   return new PrepGPI(*gRun);
}
}
XrdVERSIONINFO(XrdOfsgetPrepare,PrepGPI);
