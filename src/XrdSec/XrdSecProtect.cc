/******************************************************************************/
/*                                                                            */
/*                      X r d S e c P r o t e c t . c c                       */
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
#include <netinet/in.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "openssl/sha.h"

#include "XrdVersion.hh"

#include "XProtocol/XProtocol.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSec/XrdSecProtect.hh"
#include "XrdSec/XrdSecProtector.hh"
#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"
  
/******************************************************************************/
/*                      S t r u c t   X r d S e c R e q                       */
/******************************************************************************/
  
namespace
{
struct XrdSecReq
{
SecurityRequest secReq;
unsigned char   secHash[SHA256_DIGEST_LENGTH];
ClientRequest   orgReq;
};

const ClientSigverRequest initSigVer = {{0,0}, htons(kXR_sigver), 0,
                                        kXR_secver_0, kXR_sessKey, 0,
                                        kXR_SHA256, {0, 0, 0},
                                        htonl(SHA256_DIGEST_LENGTH)
                                       };
XrdSysMutex seqMutex;
kXR_unt64   seqNum = 1;
}

/******************************************************************************/
/*                       C l a s s   X r d S e c V e c                        */
/******************************************************************************/
  
namespace
{
class XrdSecVec
{
public:

char  Vec[XrdSecProtectParms::secFence-1][kXR_REQFENCE-kXR_auth];

      XrdSecVec(int arg, ...)
               {va_list ap;
                int reqCode, sVal;
                memset(Vec, 0, sizeof(Vec));
                va_start(ap, arg);
                reqCode = va_arg(ap, int);
                while(reqCode >= kXR_auth && reqCode < kXR_REQFENCE)
                     {for (int i=0; i < (int)XrdSecProtectParms::secFence-1; i++)
                          {sVal = va_arg(ap, int);
                           Vec[i][reqCode-kXR_auth] = static_cast<char>(sVal);
                          }
                      reqCode = va_arg(ap, int);
                     }
               }
     ~XrdSecVec() {}
};
}

/******************************************************************************/
/*                        S e c u r i t y   T a b l e                         */
/******************************************************************************/
  
namespace
{
static const int sec_Ignore = 0;
static const int sec_Likely = 1;
static const int sec_Needed = 2;

XrdSecVec secTable(0,
//             Compatible  Standard    Intense     Pedantic
kXR_admin,     sec_Needed, sec_Needed, sec_Needed, sec_Needed, 
kXR_auth,      sec_Ignore, sec_Ignore, sec_Ignore, sec_Ignore, 
kXR_bind,      sec_Ignore, sec_Ignore, sec_Needed, sec_Needed,
kXR_chmod,     sec_Needed, sec_Needed, sec_Needed, sec_Needed, 
kXR_close,     sec_Ignore, sec_Ignore, sec_Needed, sec_Needed,
kXR_decrypt,   sec_Ignore, sec_Ignore, sec_Ignore, sec_Ignore, 
kXR_dirlist,   sec_Ignore, sec_Ignore, sec_Ignore, sec_Needed,
kXR_endsess,   sec_Ignore, sec_Ignore, sec_Needed, sec_Needed,
kXR_getfile,   sec_Needed, sec_Needed, sec_Needed, sec_Needed, 
kXR_locate,    sec_Ignore, sec_Ignore, sec_Ignore, sec_Needed,
kXR_login,     sec_Ignore, sec_Ignore, sec_Ignore, sec_Ignore, 
kXR_mkdir,     sec_Ignore, sec_Needed, sec_Needed, sec_Needed,
kXR_mv,        sec_Needed, sec_Needed, sec_Needed, sec_Needed, 
kXR_open,      sec_Likely, sec_Needed, sec_Needed, sec_Needed, 
kXR_ping,      sec_Ignore, sec_Ignore, sec_Ignore, sec_Ignore, 
kXR_prepare,   sec_Ignore, sec_Ignore, sec_Ignore, sec_Needed,
kXR_protocol,  sec_Ignore, sec_Ignore, sec_Ignore, sec_Ignore, 
kXR_putfile,   sec_Needed, sec_Needed, sec_Needed, sec_Needed, 
kXR_query,     sec_Ignore, sec_Ignore, sec_Ignore, sec_Needed,
kXR_read,      sec_Ignore, sec_Ignore, sec_Ignore, sec_Needed,
kXR_readv,     sec_Ignore, sec_Ignore, sec_Ignore, sec_Needed,
kXR_rm,        sec_Needed, sec_Needed, sec_Needed, sec_Needed, 
kXR_rmdir,     sec_Needed, sec_Needed, sec_Needed, sec_Needed, 
kXR_set,       sec_Likely, sec_Likely, sec_Needed, sec_Needed, 
kXR_sigver,    sec_Ignore, sec_Ignore, sec_Ignore, sec_Ignore, 
kXR_stat,      sec_Ignore, sec_Ignore, sec_Ignore, sec_Needed,
kXR_statx,     sec_Ignore, sec_Ignore, sec_Ignore, sec_Needed,
kXR_sync,      sec_Ignore, sec_Ignore, sec_Ignore, sec_Needed,
kXR_truncate,  sec_Needed, sec_Needed, sec_Needed, sec_Needed, 
kXR_verifyw,   sec_Ignore, sec_Ignore, sec_Needed, sec_Needed,
kXR_write,     sec_Ignore, sec_Ignore, sec_Needed, sec_Needed,
0);
}

/******************************************************************************/
/* Private:                      G e t S H A 2                                */
/******************************************************************************/

bool XrdSecProtect::GetSHA2(unsigned char *hBuff, struct iovec *iovP, int iovN)
{
   SHA256_CTX sha256;

// Initialize the hash calculattion
//
   if (0 == SHA256_Init(&sha256)) return false;

// Go through the iovec updating the hash
//
   for (int i = 0; i < iovN; i++)
       {if (1 != SHA256_Update(&sha256, iovP[i].iov_base, iovP[i].iov_len))
           return false;
       }

// Compute final hash and return result
//
  return (1 == SHA256_Final(hBuff, &sha256));
}

/******************************************************************************/
/* Private:                       S c r e e n                                 */
/******************************************************************************/
  
bool XrdSecProtect::Screen(ClientRequest &thereq)
{
   static const int rwOpen = kXR_delete|kXR_new|kXR_open_apnd|kXR_open_updt;

   kXR_unt16 reqCode = ntohs(thereq.header.requestid);
   char theLvl;

// Validate the request code. Invalid codes are never secured
//
   if (reqCode < kXR_auth || reqCode >= kXR_REQFENCE || !secVec) return false;

// Get the security level
//
   theLvl = secVec[reqCode-kXR_auth];

// If we need not secure this or we definitely do then return result
//
   if (theLvl == sec_Ignore) return false;
   if (theLvl != sec_Likely) return true;

// Security is conditional based on open() trying to modify something.
//
   if (reqCode == kXR_open)
      {kXR_int16 opts = ntohs(thereq.open.options);
       return (opts & rwOpen) != 0;
      }

// Security is conditional based on set() trying to modify something.
//
   if (reqCode == kXR_set) return thereq.set.subCode != 0;

// At this point we force security as we don't understand this code
//
   return true;
}

/******************************************************************************/
/*                                S e c u r e                                 */
/******************************************************************************/
  
int XrdSecProtect::Secure(SecurityRequest *&newreq,
                          ClientRequest    &thereq,
                          const char       *thedata)
{
   struct buffHold {XrdSecReq    *P;
                    XrdSecBuffer *bP;
                    buffHold() : P(0), bP(0) {}
                   ~buffHold() {if (P) free(P); if (bP) delete bP;}
                   };
   static const  int iovNum = 3;
   struct iovec  iov[iovNum];
   buffHold      myReq;
   kXR_unt64     mySeq;
   unsigned char secHash[SHA256_DIGEST_LENGTH];
   int           rc, n;

// Allocate a new request object
//
   myReq.P = (XrdSecReq *)malloc(sizeof(XrdSecReq));
   if (!myReq.P) return -ENOMEM;

// Setup the security request (we only support signing)
//
   memcpy(&(myReq.P->secReq), &initSigVer, sizeof(initSigVer));
   memcpy(&(myReq.P->secReq.header.streamid ), thereq.header.streamid,
          sizeof(myReq.P->secReq.header.streamid));
   memcpy(&(myReq.P->secReq.sigver.expectrid),&thereq.header.requestid,
          sizeof(myReq.P->secReq.sigver.expectrid));
   memcpy(&(myReq.P->orgReq), &thereq, sizeof(myReq.P->orgReq));

// Generate a new sequence number
//
   AtomicBeg(seqMutex);
   mySeq = AtomicInc(seqNum);
   AtomicEnd(seqMutex);
   myReq.P->secReq.sigver.seqno = htonll(mySeq);

// Determine if we are going to sign the payload
//
   if (thereq.header.dlen)
      {kXR_unt16 reqid = htons(thereq.header.requestid);
       if (reqid == kXR_write || reqid == kXR_verifyw) n = (secVerData ? 3 : 2);
          else n = 3;
      }   else n = 2;


// Fill out the iovec
//
   iov[0].iov_base = &(myReq.P->secReq.sigver.seqno);
   iov[0].iov_len  = sizeof(mySeq);
   iov[1].iov_base = &thereq;
   iov[1].iov_len  = sizeof(ClientRequest);
   if (n < 3) myReq.P->secReq.sigver.flags |= kXR_nodata;
      else {iov[2].iov_base = (void *)thedata;
            iov[2].iov_len  = ntohl(thereq.header.dlen);
           }

// Compute the hash
//
   if (!GetSHA2(secHash, iov, n)) return -EDOM;

// Now encrypt the hash
//
   if (edOK)
      {rc = authProt->Encrypt((const char *)secHash,sizeof(secHash),&myReq.bP);
       if (rc < 0) return rc;
       if (myReq.bP->size != (int)sizeof(secHash)) return -ERANGE;
      }

// Move the signature to the request and return new request
//
   memcpy(myReq.P->secHash, myReq.bP, sizeof(secHash));
   newreq = &(myReq.P->secReq); myReq.P = 0;
   return (int)sizeof(XrdSecReq);
}

/******************************************************************************/
/* Private:                S e t P r o t e c t i o n                          */
/******************************************************************************/
  
kXR_int32 XrdSecProtect::SetProtection(const XrdSecProtectParms &parms)
{
   kXR_int32 pResp;
   XrdSecProtectParms::secLevel lvl = parms.level;

// Check for no security, the simlplest case
//
   if (lvl <= XrdSecProtectParms::secNone)
      {secVec = 0;
       secEncrypt = false;
       secVerData = false;
       return 0;
      }

// Validate the level
//
   if (lvl >= XrdSecProtectParms::secFence)
       lvl  = XrdSecProtectParms::secPedantic;

// Insert the security level in the response token
//
   pResp = ((int)lvl)<<kXR_secLvlSft | kXR_secver_0<<kXR_sftVersft;

// Add additional options
//
   if ((secEncrypt = (parms.opts & XrdSecProtectParms::useEnc) != 0))
      pResp |= kXR_secOEnc;
   if ((secVerData = (parms.opts & XrdSecProtectParms::doData) != 0))
      pResp |= kXR_secOData;
   if ((             (parms.opts & XrdSecProtectParms::force)  != 0))
      pResp |= kXR_secOFrce;

// Now establish the security decision vector
//
   secVec = secTable.Vec[lvl-1];

// All done
//
   return pResp;
}

/******************************************************************************/
/*                                V e r i f y                                 */
/******************************************************************************/
  
const char *XrdSecProtect::Verify(SecurityRequest  &secreq,
                                  ClientRequest    &thereq,
                                  const char       *thedata
                                 )
{
   struct buffHold {XrdSecBuffer *bP;
                    buffHold() :  bP(0) {}
                   ~buffHold() {if (bP) delete bP;}
                   };
   static const  int iovNum = 3;
   struct iovec  iov[iovNum];
   buffHold      myReq;
   unsigned char *inHash, secHash[SHA256_DIGEST_LENGTH];
   int           dlen, n, rc;

// First check for replay attacks. The incomming sequence number must be greater
// the previous onw we haveseen. Since it is in network byte order we can use
// a simple byte for byte compare (no need for byte swapping).
//
   if (memcmp(&lastSeqno, &secreq.sigver.seqno, sizeof(lastSeqno)) <= 0)
      return "Incorrect signature sequence";

// Do basic verification for this request
//
   if (memcmp(secreq.header.streamid, thereq.header.streamid,
              sizeof(secreq.header.streamid)))
      return "Signature streamid mismatch";
   if (secreq.sigver.expectrid != thereq.header.requestid)
      return "Signature requestid mismatch";
   if (thereq.sigver.version   != kXR_secver_0)
      return "Unsupported signature version";
   if (thereq.sigver.hash      != kXR_SHA256)
      return "Unsupported signature hash";
   if (thereq.sigver.flags & kXR_rsaKey)
      return "Unsupported signature key";

// Now get the hash information
//
   dlen = ntohl(secreq.header.dlen);
   if (dlen != (int)sizeof(secHash))
      return "Invalid signature hash length";
   inHash = (unsigned char *)(&secreq+sizeof(SecurityRequest));

// Now decrypt the hash
//
   if (edOK)
      {rc = authProt->Decrypt((const char *)inHash, sizeof(secHash), &myReq.bP);
       if (rc < 0) return strerror(-rc);
      }

// Fill out the iovec to recompute the hash
//
   iov[0].iov_base = &secreq.sigver.seqno;
   iov[0].iov_len  = sizeof(secreq.sigver.seqno);
   iov[1].iov_base = &thereq;
   iov[1].iov_len  = sizeof(ClientRequest);
   if (thereq.header.dlen == 0 || secreq.sigver.flags & kXR_nodata) n = 2;
      else {iov[2].iov_base = (void *)thedata;
            iov[2].iov_len  = ntohl(thereq.header.dlen);
            n = 3;
           }

// Compute the hash
//
   if (!GetSHA2(secHash, iov, n))
      return "Signature hash computation failed";

// Compare this hash with the hash we were given
//
   if (memcmp(secHash, myReq.bP->buffer, sizeof(secHash)))
      return "Signature hash mismatch";

// This request has been verified
//
   return 0;
}
