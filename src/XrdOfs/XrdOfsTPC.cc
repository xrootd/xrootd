/******************************************************************************/
/*                                                                            */
/*                          X r d O f s T P C . c c                           */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <strings.h>
  
#include "XrdAcc/XrdAccAccess.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdOuc/XrdOucPList.hh"
#include "XrdOfs/XrdOfsSecurity.hh"
#include "XrdOfs/XrdOfsStats.hh"
#include "XrdOfs/XrdOfsTPC.hh"
#include "XrdOfs/XrdOfsTPCAuth.hh"
#include "XrdOfs/XrdOfsTPCConfig.hh"
#include "XrdOfs/XrdOfsTPCJob.hh"
#include "XrdOfs/XrdOfsTPCProg.hh"
#include "XrdOfs/XrdOfsTrace.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucCallBack.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucNList.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucTPC.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysTimer.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
extern XrdSysError  OfsEroute;
extern XrdOfsStats  OfsStats;
extern XrdSysTrace  OfsTrace;
extern XrdOss      *XrdOfsOss;

namespace XrdOfsTPCParms
{
static const int   fcMax = 8;

struct fcTb {char *aVar;
             char  aProt[XrdSecPROTOIDSIZE];
             bool  aOpt;
             bool  aGSI;
             }     fcAuth[fcMax];

const char        *gsiPKH   = "-----BEGIN PRIVATE KEY-----\n";

int                fcNum    = 0;
bool               tpcOK    = false;
bool               encTPC   = false;

XrdOfsTPCConfig    Cfg;
}

using namespace XrdOfsTPCParms;

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
/******************************************************************************/
/*                        X r d O f s T P C A l l o w                         */
/******************************************************************************/
  
class XrdOfsTPCAllow
{
public:

XrdOfsTPCAllow *Next;
char           *theDN;
char           *theGN;
XrdOucNList    *theHN;
char           *theVO;

int             Match(const XrdSecEntity *Who, const char *Host);

                XrdOfsTPCAllow(char *vDN, char *vGN, char *vHN, char *vVO,
                               XrdOfsTPCAllow *Prev)
                              : Next(Prev), theDN(vDN), theGN(vGN), theVO(vVO)
                              {if (vHN) theHN = new XrdOucNList(vHN);
                                  else  theHN = 0;
                              }
               ~XrdOfsTPCAllow() {if (theHN) delete theHN;}
};

/******************************************************************************/
/*                 X r d O f s T P C A l l o w : : M a t c h                  */
/******************************************************************************/
  
int XrdOfsTPCAllow::Match(const XrdSecEntity *Who, const char *Host)
{
// Host name comparisons should be case insensitive. However, DN's and VO's
// do take case into account.
//
   if (theHN && (!Host        || !(theHN->NameKO(Host  )))) return 0;
   if (theDN && (!(Who->name) || strcmp(theDN, Who->name))) return 0;
   if (theVO && (!(Who->vorg) || strcmp(theDN, Who->vorg))) return 0;
   if (!theGN) return 1;
   if (Who->grps)
      {char gBuff[1028], Group[64];
       strlcpy(gBuff+1, Who->grps, sizeof(gBuff)-1); *gBuff = ' ';
       strlcpy(Group+1, theGN, sizeof(Group)-1);     *Group = ' ';
       return strstr(gBuff, Group) != 0;
      } else return 0;
   return 1;
}

/******************************************************************************/
/*                      S t a t i c   V a r i a b l e s                       */
/******************************************************************************/
  
XrdOucTList       *XrdOfsTPC::AuthDst  = 0;
XrdOucTList       *XrdOfsTPC::AuthOrg  = 0;

XrdOucPListAnchor *XrdOfsTPC::RPList;

XrdOfsTPCAllow    *XrdOfsTPC::ALList   = 0;

XrdAccAuthorize   *XrdOfsTPC::fsAuth   = 0;

char              *XrdOfsTPC::cPath    = 0;

namespace
{
atomic_int rpInst = {0};
}
  
/******************************************************************************/
/*                               A d d A u t h                                */
/******************************************************************************/

const char *XrdOfsTPC::AddAuth(const char *auth, const char *avar)
{
   bool aOpt, aGSI;

// Check if credentials are optional
//
   if (*auth != '?') aOpt = false;
      else {aOpt = true;
            auth++;
           }
   aGSI = strcmp("gsi", auth) == 0;

// Verify that the authname is not too long
//
   if (strlen(auth) >= XrdSecPROTOIDSIZE) return "Invalid auth";

// Check if auth is already in the table
//
   for (int i = 0; i < fcNum; i++)
       if (!strcmp(auth, fcAuth[i].aProt))
          {if (fcAuth[i].aVar) free(fcAuth[i].aVar);
           fcAuth[i].aVar = strdup(avar);
           fcAuth[i].aOpt = aOpt;
           fcAuth[i].aGSI = aGSI;
           return 0;
          }

// Check if we have room to add an auth
//
   if (fcNum >= fcMax) return "Too many fcred auths";

// Add an auth
//
   strcpy(fcAuth[fcNum].aProt, auth);
   fcAuth[fcNum].aVar = strdup(avar);
   fcAuth[fcNum].aOpt = aOpt;
   fcAuth[fcNum].aGSI = aGSI;
   fcNum++;
   return 0;
}

/******************************************************************************/
/*                                 A l l o w                                  */
/******************************************************************************/
  
void XrdOfsTPC::Allow(char *vDN, char *vGN, char *vHN, char *vVO)
{

// Add the entry
//
   ALList = new XrdOfsTPCAllow(vDN, vGN, vHN, vVO, ALList);
}

/******************************************************************************/
/*                             A u t h o r i z e                              */
/******************************************************************************/
  
int XrdOfsTPC::Authorize(XrdOfsTPC        **pTPC,
                         XrdOfsTPC::Facts  &Args,
                               int          isPLE)
{
   XrdOfsTPCAuth *myTPC;
   const char *dstHost;
   int rc, NoGo = 0;

// Determine if we can handle any TPC requests
//
   if (!tpcOK || !Args.Usr)
      return Fatal(Args, "tpc not supported", ENOTSUP);

// If we are restricting paths, make sure this meets the restriction
//
   if (RPList && !(RPList->Find(Args.Lfn)))
      return Fatal(Args, "tpc not allowed for path", EACCES);

// The origin and the destination in the arguments
//
   Args.Org = Args.Env->Get(XrdOucTPC::tpcOrg);
   Args.Dst = Args.Env->Get(XrdOucTPC::tpcDst);

// Determine if this is the origin or the destination.
// Origin: dst and key required but org may not be specified
// Dest:   org and key required but dst may not be specified
//
   if (Args.Dst && !Args.Org)
      {if (fsAuth && !fsAuth->Access(Args.Usr, Args.Lfn, AOP_Read, Args.Env))
          return Fatal(Args, "permission denied", EACCES);
       if (AuthOrg && !Screen(Args, AuthOrg, isPLE)) return SFS_ERROR;
       if (!(myTPC = new XrdOfsTPCAuth(getTTL(Args.Env))))
          return Fatal(Args, "insufficient memory", ENOMEM);
       if (!(myTPC->Add(Args))) {delete myTPC; return SFS_ERROR;}
       *pTPC = (XrdOfsTPC *)myTPC;
       return SFS_OK;
      }
      else if (!Args.Org || Args.Dst)
              return Fatal(Args, "conflicting tpc cgi", EINVAL);

// If we need to enforce authentication, do so now
//
   if (AuthDst && !Screen(Args, AuthDst, isPLE)) return SFS_ERROR;

// Avoid nodnr manglement of the host name, we always will need one. If we have
// see if we should restrict the destinations and if so, do it.
//
   if (!(dstHost = Args.Usr->addrInfo->Name())) NoGo = 1;
      else if (ALList)
              {XrdOfsTPCAllow *aP = ALList;
               while(aP && !aP->Match(Args.Usr, dstHost)) aP = aP->Next;
               if (!aP) NoGo = 1;
              }

// Check if this destination is actually authorized
//
   if (NoGo)
      {OfsEroute.Emsg("TPC", Args.eRR->getErrUser(),
                             "denied tpc access to", Args.Lfn);
       OfsStats.Add(OfsStats.Data.numTPCdeny);
       return Fatal(Args, "dest not authorized for tpc" ,EACCES, 1);
      }

// This is the destination trying to open a source file. We must make sure
// that the origin has authorized this action for this destination.
//
   Args.Dst = dstHost;
   if ((rc = XrdOfsTPCAuth::Get(Args, &myTPC))) return rc;

// Check if entry already expired
//
   if (myTPC->Expired())
      {myTPC->Expired(Args.Usr->tident);
       myTPC->Del();
       return Fatal(Args, "authorization expired", EACCES, 1);
      }

// Log the grant if so wanted
//
   if (Cfg.LogOK)
      {char Buff[1024];
       snprintf(Buff, sizeof(Buff), "%s granted tpc access by %s to",
                Args.Usr->tident, Args.Org);
       Buff[sizeof(Buff)-1] = 0;
       OfsEroute.Emsg("TPC", Buff, Args.Lfn);
      }

// All done
//
   OfsStats.Add(OfsStats.Data.numTPCgrant);
   *pTPC = (XrdOfsTPC *)myTPC;
   return SFS_OK;
}
  
/******************************************************************************/
/* Private:                        D e a t h                                  */
/******************************************************************************/
  
int XrdOfsTPC::Death(XrdOfsTPC::Facts &Args, const char *eMsg, int eCode, int nomsg)
{
// If automatc removal is wanted, remove the file.
//
   if (Cfg.autoRM && Args.Pfn) XrdOfsOss->Unlink(Args.Lfn);

// Return error information
//
   return Fatal(Args, eMsg, eCode, nomsg);
}
  
/******************************************************************************/
/* Private:                        F a t a l                                  */
/******************************************************************************/
  
int XrdOfsTPC::Fatal(XrdOfsTPC::Facts &Args, const char *eMsg, int eCode, int nomsg)
{
   char Buff[2048];

// Format the error message
//
    snprintf(Buff, sizeof(Buff), "Unable to open %s; %s", Args.Lfn, eMsg);

// Print it out if debugging is enabled
//
#ifndef NODEBUG
    if (!nomsg) OfsEroute.Emsg("TPC", Args.eRR->getErrUser(), Buff);
#endif

// Place the error message in the error object and return
//
   Args.eRR->setErrInfo(eCode, Buff);
   OfsStats.Add(OfsStats.Data.numTPCerrs);
   return SFS_ERROR;
}
  
/******************************************************************************/
/*                                g e n O r g                                 */
/******************************************************************************/
  
int XrdOfsTPC::genOrg(const XrdSecEntity *client, char *Buff, int Blen)
{
   const char *Colon, *cOrg = client->tident;
   char *Name;
   int n;

// Extract out the login name and pid
//
   if (!(Colon = index(cOrg,   ':'))) return 0;
   n = (Colon - cOrg);

// Expand out client's full name
//
   if (!(Name = Verify("origin", client->host, Buff, Blen))) return 0;

// Make sure this all fits
//
   if (((n + 1) + int(strlen(Name))) >= Blen)
      {strncpy(Buff, "origin ID too long", Blen);
       Buff[Blen-1] = 0;
       free(Name);
       return 0;
      }

// Construct the origin information
//
   strncpy(Buff, cOrg, n);
   Buff += n; *Buff++ = '@';
   strcpy(Buff, Name);
   free(Name);
   return 1;
}

/******************************************************************************/
/* Private:                       g e t T T L                                 */
/******************************************************************************/
  
int XrdOfsTPC::getTTL(XrdOucEnv *Env)
{
    const char *vTTL = Env->Get(XrdOucTPC::tpcTtl);

    if (vTTL)
       {char *ePtr;
        int   n;
        n = strtol(vTTL, &ePtr, 10);
        if (n < 0 || *ePtr) return Cfg.dflTTL;
        return (n > Cfg.maxTTL ? Cfg.maxTTL : n);
       }
    return Cfg.dflTTL;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
void XrdOfsTPC::Init()
{
   std::string aStr;

// Create credential forwarding template, if cred path specified. It is
// gauranteed to end with a slash (it better be).
//
   if (Cfg.cPath && Cfg.fCreds) cPath = Cfg.cPath;
      else cPath = 0;

// Check for streams option
//
   if (Cfg.tcpSTRM > 15) Cfg.tcpSTRM = 15;
   if (Cfg.tcpSMax > 15) Cfg.tcpSMax = 15;


// Record all delegated auths
//
   for (int i = 0; i < fcNum; i++)
       {aStr += ' '; aStr += fcAuth[i].aProt;}

// Export the delegated auths
//
   if (aStr.length())
      XrdOucEnv::Export("XRDTPCDLG", strdup(aStr.c_str()+1));
}

/******************************************************************************/
/*                               R e q u i r e                                */
/******************************************************************************/

void XrdOfsTPC::Require(const char *Auth, int rType)
{
   int n = strlen(Auth), doEnc = (Auth[n-1] == '+');

   if (!rType || rType == reqDST)
      {AuthDst = new XrdOucTList(Auth, doEnc, AuthDst);
       if (doEnc) AuthDst->text[n-1] = 0;
      }

   if (!rType || rType == reqORG)
      {AuthOrg = new XrdOucTList(Auth, doEnc, AuthOrg);
       if (doEnc) AuthOrg->text[n-1] = 0;
      }
   encTPC |= doEnc;
}
  
/******************************************************************************/
/*                              R e s t r i c t                               */
/******************************************************************************/

int XrdOfsTPC::Restrict(const char *Path)
{
   XrdOucPList *plp;

   char pBuff[MAXPATHLEN];
   int n = strlen(Path);

   if (n >= MAXPATHLEN)
      {OfsEroute.Emsg("Config", "tpc restrict path too long");
       return 0;
      }

   if (Path[n-1] != '/')
      {strcpy(pBuff, Path);
       pBuff[n++] = '/'; pBuff[n] = 0;
       Path = pBuff;
      }

   if (!RPList) RPList = new XrdOucPListAnchor;

   if (!(plp = RPList->Match(pBuff)))
      {plp = new XrdOucPList(pBuff);
       RPList->Insert(plp);
      }

   return 1;
}

/******************************************************************************/
/* Private:                       S c r e e n                                 */
/******************************************************************************/
  
int XrdOfsTPC::Screen(XrdOfsTPC::Facts &Args, XrdOucTList *tP, int wasEnc)
{
   const char *aProt = Args.Usr->prot;

   while(tP)
        {if (!strcmp(tP->text, aProt))
            {if (tP->val && wasEnc) return 1;
             Fatal(Args, "unencrypted tpc disallowed", EACCES);
             break;
            }
         tP = tP->next;
        }

   if (!tP) Fatal(Args, "improper tpc authentication", EACCES);

   OfsStats.Add(OfsStats.Data.numTPCdeny);
   return 0;
}

/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
int XrdOfsTPC::Start()
{

// If there is a path restriction list then setup it up
//
   if (RPList) RPList->Default(1);

// If there is no copy program then we use the default one
//
   if (!Cfg.XfrProg) Cfg.XfrProg = strdup("xrdcp --server");

// Allocate copy program objects
//
   if (!XrdOfsTPCProg::Init()) return 0;

// Start the expiration thread
//
   if (!XrdOfsTPCAuth::RunTTL(1)) return 0;

// All done
//
   XrdOucEnv::Export("XRDTPC", (encTPC ? "+1" : "1"));
   tpcOK = true;
   return 1;
}
  
/******************************************************************************/
/*                              V a l i d a t e                               */
/******************************************************************************/
  
int XrdOfsTPC::Validate(XrdOfsTPC **theTPC, XrdOfsTPC::Facts &Args)
{
   XrdOfsTPCJob *myTPC;
   const char *tpcLfn = Args.Env->Get(XrdOucTPC::tpcLfn);
   const char *tpcSrc = Args.Env->Get(XrdOucTPC::tpcSrc);
   const char *tpcCks = Args.Env->Get(XrdOucTPC::tpcCks);
   const char *tpcSgi = Args.Env->Get(XrdOucTPC::tpcSgi);
   const char *tpcStr = Args.Env->Get(XrdOucTPC::tpcStr);
   const char *tpcSpr = Args.Env->Get(XrdOucTPC::tpcSpr);
   const char *tpcTpr = Args.Env->Get(XrdOucTPC::tpcTpr);
   const char *theCGI, *enVar = 0;
         char  Buff[512], myURL[4096], sVal = 0;
         int   n, doRN = 0, myURLen = sizeof(myURL);
         short lfnLoc[2];

// Determine if we can handle any TPC requests
//
   if (!tpcOK || !Args.Usr) return Death(Args, "tpc not supported", ENOTSUP);

// If we will be forwarding credentials, then verify that we have some
//
   for (int i = 0; i < fcNum; i++)
       {if (!strcmp(Args.Usr->prot, fcAuth[i].aProt))
           {if (Args.Usr->creds == 0 || Args.Usr->credslen < 1
            || (fcAuth[i].aGSI && !strstr(Args.Usr->creds, gsiPKH)))
               {if (!fcAuth[i].aOpt)
                   return Death(Args,"no delegated credentials for tpc",EACCES);
               } else enVar  = fcAuth[i].aVar;
            const char *tpcDlg = Args.Env->Get(XrdOucTPC::tpcDlg);
            if (tpcDlg) tpcSrc = tpcDlg;
            break;
           }
       }

// This is a request by a writer to get data from another party. Make sure
// the source has been specified.
//
   if (!tpcSrc)   return Death(Args, "tpc source not specified", EINVAL);
   if (!Args.Pfn) return Death(Args, "tpc pfn not specified", EINVAL);

// If the lfn, if present, it must be absolute.
//
        if (!tpcLfn) tpcLfn = Args.Lfn;
   else if (Cfg.noids && *tpcLfn != '/')
           return Death(Args,"source lfn not absolute",EINVAL);
   else doRN = (strcmp(Args.Lfn, tpcLfn) != 0);

// Validate number of streams and adjust accordingly
//
   if (tpcStr)
      {char *eP;
       long nStrm = strtol(tpcStr, &eP, 10);
       if (nStrm < 0 || *eP)
          return Death(Args, "tpc streams value is invalid", EINVAL);
       if (nStrm > Cfg.tcpSMax) nStrm = Cfg.tcpSMax;
       sVal = static_cast<char>(nStrm);
      } else sVal = static_cast<char>(Cfg.tcpSTRM);

// Generate the origin id
//
   if (!enVar && !genOrg(Args.Usr, Buff, sizeof(Buff)))
      return Death(Args, Buff, EINVAL);

// Construct the source url (it may be very big)
//
   const char *xProt = (!tpcSpr || strcmp("xroots",tpcSpr) ? "xroot" : tpcSpr);
   n = snprintf(myURL, myURLen, "%s://%s/%s?", xProt, tpcSrc, tpcLfn);
   char *cgiP = myURL+n;
   int   cgiL = myURLen-n;
   if (cgiL < 3) return Death(Args, "url too long", EINVAL);

// Set lfn location in the URL but only if we need to do a rename
//
   if (doRN) {lfnLoc[1] = strlen(tpcLfn); lfnLoc[0] = n - lfnLoc[1];}
      else    lfnLoc[1] = lfnLoc[0] = 0;

// Copy user specified CGI into the source URL (omit tpc tokens)
//
   if (tpcSgi)
      {if ((int)strlen(tpcSgi) >= cgiL)
          return Death(Args, "url too long", EINVAL);
       n = XrdOucTPC::copyCGI(tpcSgi, cgiP, cgiL);
       cgiP += n;
       cgiL -= n;
      }

// Insert tpc tokens unless this is a delegated tpc which needs no tokens
//
   if (!enVar)
      {if (cgiL < 3) return Death(Args, "url too long", EINVAL);
       *cgiP++ = '&'; cgiL--; *cgiP = 0;
       theCGI = XrdOucTPC::cgiD2Src(Args.Key, Buff, cgiP, cgiL);
       if (*theCGI == '!') return Death(Args, theCGI+1, EINVAL);
      }

// Create a pseudo tpc object that will contain the information we need to
// actually peform this copy.
//
   if (!(myTPC = new XrdOfsTPCJob(myURL, Args.Usr->tident,
                                  Args.Lfn, Args.Pfn, tpcCks, lfnLoc,
                                  tpcSpr, tpcTpr)))
      return Death(Args, "insufficient memory", ENOMEM);

// Set credentials for the job if we need to
//
   if (enVar && Args.Usr->credslen > 0)
      myTPC->Info.SetCreds(enVar, Args.Usr->creds, Args.Usr->credslen);

// Setup reproxing if this is required
//
   if (Cfg.rPath)
      {int inst = rpInst++;
       char rpBuff[1024];
       snprintf(rpBuff, sizeof(rpBuff), Cfg.rPath, inst);
       myTPC->Info.SetRPath(rpBuff);
       Args.Env->Put("tpc.reproxy", rpBuff);
      }

// Set number of streams to use
//
   if (sVal > 0) myTPC->Info.SetStreams(sVal);

// All done
//
   myTPC->Info.isDest();
   *theTPC = (XrdOfsTPC *)myTPC;
   return SFS_OK;
}

/******************************************************************************/
/* Private:                       V e r i f y                                 */
/******************************************************************************/

char *XrdOfsTPC::Verify(const char *Who, const char *Name,
                              char *Buf,       int   Blen)
{
   XrdNetAddr vAddr;
   const char *etext, *Host;

// Obtain full host name and return it if successful
//
   if (!(etext = vAddr.Set(Name,0)) && (Host = vAddr.Name(0, &etext)))
      return strdup(Host);

// Generate error
//
   snprintf(Buf, Blen, "unable to verify %s %s (%s)", Who, Name, etext);
   Buf[Blen-1] = 0;
   return 0;
}
