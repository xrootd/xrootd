/******************************************************************************/
/*                                                                            */
/*                    X r d S e c P r o t e c t o r . c c                     */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cerrno>
#include <cinttypes>
#include <cstring>
#include <sys/types.h>

#include "XrdVersion.hh"

#include "XrdNet/XrdNetIF.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSec/XrdSecProtect.hh"
#include "XrdSec/XrdSecProtector.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                       L i b r a r y   L i n k a g e                        */
/******************************************************************************/
  
namespace
{
class protoProtector : public XrdSecProtector
{
public:
      protoProtector() {}
     ~protoProtector() {}
};

protoProtector baseProtector;
}

XrdSecProtector *XrdSecProtObjectP = &baseProtector;

XrdVERSIONINFO(XrdSecProtObjectP,"secProt");

/******************************************************************************/
/*             S e r v e r - S i d e   C o n f i g u r a t i o n              */
/******************************************************************************/

namespace
{
struct ProtInfo {XrdSecProtect               *theProt;
                 ServerResponseReqs_Protocol  reqs;
                 bool                         relaxed;
                 bool                         force;
                 ProtInfo() : theProt(0), relaxed(false), force(false)
                             {reqs.theTag = 'S';
                              reqs.rsvd   = 0;
                              reqs.secver = kXR_secver_0;
                              reqs.secopt = 0;
                              reqs.seclvl = kXR_secNone;
                              reqs.secvsz = 0;
                             }
                } lrTab[XrdSecProtector::isLR];

bool           lrSame  = true;
bool           noProt  = true;
}
  
/******************************************************************************/
/*     S e r v e r - S i d e   E r r o r   M e s s a g e   R o u t i n g      */
/******************************************************************************/

namespace
{
XrdSysError  Say(0, "sec_");
}

/******************************************************************************/
/*                                C o n f i g                                 */
/******************************************************************************/
  
bool XrdSecProtector::Config(const XrdSecProtectParms &lclParms,
                             const XrdSecProtectParms &rmtParms,
                                   XrdSysLogger       &logr)
{

// Set the logger right off
//
   Say.logger(&logr);

// Setup local protection
//
   if (lclParms.level != XrdSecProtectParms::secNone)
      {Config(lclParms, lrTab[isLcl].reqs);
       lrTab[isLcl].theProt = new XrdSecProtect;
       lrTab[isLcl].theProt->SetProtection(lrTab[isLcl].reqs);
      }

// Setup remote protection (check for reuse of local protection)
//
   if (rmtParms.level == lclParms.level)
      {lrTab[isRmt] = lrTab[isLcl];
       lrSame = true;
      } else {
       lrSame = false;
       if (rmtParms.level != XrdSecProtectParms::secNone)
          {Config(rmtParms, lrTab[isRmt].reqs);
           lrTab[isRmt].theProt = new XrdSecProtect;
           lrTab[isRmt].theProt->SetProtection(lrTab[isRmt].reqs);
          }
      }

// Record relax flags
//
   lrTab[isLcl].relaxed = (lclParms.opts & XrdSecProtectParms::relax) != 0;
   lrTab[isLcl].force   = (lclParms.opts & XrdSecProtectParms::force) != 0;
   lrTab[isRmt].relaxed = (rmtParms.opts & XrdSecProtectParms::relax) != 0;
   lrTab[isRmt].force   = (rmtParms.opts & XrdSecProtectParms::force) != 0;

// Setup shortcut flag
//
   noProt = (lrTab[isLcl].theProt == 0) && (lrTab[isRmt].theProt == 0);

// All done
//
   return true;
}

/******************************************************************************/
  
void XrdSecProtector::Config(const XrdSecProtectParms    &parms,
                             ServerResponseReqs_Protocol &reqs)
{
   unsigned int lvl;

// Setup options
//
   if ((parms.opts & XrdSecProtectParms::doData) != 0)
      reqs.secopt |= kXR_secOData;
   if ((parms.opts & XrdSecProtectParms::force)  != 0)
      reqs.secopt |= kXR_secOFrce;

// Setup level
//
   switch(parms.level)
         {case XrdSecProtectParms::secCompatible: lvl = kXR_secCompatible;
                                                  break;
          case XrdSecProtectParms::secStandard:   lvl = kXR_secStandard;
                                                  break;
          case XrdSecProtectParms::secIntense:    lvl = kXR_secIntense;
                                                  break;
          case XrdSecProtectParms::secPedantic:   lvl = kXR_secPedantic;
                                                  break;
          default:                                lvl = kXR_secNone;
                                                  break;
         }
    reqs.seclvl = lvl;
}

/******************************************************************************/
/*                                 L N a m e                                  */
/******************************************************************************/
  
const char *XrdSecProtector::LName(XrdSecProtectParms::secLevel level)
{
   static const char *lvlVec[] = {"none",    "compatible", "standard",
                                  "intense", "pedantic"};

// Validate the level
//
  if (level < XrdSecProtectParms::secNone) level = XrdSecProtectParms::secNone;
     else if (level > XrdSecProtectParms::secPedantic)
              level = XrdSecProtectParms::secPedantic;

// Return the level name
//
   return lvlVec[level];
}

/******************************************************************************/
/*                            N e w 4 C l i e n t                             */
/******************************************************************************/
  
XrdSecProtect *XrdSecProtector::New4Client(XrdSecProtocol              &aprot,
                                     const ServerResponseReqs_Protocol &inReqs,
                                           unsigned int                 reqLen)
{
   static const unsigned int hdrLen = sizeof(ServerResponseBody_Protocol)
                                    - sizeof(ServerResponseSVec_Protocol);
   XrdSecProtect *secP;
   unsigned int vLen = static_cast<unsigned int>(inReqs.secvsz)
                     * sizeof(ServerResponseSVec_Protocol);
   bool okED;

// Validate the incoming struct (if it's bad skip the security) and that any
// security is actually wanted.
//
   if (vLen+hdrLen > reqLen
   ||  (inReqs.secvsz == 0 && inReqs.seclvl == kXR_secNone)) return 0;

// If the auth protocol doesn't support encryption, see if we still need to
// send off signed requests (mostly for testng)
//
   okED = aprot.getKey() > 0;
   if (!okED && (inReqs.secopt & kXR_secOFrce) == 0) return 0;

// Get a new security object and set its security level
//
   secP = new XrdSecProtect(&aprot, okED);
   secP->SetProtection(inReqs);

// All done
//
   return secP;
}

/******************************************************************************/
/*                            N e w 4 S e r v e r                             */
/******************************************************************************/
  
XrdSecProtect *XrdSecProtector::New4Server(XrdSecProtocol &aprot, int plvl)
{
   static const char *wFrc = "authentication can't encrypt; "
                             "continuing without it!";
   static const char *wIgn = "authentication can't encrypt; "
                             "allowing unsigned requests!";
   XrdSecProtect *secP;
   lrType theLR;
   bool okED;

// Check if we need any security at all
//
   if (noProt) return 0;

// Now we need to see whether this is local or remote of if it matters
//
   if (lrSame) theLR = isLcl;
      else theLR = (XrdNetIF::InDomain(aprot.Entity.addrInfo) ? isLcl : isRmt);

// Now check again, as may not need any protection for the domain
//
   if (lrTab[theLR].theProt == 0) return 0;

// Check for relaxed processing
//
   if (plvl < kXR_PROTSIGNVERSION && lrTab[theLR].relaxed) return 0;

// Check if protocol supports encryption
//
   okED = aprot.getKey() > 0;
   if (!okED)
      {char pName[XrdSecPROTOIDSIZE+1];
       const char *action;
       strncpy(pName, aprot.Entity.prot, XrdSecPROTOIDSIZE);
       pName[XrdSecPROTOIDSIZE] = 0;
       action = (lrTab[theLR].force ? wFrc : wIgn);
       Say.Emsg("Protect", aprot.Entity.tident, pName, action);
       if (!lrTab[theLR].force) return 0;
      }

// Get a new security object and make it a clone of this right one
//
    secP = new XrdSecProtect(&aprot, *lrTab[theLR].theProt, okED);

// All done
//
   return secP;
}

/******************************************************************************/
/*                              P r o t R e s p                               */
/******************************************************************************/

int XrdSecProtector::ProtResp(ServerResponseReqs_Protocol &resp,
                              XrdNetAddrInfo &nai, int pver)
{
   static const int rsplen = sizeof(ServerResponseReqs_Protocol)
                           - sizeof(ServerResponseSVec_Protocol);
   ServerResponseReqs_Protocol *myResp;

// Check if we need any response at all
//
   if (noProt) return 0;

// Get the right response
//
   if (lrSame || XrdNetIF::InDomain(&nai)) myResp = &lrTab[isLcl].reqs;
      else myResp = &lrTab[isRmt].reqs;

// Return result
//
   memcpy(&resp, myResp, rsplen);
   return rsplen;
}
