/******************************************************************************/
/*                                                                            */
/*                     X r d O s s A r c S t a g e . c c                      */
/*                                                                            */
/* (c) 2024 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <queue>
#include <string>
#include <set>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#include "Xrd/XrdScheduler.hh"
#include "XrdOssArc/XrdOssArcConfig.hh"
#include "XrdOssArc/XrdOssArcStage.hh"
#include "XrdOssArc/XrdOssArcTrace.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdOssArcGlobals
{
extern XrdScheduler*   schedP;

extern XrdSysError     Elog;

extern XrdOssArcConfig Config;

XrdSysMutex schedMtx;
XrdSysMutex stageMtx;

struct ActInfo
      {int         rc;
       const char* path;
             char* pMem;

       ActInfo(const char* p, bool cpy=false) : rc(0)
              {if (cpy) path = pMem = strdup(p);
                  else {path = p; pMem = 0;} 
              }

      ~ActInfo() {if (pMem) free(pMem);}
      };

bool cmpLess(const ActInfo* a, const ActInfo* b)
            {return strcmp(a->path, b->path) < 0;}

std::set<ActInfo*, decltype(&cmpLess)> Active(&cmpLess);

std::queue<const char*> Pending;
}
using namespace XrdOssArcGlobals;

/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/

void XrdOssArcStage::DoIt()
{
   TraceInfo("Bring_Online",0);
   char  pBuff[MAXPATHLEN];
   const char* nxtPath;
   time_t seTime;
   int   fd, rc;

// Generate the path to the tape buffer as that's where it will be placed
// and open it to force it to be stage online.
//
do{rc = Config.GenTapePath(arcvPath, pBuff, sizeof(pBuff));
   if (rc) StageError(rc, "prepare file for staging", arcvPath);
      else {DEBUG("Staging "<<pBuff);
            seTime = time(0);
            if ((fd = XrdSysFD_Open(pBuff, O_RDONLY)) < 0)
               StageError(errno, "open/stage file", pBuff);
               else {close(fd);
                     seTime = time(0) - seTime;
                     DEBUG(pBuff<<" staged in "<<seTime<<" second(s)");
                    }
           }

// Check if there is something pending that we can do now
//
   schedMtx.Lock();
   if (Pending.empty()) break;
   nxtPath = Pending.front();
   Pending.pop();
   schedMtx.UnLock();

// Reset this object to handle the path
//
   Reset(nxtPath);

  } while(true);

// Do final reset as we finished processing
//
   Reset(0);

// We are done, so delete this object and return 
//
   int n = Config.maxStage++;  // schedMtx is still held
   schedMtx.UnLock();
   DEBUG("Staging queue empty; MaxStage="<<n+1);
   delete this;
}

/******************************************************************************/
/* Private:                     i s O n l i n e                               */
/******************************************************************************/
  
XrdOssArcStage::MssRC XrdOssArcStage::isOnline(const char* path)
{
   TraceInfo("isOnline",0);
   int rc, finrc;
 
   DEBUG("Running "<<Config.MssComPath<<" online "<<path);
   rc = Config.MssComProg->Run("online", path);

// Adjust return code. Note that XrdOucProg return -status!
//
   if (rc < -1 || rc > 1) finrc = -1;
      else finrc = -rc;
   DEBUG("MssComCmd returned "<<rc<<" -> "<<finrc);
          
   return static_cast<MssRC>(finrc);
}

/******************************************************************************/
/* Private:                        R e s e t                                  */
/******************************************************************************/

void XrdOssArcStage::Reset(const char* path)
{

// Remove ourselves from the active set if we still have a path
//
   if (arcvPath)
      {ActInfo aInfo(arcvPath);
       stageMtx.Lock();
       auto it = Active.find(&aInfo);
       if (it != Active.end())
          {ActInfo* aiP = *it;
           Active.erase(it);
           delete aiP;
          }
       stageMtx.UnLock();
      }

// Replace out path with the new path
//
   arcvPath = path;
}
  
/******************************************************************************/
/*                                 S t a g e                                  */
/******************************************************************************/

int XrdOssArcStage::Stage(const char *path)
{
   TraceInfo("Stage",0);
   ActInfo aInfo(path);

// Check if this is being staged
//
   stageMtx.Lock();
   auto it = Active.find(&aInfo);
   if (it != Active.end())
      {int rc;
       if ((*it)->rc == 0) rc = EINPROGRESS;
          else {rc = (*it)->rc;
                ActInfo* aiP = *it;
                Active.erase(it);
                delete aiP;
               }
       stageMtx.UnLock();
       return rc;
      } else stageMtx.UnLock();

// Make sure the path exists and is actually online
//
   MssRC mssRC = isOnline(path);
   switch(mssRC)
         {case isFalse: break;
          case isTrue:  return EEXIST; break;
          default:      return EINVAL; break;
         }

// Create a an action information object. This will copy the path and we
// can use the copy in other places as the pointer i
//
   ActInfo* stageInfo = new ActInfo(path, true);

// Add the path to the staging set. Another thread may have beat us to it.
//
   stageMtx.Lock();
   auto iResult = Active.insert(stageInfo);
   stageMtx.UnLock();
   if (!iResult.second)
      {delete stageInfo;
       return EINPROGRESS;
      }

// Schedule this staging request if we are allowed to do so
//
   int smx;
   schedMtx.Lock();
   if (Config.maxStage)
      {smx = Config.maxStage--; 
       schedMtx.UnLock();
       XrdOssArcStage *asP = new XrdOssArcStage(stageInfo->path);
       schedP->Schedule((XrdJob*)asP);
       return EINPROGRESS;
      } else smx = 0;

// Too many things being staged, so queue this request
//
   Pending.push(stageInfo->path);
   schedMtx.UnLock();

// Do some debugging
//
   DEBUG("MaxStage="<<smx<<" staging '"<<path<<(smx?"' scheduled":"' queued"));

// All done
//
   return EINPROGRESS;
}

/******************************************************************************/
/* Private:                   S t a g e E r r o r                             */
/******************************************************************************/
  
void XrdOssArcStage::StageError(int rc, const char* what, const char* path)
{
   ActInfo aInfo(arcvPath);

// Flag this request as failed
//
   stageMtx.Lock();
   auto it = Active.find(&aInfo);
   stageMtx.UnLock();
   if (it != Active.end()) (*it)->rc = rc;
   
// Issue error message
//
   Elog.Emsg("Stage", rc, what, path);

// We now must clear our arcvPath to prevent removal from the active set
//
   arcvPath = 0;
}
