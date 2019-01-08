/******************************************************************************/
/*                                                                            */
/*                     X r d C m s S e c u r i t y . c c                      */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "XrdVersion.hh"

#include "XProtocol/YProtocol.hh"

#include "Xrd/XrdLink.hh"

#include "XrdCms/XrdCmsSecurity.hh"
#include "XrdCms/XrdCmsTalk.hh"
#include "XrdCms/XrdCmsTrace.hh"
#include "XrdCms/XrdCmsVnId.hh"

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdSec/XrdSecLoadSecurity.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPthread.hh"

using namespace XrdCms;

/******************************************************************************/
/*                        S t a t i c   S y m b o l s                         */
/******************************************************************************/
  
namespace
{
static XrdSecGetProt_t getProtocol = 0;

static const int nidMax = 64;
}

XrdSecService *XrdCmsSecurity::DHS  = 0;

/******************************************************************************/
/*                          A u t h e n t i c a t e                           */
/******************************************************************************/
  
int XrdCmsSecurity::Authenticate(XrdLink *Link, const char *Token, int Toksz)
{
   CmsRRHdr myHdr = {0, kYR_xauth, 0, 0};
   XrdSecCredentials cred;
   XrdSecProtocol   *AuthProt = 0;
   XrdSecParameters *parm = 0;
   XrdOucErrInfo     eMsg;
   const char       *eText = 0;
   char *authName, authBuff[4096];
   int rc, myDlen, abLen = sizeof(authBuff);

// Send a request for authentication
//
   if ((eText = XrdCmsTalk::Request(Link, myHdr, (char *)Token, Toksz+1)))
      {Say.Emsg("Auth",Link->Host(),"authentication failed;",eText);
       return 0;
      }

// Perform standard authentication
//
do {

// Get the response header and verify the request code
//
   if ((eText = XrdCmsTalk::Attend(Link,myHdr,authBuff,abLen,myDlen))) break;
   if (myHdr.rrCode != kYR_xauth) {eText = "invalid auth response";    break;}
   cred.size = myDlen; cred.buffer = authBuff;

// If we do not yet have a protocol, get one
//
   if (!AuthProt)
      {if (!DHS || !(AuthProt=DHS->getProtocol(Link->Host(),
                                             *(Link->AddrInfo()),&cred,&eMsg)))
          {eText = eMsg.getErrText(rc); break;}
      }

// Perform the authentication
//
    AuthProt->Entity.addrInfo = Link->AddrInfo();
    if (!(rc = AuthProt->Authenticate(&cred, &parm, &eMsg))) break;
    if (rc < 0) {eText = eMsg.getErrText(rc); break;}
    if (parm) 
       {eText = XrdCmsTalk::Request(Link, myHdr, parm->buffer, parm->size);
        delete parm;
        if (eText) break;
       } else {eText = "auth interface violation"; break;}

} while(1);

// Check if we succeeded
//
   if (!eText)
      {if (!(authName = AuthProt->Entity.name)) eText = "entity name missing";
          else {Link->setID(authName,0);
                Say.Emsg("Auth",Link->Host(),"authenticated as", authName);
               }
      }

// Check if we failed
//
   if (eText) Say.Emsg("Auth",Link->Host(),"authentication failed;",eText);

// Perform final steps here
//
   if (AuthProt) AuthProt->Delete();
   return (eText == 0);
}

/******************************************************************************/
/*                               c h k V n I d                                */
/******************************************************************************/
  
char *XrdCmsSecurity::chkVnId(XrdSysError &eDest, const char *vnid,
                                                  const char *what)
{
   int n = strlen(vnid);

// Check if it is too long
//
   if (n > nidMax)
      {eDest.Emsg("Config", what, "a too long vnid -", vnid);
       return 0;
      }

// Check for a null vnid
//
   if (!n || !(*vnid))
      {eDest.Emsg("Config", what, "a null vnid.");
       return 0;
      }

// Make sure the vnid does not contain invalid characters
//
   const char *cP = vnid;
   while(*cP && (isalnum(*cP) || ispunct(*cP)) && *cP != '&' && *cP != ' ') cP++;
   if (*cP)
      {eDest.Emsg("Config", what, "an invalid vnid -", vnid);
       return 0;
      }

// All is well
//
   return strdup(vnid);
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/

int XrdCmsSecurity::Configure(const char *Lib, const char *Cfn)
{
   static XrdSysMutex myMutex;
   XrdSysMutexHelper  hlpMtx(&myMutex);

// If we aleady have a security interface, return (may happen in client)
//
   if (!Cfn && getProtocol) return 1;

// Get the server object and protocol creator
//
   if (!(DHS = XrdSecLoadSecService(&Say, Cfn, (strcmp(Lib,"default") ? Lib:0),
                                    &getProtocol)))
      {Say.Emsg("Config","Unable to create security service object via",Lib);
       return 0;
      }

// All done
//
   return 1;
}

/******************************************************************************/
/*                               g e t V n I d                                */
/******************************************************************************/
  
char *XrdCmsSecurity::getVnId(XrdSysError &eDest, const char *cfgFN,
                              const char *nidarg, const char *nidparm,
                                    char  nidType)
{
   static XrdVERSIONINFODEF (myVer, XrdNID, XrdVNUMBER, XrdVERSION);
   std::string (*ep)(XrdCmsgetVnIdArgs);
   std::string nidName;

// Read the vnid from a file is so directed
//
   if (*nidarg == '<')
      {char buff[nidMax+8];
       int nfd = XrdSysFD_Open(nidarg+1, O_RDONLY);
       if (nfd < 0)
          {eDest.Emsg("Config", errno, "open vnid file", nidarg+1);
           return 0;
          }
       int n = read(nfd, buff, sizeof(buff)-1);
       if (n < 0)
          {eDest.Emsg("Config", errno, "read vnid file", nidarg+1);
           close(nfd);
           return 0;
          }
       close(nfd);
       while(n && buff[n-1] == '\n') n--;
       buff[n] = 0;
       return chkVnId(eDest, buff, "vnid file contains");
      }

// Check if the actual vnid is being specified
//
   if (*nidarg == '=') return chkVnId(eDest, nidarg+1, "vnid value is");

// Make sure a plugin is being passed
//
   if (*nidarg != '@')
      {eDest.Emsg("Config", "vnid specification is invalid -", nidarg);
       return 0;
      }

// Get the entry point of the node ID creator
// 
   XrdOucPinLoader nidLib(&eDest, &myVer, "vnid", nidarg+1);
   ep = (std::string (*)(XrdCmsgetVnIdArgs))(nidLib.Resolve("XrdCmsgetVnId"));
   if (!ep) return 0;

// Get the node ID
// 
   nidName = ep(eDest, std::string(cfgFN), std::string(nidparm ? nidparm : ""),
                       nidType, nidMax);

// Unload the plugin (we don't need it anymore)
//
   nidLib.Unload();

// Verify that the node ID meets specs
//
   return chkVnId(eDest, nidName.c_str(), "vnid plugin returned");
}
  
/******************************************************************************/
/*                              g e t T o k e n                               */
/******************************************************************************/
  
const char *XrdCmsSecurity::getToken(int &size, XrdNetAddrInfo *endPoint)
{

// If not configured, return a null to indicate no authentication required
//
   if (!DHS) {size = 0; return 0;}

// Return actual token
//
   return DHS->getParms(size, endPoint);
}

/******************************************************************************/
/*                              I d e n t i f y                               */
/******************************************************************************/
  
int XrdCmsSecurity::Identify(XrdLink *Link, XrdCms::CmsRRHdr &inHdr, 
                             char *authBuff, int abLen)
{
   CmsRRHdr outHdr = {0, kYR_xauth, 0, 0};
   const char *hName = Link->Host();
   XrdSecCredentials *cred;
   XrdSecProtocol    *AuthProt = 0;
   XrdSecParameters   AuthParm, *AuthP = 0;
   XrdOucErrInfo      eMsg;
   const char        *eText = 0;
   int rc, myDlen;

// Verify that we are configured
//
   if (!getProtocol && !Configure("libXrdSec.so"))
      {Say.Emsg("Auth", hName ,"authentication configuration failed.");
       return 0;
      }

// Obtain the protocol
//
   AuthParm.buffer = (char *)authBuff; AuthParm.size = strlen(authBuff);
   if (!(AuthProt = getProtocol(hName,*(Link->AddrInfo()),AuthParm,&eMsg)))
      {Say.Emsg("Auth", hName, "getProtocol() failed;", eMsg.getErrText(rc));
       return 0;
      }

// Perform standard authentication
//
do {

// Get credentials
//
   if (!(cred = AuthProt->getCredentials(AuthP, &eMsg)))
      {eText = eMsg.getErrText(rc); break;}

// Send credentials to the server
//
   eText = XrdCmsTalk::Request(Link, outHdr, cred->buffer, cred->size);
   delete cred;
   if (eText) break;

// Get the response header and prepare for next iteration if need be
//
   if ((eText = XrdCmsTalk::Attend(Link,inHdr,authBuff,abLen,myDlen))) break;
   AuthParm.size = myDlen; AuthParm.buffer = authBuff; AuthP = &AuthParm;

} while(inHdr.rrCode == kYR_xauth);

// Check if we failed
//
   if (eText) Say.Emsg("Auth", hName, "authentication failed;", eText);

// Perform final steps here
//
   if (AuthProt) AuthProt->Delete();
   return (eText == 0);
}

/******************************************************************************/
/*                            s e t S e c F u n c                             */
/******************************************************************************/
  
void XrdCmsSecurity::setSecFunc(void *secfP)
     {getProtocol = (XrdSecGetProt_t)secfP;}

/******************************************************************************/
/*                           s e t S y s t e m I D                            */
/******************************************************************************/
  
char *XrdCmsSecurity::setSystemID(XrdOucTList *tp,    const char *iVNID,
                                  const char  *iTag,        char  iType)
{
   XrdOucTList *tpF;
   char sidbuff[8192], *sidend = sidbuff+sizeof(sidbuff)-32;
   char *cP, *sp = sidbuff;
   char *fMan, *fp, *xp;
   int n;

// Extract out the instance name (we must have one)
//
   const char *instP = getenv("XRDINSTANCE");
   if (instP) instP = index(instP, ' ');
   if (!instP) return (char *)"!envar XRDINSTANCE undefined.";
   while(*instP && *instP == ' ') instP++;
   if (!(*instP)) return (char *)"!envar XRDINSTANCE invalid.";

// The system ID starts with the semi-unique name of this node unless it's
// a vnetid, in which case it's unique withn this cluster. Note that vnetid's
// always start with an asterisk. It does not otherwise.
//
   if (iVNID)
      {*sp++ = '*'; *sp++ = iType; *sp++ = '-';
       strcpy(sp, iVNID);
       sp += strlen(iVNID);
      } else {
       *sp++ = iType; *sp++ = '-';
       strcpy(sp, instP);
       sp += strlen(instP);
      }

// Export the vnid
//
   *sp = 0;
   XrdOucEnv::Export("XRDCMSVNID", sidbuff);
   *sp++ = ' '; cP = sp;

// Insert tag if we have one
//
   if (iTag) sp += sprintf(sp, "%s.", iTag);

// Develop a unique cluster name for this cluster
//
   if (!tp) sp += sprintf(sp, "%s", instP);
      else {tpF = tp;
            fMan = tp->text + strlen(tp->text) - 1;
            while((tp = tp->next))
                 {fp = fMan; xp = tp->text + strlen(tp->text) - 1;
                  do {if (*fp != *xp) break;
                      xp--;
                     } while(fp-- != tpF->text);
                  if ((n = xp - tp->text + 1) > 0)
                     {sp += sprintf(sp, "%d", tp->val);
                      if (sp+n >= sidend) return (char *)0;
                      strncpy(sp, tp->text, n); sp += n;
                     }
                 }
            sp += sprintf(sp, "%d", tpF->val);
            n = strlen(tpF->text);
            if (sp+n >= sidend) return (char *)0;
            strcpy(sp, tpF->text); sp += n;
           }

// Set envar to hold the cluster name
//
   *sp = '\0';
   XrdOucEnv::Export("XRDCMSCLUSTERID", cP);

// Export the full virtual network ID
//
   XrdOucEnv::Export("XRDCMSSYSID", sidbuff);

// Return the system ID
//
   return  strdup(sidbuff);
}
