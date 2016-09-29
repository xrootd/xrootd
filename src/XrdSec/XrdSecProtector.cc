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

#include <errno.h>
#include <inttypes.h>
#include <string.h>
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
struct ProtInfo {XrdSecProtect *theProt;
                 kXR_int32      theResp;
                 bool           relaxed;
                 bool           force;
                 ProtInfo() : theProt(0), theResp(0), relaxed(false),
                              force(false) {}
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
      {lrTab[isLcl].theProt = new XrdSecProtect;
       lrTab[isLcl].theResp = lrTab[isLcl].theProt->SetProtection(lclParms);
      }

// Setup remote protection (check for reuse of local protection)
//
   if (rmtParms.level == lclParms.level)
      {lrTab[isRmt] = lrTab[isLcl];
       lrSame = true;
      } else {
       lrSame = false;
       if (rmtParms.level != XrdSecProtectParms::secNone)
          {lrTab[isRmt].theProt = new XrdSecProtect;
           lrTab[isRmt].theResp = lrTab[isRmt].theProt->SetProtection(rmtParms);
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
  
XrdSecProtect *XrdSecProtector::New4Client(XrdSecProtocol &aprot,
                                           kXR_int32       presp)
{
   XrdSecProtect     *secP;
   XrdSecProtectParms myParms;
   int  n;
   bool okED = aprot.getKey() > 0;

// Check if we need any security at all
//
   n = (presp & kXR_secLvl);
   if (n == 0 || (!okED && ((presp & kXR_secOFrce) == 0))) return 0;

// Get security level
//
   myParms.level = static_cast<XrdSecProtectParms::secLevel>(n>>kXR_secLvlSft);

// Get additional options
//
   if ((presp & kXR_secOEnc)  != 0)
      myParms.opts |= XrdSecProtectParms::useEnc;
   if ((presp & kXR_secOData) != 0)
      myParms.opts |= XrdSecProtectParms::doData;

// Get a new security object and set its security level
//
   secP = new XrdSecProtect(&aprot, okED);
   secP->SetProtection(myParms);

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

kXR_int32 XrdSecProtector::ProtResp(XrdNetAddrInfo &nai, int pver)
{

// Check if we need any response at all
//
   if (noProt) return 0;

// Return the right response
//
   if (lrSame || XrdNetIF::InDomain(&nai)) return lrTab[isLcl].theResp;
   return lrTab[isRmt].theResp;
}
