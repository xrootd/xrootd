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
#include "XrdOss/XrdOss.hh"
#include "XrdOssArc/XrdOssArcConfig.hh"
#include "XrdOssArc/XrdOssArcStage.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdOssArcGlobals
{
extern XrdOss*         ossP;

extern XrdScheduler*   schedP;

extern XrdSysError     Elog;

extern XrdOssArcConfig Config;

XrdSysMutex schedMtx;
XrdSysMutex stageMtx;

bool cmpLess(const char* a, const char* b) {return strcmp(a, b) < 0;}

std::set<const char*, decltype(&cmpLess)> Active(&cmpLess);

std::queue<char*> Pending;
}
using namespace XrdOssArcGlobals;

/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/

void XrdOssArcStage::DoIt()
{
   XrdOucEnv stageEnv;
   char* nxtPath;
   int rc;

// Obtain a file object
//
   XrdOssDF* fileP = ossP->newFile("XrdOssArcStage");

// Open the file in read mode. This will force the file to come online
//
do{rc = fileP->Open(arcvPath, O_RDONLY, 0, stageEnv);

// Diagnose any errors or merely close the file
//
   if (rc == 0) rc = fileP->Close();
      else Elog.Emsg("XrdOssArcStage", rc, "open/stage file", arcvPath);

// Perform any required cleanup
//
   if (rc)
      {delete fileP;
       fileP = 0;
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

// Allocate a new file object if we deleted the previous one
//
   if (!fileP) fileP = ossP->newFile("XrdOssArcStage");
  } while(true);

// We are done, so delete this object and return 
//
   Config.maxStage++;  // schedMtx is still held
   schedMtx.UnLock();
   delete this;
}

/******************************************************************************/
/* Private:                     i s O n l i n e                               */
/******************************************************************************/
  
XrdOssArcStage::MssRC XrdOssArcStage::isOnline(const char* path)
{
   int rc;
 
   rc = Config.MssComProg->Run("online", path);
   if (rc != 0 && rc != 1) rc = -1;
   return static_cast<MssRC>(rc);
}

/******************************************************************************/
/* Private:                        R e s e t                                  */
/******************************************************************************/

void XrdOssArcStage::Reset(char* path)
{

// Remove ourselves from the staging set
//
   stageMtx.Lock();
   Active.erase(arcvPath);
   stageMtx.UnLock();

// Free the current path and replace it with the new path
//
   free(arcvPath);
   arcvPath = path;
}
  
/******************************************************************************/
/*                                 S t a g e                                  */
/******************************************************************************/
  
int XrdOssArcStage::Stage(const char *path, const char* member)
{

// Check if this is being staged
//
   stageMtx.Lock();
   auto it = Active.find(path);
   stageMtx.UnLock();
   if (it != Active.end()) return EINPROGRESS;

// Make sure the path is actually online
//
   MssRC mssRC = isOnline(path);
   switch(mssRC)
         {case isFalse: break;
          case isTrue:  return EEXIST; break;
          default:      return EINVAL; break;
         }

// Create a copy of the const path
//
   char* stagePath = strdup(path);

// Add the path to the staging set. Another thread may have beat us to it.
//
   stageMtx.Lock();
   auto iResult = Active.insert(stagePath);
   stageMtx.UnLock();
   if (!iResult.second)
      {free(stagePath);
       return EINPROGRESS;
      }

// Schedule this staging request if are allowed to do so
//
   schedMtx.Lock();
   if (Config.maxStage)
      {Config.maxStage--;
       schedMtx.UnLock();
       XrdOssArcStage *asP = new XrdOssArcStage(stagePath);
       schedP->Schedule((XrdJob*)asP);
       return EINPROGRESS;
      }

// Too manny things being staged, so queue this request
//
   Pending.push(stagePath);
   schedMtx.UnLock();

// All done
//
   return EINPROGRESS;
}
