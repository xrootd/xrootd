/******************************************************************************/
/*                                                                            */
/*                    X r d C m s B l a c k L i s t . c c                     */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Xrd/XrdScheduler.hh"

#include "XrdCms/XrdCmsBlackList.hh"
#include "XrdCms/XrdCmsCluster.hh"
#include "XrdCms/XrdCmsTrace.hh"

#include "XrdNet/XrdNetAddr.hh"

#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucStream.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdCms
{
   XrdScheduler    *blSched = 0;

   XrdCmsCluster   *blCluster;

   class MidNightTask : public XrdSysLogger::Task
        {public:
         void    Ring();
                 MidNightTask() {}
                ~MidNightTask() {}
        };
   MidNightTask     blMN;

   XrdSysMutex      blMutex;


   XrdCmsBlackList  BlackList;

   XrdOucTList     *blReal;

   char            *blFN;

   time_t           blTime;

   int              blChk;

   extern XrdSysError   Say;
};

using namespace XrdCms;

/******************************************************************************/
/* Private:                        A d d B L                                  */
/******************************************************************************/
  
void XrdCmsBlackList::AddBL(XrdOucTList *&bAnchor, char *hSpec)
{
   XrdNetAddr   blAddr;
   const char  *eText;
   char *Ast, blBuff[512];
   short lrLen[4] = {1, 0, 0, 0};

// Get the full name if this is not generic
//
   if (!(Ast = index(hSpec, '*')))
      {if ((eText = blAddr.Set(hSpec,0)))
          {snprintf(blBuff, sizeof(blBuff), "'; %s", eText);
           Say.Emsg("AddBL", "Unable to blacklist '", hSpec, blBuff);
           return;
          }
       blAddr.Format(blBuff, sizeof(blBuff), XrdNetAddrInfo::fmtName,
                                             XrdNetAddrInfo::noPort);
       hSpec = blBuff;
      } else {
       lrLen[0] = 0;
       lrLen[1] = Ast - hSpec;
       lrLen[2] = strlen(hSpec+lrLen[1]+1);
       lrLen[3] = lrLen[1] + lrLen[2];
      }

// Add specification to the list
//
   bAnchor = new XrdOucTList(hSpec, lrLen, bAnchor);
}

/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
  
void XrdCmsBlackList::DoIt()
{
   struct stat Stat;
   XrdOucTList *blTemp = 0, *tP = 0, *nP;
   int rc;
   bool doUpdt = false, doPrt = false;

// Check if the black list file was modified
//
   rc = stat(blFN, &Stat);
   if ((!rc && blTime != Stat.st_mtime) || (rc && blTime && errno == ENOENT))
      {blTime = (rc ? 0 : Stat.st_mtime);
       if (!GetBL(blTemp)) tP = blTemp;
          else {tP = blReal;
                blMutex.Lock(); blReal = blTemp; blMutex.UnLock();
                if (!blTemp && tP) doPrt = true;
                   else blMN.Ring();
                doUpdt = true;
               }
      }

// Delete any list we need to release
//
   while(tP)
        {if (doPrt) Say.Say("Config ", tP->text, " removed from blacklist.");
         nP = tP->next; delete tP; tP = nP;
        }

// Do real-time update if need be
//
   if (doUpdt) blCluster->BlackList(blReal);

// Reschedule this to check any modifications
//
   blSched->Schedule((XrdJob *)&BlackList, time(0) + blChk);
}

/******************************************************************************/
/* Private:                        G e t B L                                  */
/******************************************************************************/
  
bool XrdCmsBlackList::GetBL(XrdOucTList *&bAnchor)
{
   static int msgCnt = 0;
   XrdOucStream blFile;
   char *var;
   int blFD, retc;

// Open the config file
//
   if ( (blFD = open(blFN, O_RDONLY, 0)) < 0)
      {if (errno == ENOENT) return true;
       if (!(msgCnt & 0x03))
          Say.Emsg("GetBL", errno, "open blacklist file", blFN);
       return false;
      }
   blFile.Attach(blFD);

// Trace this now
//
   Say.Say("Config processing blacklist file ", blFN);

// Start reading the black list
//
   while((var = blFile.GetLine()))
        {if (!(var = blFile.GetToken()) || !(*var) || *var == '#') continue;
         AddBL(bAnchor, var);
        }

// Now check if any errors occured during file i/o
//
   if ((retc = blFile.LastError()))
      Say.Emsg("GetBL", retc, "read blacklist file ", blFN);

// Return ending status
//
   blFile.Close();
   return retc == 0;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
void XrdCmsBlackList::Init(XrdScheduler *sP,   XrdCmsCluster *cP,
                           const char   *blfn, int chkt)
{
   struct stat Stat;
   const char *cfn;

// Copy out the scheduler and cluster pointers
//
   blSched   = sP;
   blCluster = cP;

// Copy the file path (this is a one time call during config)
//
        if (blfn) blFN = strdup(blfn);
   else if (!(cfn = getenv("XRDCONFIGFN"))) return;
   else {char pBuff[2048], *Slash;
         strcpy(pBuff, cfn);
         if (!(Slash = rindex(pBuff, '/'))) return;
         strcpy(Slash+1, "cms.blacklist");
         blFN = strdup(pBuff);
        }

// Check if the black list file exists, it might not. If it does, process it
//
   if (!stat(blFN, &Stat))
      {blTime = Stat.st_mtime;
       GetBL(blReal);
       if (blReal) blMN.Ring();
      }

// Schedule this to recheck any modifications
//
   blChk = chkt;
   blSched->Schedule((XrdJob *)&BlackList, time(0) + chkt);

// Add ourselves to the midnight run list
//
   Say.logger()->AtMidnight(&blMN);
}

/******************************************************************************/
/*                               P r e s e n t                                */
/******************************************************************************/
  
bool XrdCmsBlackList::Present(const char *hName, XrdOucTList *bList)
{
   bool doUnLk;
   int  hLen;

// Check if we really have a name here
//
   if (!hName || !blSched) return false;

// Check if we need to supply our list
//
   if (bList) doUnLk = false;
      else   {doUnLk = true;
              blMutex.Lock();
              bList  = blReal;
             }

// Run through the list and try to compare
//
   hLen = strlen(hName);
   while(bList)
        {     if (bList->sval[0]) {if (!strcmp(hName, bList->text)) break;}
         else if (hLen >= bList->sval[3])
                 {if (!bList->sval[1]
                  ||  !strncmp(bList->text, hName, bList->sval[1]))
                     {if (!bList->sval[2]
                      || !strncmp(bList->text+bList->sval[1]+1,
                                  hName + (hLen - bList->sval[2]),
                                  bList->sval[2])) break;
                     }
                 }
         bList = bList->next;
        }

// Unlock ourselves if need be and return result
//
   if (doUnLk) blMutex.UnLock();
   return bList != 0;
}

/******************************************************************************/
/*                                  R i n g                                   */
/******************************************************************************/
  
void MidNightTask::Ring()
{
   XrdOucTList *tP;

// Get the list lock
//
   blMutex.Lock();
   tP = blReal;

// Print the list
//
   while(tP)
        {Say.Say("Config Blacklisting ", tP->text);
         tP = tP->next;
        }

// All done
//
   blMutex.UnLock();
}
