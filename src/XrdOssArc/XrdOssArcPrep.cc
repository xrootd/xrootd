/******************************************************************************/
/*                                                                            */
/*                      X r d O s s A r c P r e p . c c                       */
/*                                                                            */
/* (c) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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

/******************************************************************************/
/*                             I n c l u d e s                                */
/******************************************************************************/

#include <string>

#include <stdio.h>
#include <unistd.h>

#include "XrdOfs/XrdOfsPrepare.hh"

#include "XrdOssArc/XrdOssArcDataset.hh"
#include "XrdOssArc/XrdOssArcTrace.hh"

#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucTList.hh"

#include "XrdSec/XrdSecEntity.hh"

#include "XrdSfs/XrdSfsInterface.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysE2T.hh"

#include "XrdVersion.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

class XrdOssArc;
  
namespace XrdOssArcGlobals
{
       XrdOucBuffPool  bPool(1024, 65536);

extern XrdOssArc*      ArcSS;
extern XrdSysTrace     ArcTrace;
}
using namespace XrdOssArcGlobals;

/******************************************************************************/
/*                                                                            */
/*                   C l a s s   X r d O s s A r c P r e p                    */
/*                                                                            */
/******************************************************************************/
  
class XrdOssArcPrep : public XrdOfsPrepare
{
public:

int            begin(      XrdSfsPrep      &pargs,
                           XrdOucErrInfo   &eInfo,
                     const XrdSecEntity    *client = 0) override;

int            cancel(      XrdSfsPrep     &pargs,
                            XrdOucErrInfo  &eInfo,
                      const XrdSecEntity   *client = 0) override;

void           Init(const char* parms, XrdSysError* errP, XrdOss* theOss,
                    XrdOucEnv*  envP);

int            query(      XrdSfsPrep      &pargs,
                           XrdOucErrInfo   &eInfo,
                     const XrdSecEntity    *client = 0) override;

               XrdOssArcPrep(XrdOfsPrepare* prepP)
                            : nextPrepPI(prepP), Elog(0,"ArcPrep_") {}

virtual       ~XrdOssArcPrep() {}

private:

void Add2Resp(const char* path, std::string& rspBuff, bool isMine,
              time_t reqTime=0);
int  RetErr(XrdOucErrInfo &eInfo, int rc, const char *txt1, const char *txt2);
void setResp(XrdSfsPrep& pargs, std::string& rspVec, XrdOucErrInfo& eInfo);

XrdOfsPrepare*  nextPrepPI;
XrdOss*         ossP = 0;
XrdSysError     Elog;
};

/******************************************************************************/
/*               X r d O s s A r c P r e p : : a d d 2 R e s p                */
/******************************************************************************/

void XrdOssArcPrep::Add2Resp(const char* path, std::string& rspBuff,
                             bool isMine, time_t reqTime)
{
//                         [|,           lfn                  true|false
   static const char *fmt="%s{\"path\":\"%s\",\"path_exists\":%s,"
//                                                            true|false
                           "\"error_text\":\"%s\",\"on_tape\":%s,"
//                                     true|false
                           "\"online\":%s,\"requested\":false,"
//                                        true|false
                           "\"has_reqid\":%s,\"req_time\":%llu}";

    const char *pExists, *eTxt="", *onTape, *hasID, *online, *bcont;
    char buff[4096];
    unsigned long long theTime;

// Set appropriate values
//
   if (isMine)
      {pExists = "true";
       onTape  = "true";
       online  = "false";
       hasID   = "true";
      } else {
       pExists = "true";
       onTape  = "false";
       online  = "false";
       hasID   = "false";
      }
    bcont = (rspBuff.size() ? "," : "[");

// Get corect time value
//
   if (reqTime) theTime = static_cast<unsigned long long>(reqTime);
      else theTime = static_cast<unsigned long long>(time(0));

// Format the response
//
   snprintf(buff, sizeof(buff), fmt, bcont, path, pExists, eTxt, onTape,
                                online, hasID, theTime); 

// Append the result to the response buffer
//
   rspBuff += buff;  
}
  
/******************************************************************************/
/*                  X r d O s s A r c P r e p : : b e g i n                   */
/******************************************************************************/

int XrdOssArcPrep::begin(      XrdSfsPrep&    pargs,
                               XrdOucErrInfo& eInfo,
                         const XrdSecEntity*  client)
{

// Route or ignore this request
//
   return (nextPrepPI ? nextPrepPI->begin(pargs, eInfo, client) : SFS_OK);
}

/******************************************************************************/
/*                 X r d O s s A r c P r e p : : c a n c e l                  */
/******************************************************************************/
  
int XrdOssArcPrep::cancel(      XrdSfsPrep&    pargs,
                                XrdOucErrInfo& eInfo,
                          const XrdSecEntity*  client)
{

// Route or ignore this request
//
   return (nextPrepPI ? nextPrepPI->begin(pargs, eInfo, client) : SFS_OK);
}
  
/******************************************************************************/
/*                   X r d O s s A r c P r e p : : I n i t                    */
/******************************************************************************/

void XrdOssArcPrep::Init(const char *parms, XrdSysError* errP, XrdOss* theOss,
                         XrdOucEnv* envP)
{

// Save some of the arguments that we may need later
//
   Elog.logger(errP->logger());
   ossP = theOss;

// Set debug info. This may be set via the OssArc plugin as well.
//
   if (getenv("XRDDEBUG") || getenv("XRDOSSARC_DEBUG"))
      ArcTrace.What |= TRACE_Debug;
}
  
/******************************************************************************/
/*                  X r d O s s A r c P r e p : : q u e r y                   */
/******************************************************************************/

int XrdOssArcPrep::query(      XrdSfsPrep&    pargs,
                               XrdOucErrInfo& eInfo,
                         const XrdSecEntity*  client)
{
   TraceInfo("PrepQuery", client->tident);
   std::string respBuff;
   time_t reqTime;
   int pathCnt = 0, procCnt = 0;

// For each path attempt to post a completion. If any are successful, then we
// ignore those that are not as there should not have been a mixed-mode query.
// If none were successful, we forward the query if need be.
//
   XrdOucTList* pathP = pargs.paths;
   while(pathP)
        {pathCnt++;
         if (ArcSS && !XrdOssArcDataset::Complete(pathP->text, reqTime))
            {Add2Resp(pathP->text, respBuff, true, reqTime);
             procCnt++;
            }
         pathP = pathP->next;
        }

// If the response buffer is empty then forward this request
//
   if (respBuff.size() == 0)
      {if (nextPrepPI)
          {DEBUG("No query prepare paths refer to archive; forwarding query.");
           return nextPrepPI->query(pargs, eInfo, client);
          } else {
           DEBUG("No query prepare paths refer to archive; returning fake resp!");
           pathP = pargs.paths;
           while(pathP)
                {Add2Resp(pathP->text, respBuff, false);
                 pathP = pathP->next;
                }
          }
      }

// Complete the response by putting a header on it and moving it to
// a independent buffer of sufficient size.
//
   setResp(pargs, respBuff, eInfo);

// Do some debugging
//
   int n;
   DEBUG("Query prepare response for "<<procCnt<<" archive of "
         <<pathCnt<<" paths:\n"<<eInfo.getErrText(n));

// All done
//
   return SFS_DATA;
}
  
/******************************************************************************/
/* Private:        X r d O s s A r c P r e p : : R e t E r r                  */
/******************************************************************************/

int XrdOssArcPrep::RetErr(XrdOucErrInfo &eInfo, int rc, const char *txt1,
                                                  const char *txt2)
{
   char *bP;
   int bL;

// Set error code and get the buffer
//
   eInfo.setErrCode(rc);
   bP = eInfo.getMsgBuff(bL);

// Format messages
//
   snprintf(bP, bL, "Unable to %s %s; %s", txt1, txt2, XrdSysE2T(rc));
   return SFS_ERROR;
}
  
/******************************************************************************/
/* Private:       X r d O s s A r c P r e p : : s e t R e s p                 */
/******************************************************************************/
  
void XrdOssArcPrep::setResp(XrdSfsPrep& pargs, std::string& rspVec,
                            XrdOucErrInfo& eInfo)
{
   static const char* fmt = "{\"request_id\":\"%s\",\"responses\":%s}";
   static const int fmtLen = strlen(fmt);
   const char* reqID = (pargs.reqid ? pargs.reqid : "");
   int n, bL;
   char *bP = eInfo.getMsgBuff(bL);
   XrdOucBuffer* pBuff = 0;

// Calculate full length of response (we may be a bit over)
//
   n = fmtLen + strlen(reqID) + rspVec.size();

// Check if we need to allocate a buffer or response will fit in preallocated
//
   if (n < bL) eInfo.setErrCode(0);
   if (n >= bL)
      {pBuff = bPool.Alloc(n);
       bP = pBuff->Data();
       bL = pBuff->BuffSize();
      }

// Copy the response into the buffer
//
   n = snprintf(bP, bL, fmt, reqID, rspVec.c_str()) + 1;
   if (!pBuff) eInfo.setErrCode(n);
      else {pBuff->SetLen(n);
            eInfo.setErrInfo(n, pBuff);
           }
}

/******************************************************************************/
/*                      X r d O f s A d d P r e p a r e                       */
/******************************************************************************/

extern "C"
{
XrdOfsPrepare *XrdOfsAddPrepare(XrdOfsAddPrepareArguments)
{

// Return an instance of the prepare plugin
//
   XrdOssArcPrep* myPrep = new XrdOssArcPrep(prepP);
   myPrep->Init(parms, eDest, theOss, envP);
   return myPrep;
}
}
XrdVERSIONINFO(XrdOfsAddPrepare,PrepOssArc);

/******************************************************************************/
/*                      X r d O f s g e t P r e p a r e                       */
/******************************************************************************/

extern "C"
{
XrdOfsPrepare *XrdOfsgetPrepare(XrdOfsgetPrepareArguments)
{

// Return an instance of the prepare plugin
//
   return XrdOfsAddPrepare(eDest, confg, parms, theSfs, theOss, envP, 0);
}
}
XrdVERSIONINFO(XrdOfsgetPrepare,PrepOssArc);
