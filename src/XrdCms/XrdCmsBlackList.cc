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
  
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Xrd/XrdScheduler.hh"

#include "XrdCms/XrdCmsBlackList.hh"
#include "XrdCms/XrdCmsCluster.hh"
#include "XrdCms/XrdCmsTrace.hh"
#include "XrdCms/XrdCmsUtils.hh"

#include "XrdNet/XrdNetAddr.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTokenizer.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class BL_Grip
     {public:
      void Add(XrdOucTList *tP)
              {if (last) last->next  = tP;
                  else         first = tP;
               last = tP;
              }

      XrdOucTList **Array(int &anum)
              {XrdOucTList *tP = first;
               anum = Count();
               if (!anum) return 0;
               XrdOucTList **vec = new XrdOucTList *[anum];
               for (int i = 0; i < anum; i++) {vec[i] = tP; tP = tP->next;}
               first = last = 0;
               return vec;
              }

      int          Count()
                         {XrdOucTList *tP = first;
                          int n = 0;
                          while(tP) {tP=tP->next; n++;}
                          return n;
                         }

      XrdOucTList *Export()
                         {XrdOucTList *tP = first;
                          first = last = 0;
                          return tP;
                         }

      bool         Include(const char *item, int &i)
                         {XrdOucTList *tP = first;
                          i = 0;
                          while(tP && strcmp(item,tP->text)) {tP=tP->next; i++;}
                          if (!tP) {Add(new XrdOucTList(item)); return false;}
                          return true;
                         }

           BL_Grip()  :  first(0), last(0) {}

          ~BL_Grip()     {XrdOucTList *tP;
                          while((tP = first)) {first = tP->next; delete tP;}
                          last = 0;
                         }

      private:
      XrdOucTList *first;
      XrdOucTList *last;
     };

union  BL_Info
      {long long info;
       struct   {short flags;
                 short pfxLen;
                 short sfxLen;
                 short totLen;
                } v;
       enum {exact = 0x8000,
             redir = 0x4000,
             rmask = 0x00ff
            };
      };

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

   XrdCmsBlackList  BlackList;

   XrdSysMutex      blMutex;

   XrdOucTList     *blReal = 0;
   XrdOucTList    **blRedr = 0;

   char            *blFN;

   time_t           blTime = 0;

   int              blChk;
   int              blRcnt = 0;
   bool             isWList=false;

   extern XrdSysError   Say;
};

using namespace XrdCms;

/******************************************************************************/
/* Private:                        A d d B L                                  */
/******************************************************************************/
  
bool XrdCmsBlackList::AddBL(BL_Grip &bAnchor, char *hSpec,
                            BL_Grip *rAnchor, char *rSpec)
{
   const char  *bwTag = (isWList ? "whitelist '" : "blacklist '");
   XrdNetAddr   blAddr;
   XrdOucTList *bP;
   BL_Info      Hdr;
   const char  *eText;
   char *Ast, blBuff[512];

// Initialize the header
//
   Hdr.info = 0;

// Check if we are processing a redirect
//
   if (rSpec)
      {int i = AddRD(rAnchor, rSpec, hSpec);
       if (i < 0) return false;
       Hdr.v.flags |= BL_Info::redir | i;
      }

// Get the full name if this is not generic
//
   if (!(Ast = index(hSpec, '*')))
      {if ((eText = blAddr.Set(hSpec,0)))
          {snprintf(blBuff, sizeof(blBuff), "'; %s", eText);
           Say.Say("Config ", "Unable to ", bwTag, hSpec, blBuff);
           return false;
          }
       blAddr.Format(blBuff, sizeof(blBuff), XrdNetAddrInfo::fmtName,
                                             XrdNetAddrInfo::noPort);
       hSpec = blBuff;
       Hdr.v.flags |= BL_Info::exact;
      } else {
       Hdr.v.pfxLen = Ast - hSpec;
       Hdr.v.sfxLen = strlen(hSpec + Hdr.v.pfxLen + 1);
       Hdr.v.totLen = Hdr.v.pfxLen + Hdr.v.sfxLen;
      }

// Add specification to the list
//
   bP = new XrdOucTList(hSpec, &Hdr.info);
   bAnchor.Add(bP);
   return true;
}

/******************************************************************************/
/*                                 A d d R D                                  */
/******************************************************************************/

int  XrdCmsBlackList::AddRD(BL_Grip *rAnchor, char *rSpec, char *hSpec)
{
   XrdOucTList *rP, *rList = 0;
   char *rTarg;
   int   ival;
   bool  aOK = true;

// First see if we have this entry already
//
   if (rAnchor[0].Include(rSpec, ival)) return ival;

// Make sure we did not exceed the maximum number of redirect entries
//
   if (ival > BL_Info::rmask)
      {Say.Say("Config ", "Too many different redirects at ", hSpec,
                           "redirect", rSpec);
       return -1;
      }

// We now ned to tokenize the specification
//
   XrdOucTokenizer rToks(rSpec);
   rToks.GetLine();

// Process each item
//
   while((rTarg = rToks.GetToken()) && *rTarg) aOK &= AddRD(&rList,rTarg,hSpec);
   if (!aOK) return -1;

// Flatten the list and add it to out list of redirect targets
//
   rP = Flatten(rList, rList->val);
   rAnchor[1].Add(rP);

// Delete the rlist
//
   while((rP = rList)) {rList = rList->next; delete rP;}

// All done
//
   return ival;
}
  
/******************************************************************************/

bool XrdCmsBlackList::AddRD(XrdOucTList **rList, char *rSpec, char *hSpec)
{
   char *rPort;

// Screen out IPV6 specifications
//
   if (*rSpec == '[')
      {if (!(rPort = index(rSpec, ']')))
          {Say.Say("Config ","Invalid ",hSpec," redirect specification - ",rSpec);
           return -1;
          }
      } else rPort = rSpec;

// Grab the port number
//
   if ((rPort = index(rPort, ':')))
      {if (!(*(rPort+1))) rPort = 0;
          else *rPort++ = '\0';
      }

// We should have a port specification now
//
   if (!rPort) {Say.Say("Config ", "redirect port not specified for ", hSpec);
                return -1;
               }

// Convert this to a list of redirect targets
//
   return XrdCmsUtils::ParseMan(&Say, rList, rSpec, rPort, 0, true);
}
  
/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
  
void XrdCmsBlackList::DoIt()
{
   struct stat Stat;
   XrdOucTList **blOldRedr = 0, **blNewRedr = 0, *blNewReal = 0, *tP = 0, *nP;
   int rc, blOldRcnt = 0, blNewRcnt;
   bool doUpdt = false, doPrt = false;

// Check if the black list file was modified
//
   rc = stat(blFN, &Stat);
   if ((!rc && blTime != Stat.st_mtime) || (rc && blTime && errno == ENOENT))
      {blTime = (rc ? 0 : Stat.st_mtime);
       if (GetBL(blNewReal, blNewRedr, blNewRcnt))
          {blMutex.Lock();
           tP        = blReal; blReal = blNewReal;
           blOldRedr = blRedr; blRedr = blNewRedr;
           blOldRcnt = blRcnt; blRcnt = blNewRcnt;
           blMutex.UnLock();
           if (!blReal && tP) doPrt = !isWList;
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

// Delete the old redirect array
//
   if (blOldRedr)
      {for (int i = 0; i < blOldRcnt; i++) delete blOldRedr[i];
       delete [] blOldRedr;
      }

// Do real-time update if need be
//
   if (doUpdt) blCluster->BlackList(blReal);

// Reschedule this to check any modifications
//
   blSched->Schedule((XrdJob *)&BlackList, time(0) + blChk);
}

/******************************************************************************/
/* Private:                      F l a t t e n                                */
/******************************************************************************/
  
XrdOucTList *XrdCmsBlackList::Flatten(XrdOucTList *tList, int tPort)
{
   XrdOucTList *tP = tList;
   char buff[4096], bPort[8], *bP = buff;
   int n, pLen, bleft = sizeof(buff);
   short xdata[4] = {0};

// Convert port to a suffix
//
   pLen = sprintf(bPort, ":%d", tPort);
   *buff = 0;

// Fill the buffer and truncate as necessary
//
   while(tP)
        {n = strlen(tP->text)+pLen+2;
         if (n >= bleft) break;
         n = sprintf(bP, " %s%s", tP->text, bPort);
         bP += n; bleft -= n;
         tP = tP->next;
        }

// Get actual length including null byte
//
   xdata[0] = strlen(buff+1) + 1;
   xdata[1] = xdata[0] + sizeof(short);
   xdata[2] = htons(xdata[0]);

// Create a new tlist item
//
   tP = new XrdOucTList(buff+1, xdata);
   return tP;
}

/******************************************************************************/
/* Private:                        G e t B L                                  */
/******************************************************************************/
  
bool XrdCmsBlackList::GetBL(XrdOucTList  *&bList,
                            XrdOucTList **&rList, int &rcnt)
{
   static int msgCnt = 0;
   XrdOucEnv myEnv;
   XrdOucStream blFile(&Say, getenv("XRDINSTANCE"), &myEnv, "=====> ");
   BL_Grip bAnchor, rAnchor[2];
   const char *fType, *oEmsg, *rEmsg;
   char *hsp, *rsp, hspBuff[512], rSpec[4096];
   int blFD, retc;
   bool aOK = true;

// Setup message plugins
//
   if (isWList)
      {oEmsg = "open whitelist file";
       rEmsg = "read whitelist file";
       fType = "whitelist";
      } else {
       oEmsg = "open blacklist file";
       rEmsg = "read blacklist file";
       fType = "blacklist";
      }

// Open the config file
//
   if ( (blFD = open(blFN, O_RDONLY, 0)) < 0)
      {if (errno == ENOENT) return true;
       if (!(msgCnt & 0x03)) Say.Emsg("GetBL", errno, oEmsg, blFN);
       return false;
      }
   blFile.Attach(blFD, 4096);

// Trace this now
//
   Say.Say("Config processing ", fType, " file ", blFN);

// Start reading the black list
//
   while((hsp = blFile.GetMyFirstWord()))
        {if (strlen(hsp) >= sizeof(hspBuff))
            {Say.Say("Config ", hsp, " is too long."); aOK = false; continue;}
         strcpy(hspBuff, hsp); hsp = hspBuff;
         if ( (rsp = blFile.GetWord()) && *rsp)
            {if (strcmp("redirect", rsp))
                {Say.Say("Config ", rsp, " is an invalid modifier for ", hsp);
                 aOK = false;
                 continue;
                }
             *rSpec = 0; rsp = rSpec;
             if (!blFile.GetRest(rSpec, sizeof(rSpec)))
                {Say.Say("Config ", "redirect target too long ", hsp);
                 aOK = false;
                 continue;
                }
             if (!(*rSpec))
                {Say.Say("Config ", "redirect target missing for ", hsp);
                 aOK = false;
                 continue;
                }
            } else rsp = 0;
         blFile.noEcho();
         if (!AddBL(bAnchor, hsp, rAnchor, rsp)) aOK = false;
        }

// Now check if any errors occurred during file i/o
//
   if ((retc = blFile.LastError()))
      {Say.Emsg("GetBL", retc, rEmsg, blFN); aOK = false;}
      else if (!aOK) Say.Emsg("GetBL", "Error(s) encountered in",fType,"file!");

// Return ending status
//
   blFile.Close();
   bList = (aOK ? bAnchor.Export() : 0);
   rList = rAnchor[1].Array(rcnt);
   return aOK;
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

// Determine if this is a black or white list
//
   if (chkt < 0) {isWList = true; chkt = -chkt;}

// Copy the file path (this is a one time call during config)
//
        if (blfn) blFN = strdup(blfn);
   else if (!(cfn = getenv("XRDCONFIGFN"))) return;
   else {char pBuff[2048], *Slash;
         strcpy(pBuff, cfn);
         if (!(Slash = rindex(pBuff, '/'))) return;
         strcpy(Slash+1, (isWList ? "cms.whitelist" : "cms.blacklist"));
         blFN = strdup(pBuff);
        }

// Check if the black list file exists, it might not. If it does, process it
//
   if (!stat(blFN, &Stat))
      {blTime = Stat.st_mtime;
       GetBL(blReal, blRedr, blRcnt);
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
  
int  XrdCmsBlackList::Present(const char *hName, XrdOucTList *bList,
                                    char *rBuff, int rBLen)
{
   BL_Info Hdr;
   int     hLen, retval;
   bool    doUnLk;

// Check if we really have a name here
//
   if (!hName || !blSched) return 0;

// Check if we need to supply our list
//
   if (bList) doUnLk = false;
      else   {doUnLk = true;
              blMutex.Lock();
              bList  = blReal;
             }

// By definition, if there is no list at all then everybody is allowed
//
   if (!bList)
      {if (doUnLk) blMutex.UnLock();
       return 0;
      }

// Run through the list and try to compare
//
   hLen = strlen(hName);
   while(bList)
        {Hdr.info = bList->dval;
              if (Hdr.v.flags & BL_Info::exact)
                 {if (!strcmp(hName, bList->text)) break;}
         else if (hLen >= Hdr.v.totLen)
                 {if (!Hdr.v.pfxLen
                  ||  !strncmp(bList->text, hName, Hdr.v.pfxLen))
                     {if (!Hdr.v.sfxLen
                      || !strncmp(bList->text+Hdr.v.pfxLen+1,
                                  hName + (hLen - Hdr.v.sfxLen),
                                  Hdr.v.sfxLen)) break;
                     }
                 }
         bList = bList->next;
        }

// If we have a black list check if we should redirect
//
   if (bList)
      {if (!(Hdr.v.flags & BL_Info::redir)) retval = (isWList ? 0 : -1);
          else {XrdOucTList *rP = blRedr[Hdr.v.flags & BL_Info::rmask];
                if (rP)
                   {retval = rP->sval[1];
                    if (!rBuff || retval > rBLen) retval = -retval;
                       else {memcpy(rBuff, &(rP->sval[2]), sizeof(short));
                             memcpy(rBuff+sizeof(short), rP->text, rP->sval[0]);
                            }
                    } else retval = -1;
               }
      } else retval = (isWList ? -1 : 0);

// Unlock ourselves if need be and return result
//
   if (doUnLk) blMutex.UnLock();
   return retval;
}

/******************************************************************************/
/*                                  R i n g                                   */
/******************************************************************************/
  
void MidNightTask::Ring()
{
   BL_Info Hdr;
   XrdOucTList *tP;
   const char *bwTag = (isWList ? "Whitelisting " : "Blacklisting ");

// Get the list lock
//
   blMutex.Lock();
   tP = blReal;

// Print the list
//
   while(tP)
        {Hdr.info = tP->dval;
         if (!(Hdr.v.flags & BL_Info::redir))
            Say.Say("Config ", bwTag, tP->text);
            else {XrdOucTList *rP = blRedr[Hdr.v.flags & BL_Info::rmask];
                  Say.Say("Config Blacklisting ",tP->text," redirect ",rP->text);
                 }
         tP = tP->next;
        }

// All done
//
   blMutex.UnLock();
}
