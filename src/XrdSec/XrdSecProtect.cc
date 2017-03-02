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

#ifdef __APPLE__
#define COMMON_DIGEST_FOR_OPENSSL
#include "CommonCrypto/CommonDigest.h"
#else
#include "openssl/sha.h"
#endif

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
  
namespace XrdSecProtection
{
struct XrdSecReq
{
       SecurityRequest secReq;
       unsigned char   secSig;  // The encrypted hash follows starting here
};
}

using namespace XrdSecProtection; // Fix warnings from slc5 compiler!

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

XrdSecVec secTable(0,
//             Compatible      Standard        Intense         Pedantic
kXR_admin,     kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, 
kXR_auth,      kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, 
kXR_bind,      kXR_signIgnore, kXR_signIgnore, kXR_signNeeded, kXR_signNeeded,
kXR_chmod,     kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, 
kXR_close,     kXR_signIgnore, kXR_signIgnore, kXR_signNeeded, kXR_signNeeded,
kXR_decrypt,   kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, 
kXR_dirlist,   kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signNeeded,
kXR_endsess,   kXR_signIgnore, kXR_signIgnore, kXR_signNeeded, kXR_signNeeded,
kXR_getfile,   kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, 
kXR_locate,    kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signNeeded,
kXR_login,     kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, 
kXR_mkdir,     kXR_signIgnore, kXR_signNeeded, kXR_signNeeded, kXR_signNeeded,
kXR_mv,        kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, 
kXR_open,      kXR_signLikely, kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, 
kXR_ping,      kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, 
kXR_prepare,   kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signNeeded,
kXR_protocol,  kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, 
kXR_putfile,   kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, 
kXR_query,     kXR_signIgnore, kXR_signIgnore, kXR_signLikely, kXR_signNeeded,
kXR_read,      kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signNeeded,
kXR_readv,     kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signNeeded,
kXR_rm,        kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, 
kXR_rmdir,     kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, 
kXR_set,       kXR_signLikely, kXR_signLikely, kXR_signNeeded, kXR_signNeeded, 
kXR_sigver,    kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, 
kXR_stat,      kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signNeeded,
kXR_statx,     kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signNeeded,
kXR_sync,      kXR_signIgnore, kXR_signIgnore, kXR_signIgnore, kXR_signNeeded,
kXR_truncate,  kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, kXR_signNeeded, 
kXR_verifyw,   kXR_signIgnore, kXR_signIgnore, kXR_signNeeded, kXR_signNeeded,
kXR_write,     kXR_signIgnore, kXR_signIgnore, kXR_signNeeded, kXR_signNeeded,
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
   if (theLvl == kXR_signIgnore) return false;
   if (theLvl != kXR_signLikely) return true;

// Security is conditional based on open() trying to modify something.
//
   if (reqCode == kXR_open)
      {kXR_int16 opts = ntohs(thereq.open.options);
       return (opts & rwOpen) != 0;
      }

// Security is conditional based on query() trying to modify something.
//
   if (reqCode == kXR_query)
      {short qopt = (short)ntohs(thereq.query.infotype);
       switch(qopt)
             {case kXR_QStats:  return false;
              case kXR_Qcksum:  return false;
              case kXR_Qckscan: return false;
              case kXR_Qconfig: return false;
              case kXR_Qspace:  return false;
              case kXR_Qxattr:  return false;
              case kXR_Qopaque:
              case kXR_Qopaquf: return true;
              case kXR_Qopaqug: return true;
              default:          return false;
             }
      }

// Security is conditional based on set() trying to modify something.
//
   if (reqCode == kXR_set) return thereq.set.modifier != 0;

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
   static const ClientSigverRequest initSigVer = {{0,0}, htons(kXR_sigver),
                                                  0, kXR_secver_0, 0, 0,
                                                  kXR_SHA256, {0, 0, 0}, 0
                                                 };
   struct buffHold {XrdSecReq    *P;
                    XrdSecBuffer *bP;
                    buffHold() : P(0), bP(0) {}
                   ~buffHold() {if (P) free(P); if (bP) delete bP;}
                   };
   static const  int iovNum = 3;
   struct iovec  iov[iovNum];
   buffHold      myReq;
   kXR_unt64     mySeq;
   const char    *sigBuff, *payload = thedata;
   unsigned char secHash[SHA256_DIGEST_LENGTH];
   int           sigSize, n, newSize, rc, paysize = 0;
   bool          nodata = false;

// Generate a new sequence number
//
   mySeq = nextSeqno++;
   mySeq = htonll(mySeq);

// Determine if we are going to sign the payload and its location
//
   if (thereq.header.dlen)
      {kXR_unt16 reqid = htons(thereq.header.requestid);
       paysize = ntohl(thereq.header.dlen);
       if (!payload) payload = ((char *)&thereq) + sizeof(ClientRequest);
       if (reqid == kXR_write || reqid == kXR_verifyw) n = (secVerData ? 3 : 2);
          else n = 3;
      }   else n = 2;

// Fill out the iovec
//
   iov[0].iov_base = (char *)&mySeq;
   iov[0].iov_len  = sizeof(mySeq);
   iov[1].iov_base = (char *)&thereq;
   iov[1].iov_len  = sizeof(ClientRequest);
   if (n < 3) nodata = true;
      else {iov[2].iov_base = (char *)payload;
            iov[2].iov_len  = paysize;
           }

// Compute the hash
//
   if (!GetSHA2(secHash, iov, n)) return -EDOM;

// Now encrypt the hash
//
   if (edOK)
      {rc = authProt->Encrypt((const char *)secHash,sizeof(secHash),&myReq.bP);
       if (rc < 0) return rc;
       sigSize = myReq.bP->size;
       sigBuff = myReq.bP->buffer;
      } else {
       sigSize = sizeof(secHash);
       sigBuff = (char *)secHash;
      }

// Allocate a new request object
//
   newSize = sizeof(SecurityRequest) + sigSize;
   myReq.P = (XrdSecReq *)malloc(newSize);
   if (!myReq.P) return -ENOMEM;

// Setup the security request (we only support signing)
//
   memcpy(&(myReq.P->secReq), &initSigVer, sizeof(ClientSigverRequest));
   memcpy(&(myReq.P->secReq.header.streamid ), thereq.header.streamid,
          sizeof(myReq.P->secReq.header.streamid));
   memcpy(&(myReq.P->secReq.sigver.expectrid),&thereq.header.requestid,
          sizeof(myReq.P->secReq.sigver.expectrid));
   myReq.P->secReq.sigver.seqno = mySeq;
   if (nodata) myReq.P->secReq.sigver.flags |= kXR_nodata;
   myReq.P->secReq.sigver.dlen   = htonl(sigSize);

// Append the signature to the request
//
   memcpy(&(myReq.P->secSig), sigBuff, sigSize);

// Return pointer to he security request and its size
//
   newreq = &(myReq.P->secReq); myReq.P = 0;
   return newSize;
}

/******************************************************************************/
/* Private:                S e t P r o t e c t i o n                          */
/******************************************************************************/
  
void XrdSecProtect::SetProtection(const ServerResponseReqs_Protocol &inReqs)
{
   unsigned int lvl, vsz;

// Check for no security, the simlplest case
//
   if (inReqs.secvsz == 0 && inReqs.seclvl == 0)
      {memset(&myReqs, 0, sizeof(myReqs));
       secVec     = 0;
       secVerData = false;
       return;
      }

// Precheck the security level
//
   lvl = inReqs.seclvl;
   if (lvl > kXR_secPedantic) lvl = kXR_secPedantic;

// Perform the default setup (the usual case)
//
   secVec        = secTable.Vec[lvl-1];
   myReqs.seclvl = lvl;
   myReqs.secvsz = 0;
   myReqs.secver = kXR_secver_0;
   myReqs.secopt = inReqs.secopt;

// Set options
//
   secVerData    = (inReqs.secopt & kXR_secOData) != 0;

// Create a modified vectr if there are overrides
//
   if (inReqs.secvsz != 0)
      {const ServerResponseSVec_Protocol *urVec = &inReqs.secvec;
       memcpy(myVec, secVec, maxRIX);
       vsz = inReqs.secvsz;
       for (unsigned int i = 0; i < vsz; i++, urVec++)
           {if (urVec->reqindx < maxRIX)
               {if (urVec->reqsreq > kXR_signNeeded)
                        myVec[urVec->reqindx] = kXR_signNeeded;
                   else myVec[urVec->reqindx] = urVec->reqsreq;
               }
           }
       secVec = myVec;
       }
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
// the previous one we have seen. Since it is in network byte order we can use
// a simple byte for byte compare (no need for byte swapping).
//
   if (memcmp(&lastSeqno, &secreq.sigver.seqno, sizeof(lastSeqno)) >= 0)
      return "Incorrect signature sequence";

// Do basic verification for this request
//
   if (memcmp(secreq.header.streamid, thereq.header.streamid,
              sizeof(secreq.header.streamid)))
      return "Signature streamid mismatch";
   if (secreq.sigver.expectrid != thereq.header.requestid)
      return "Signature requestid mismatch";
   if (secreq.sigver.version   != kXR_secver_0)
      return "Unsupported signature version";
   if ((secreq.sigver.crypto & kXR_HashMask) != kXR_SHA256)
      return "Unsupported signature hash";
   if (secreq.sigver.crypto & kXR_rsaKey)
      return "Unsupported signature key";

// Now get the hash information
//
   dlen = ntohl(secreq.header.dlen);
   inHash = ((unsigned char *)&secreq)+sizeof(SecurityRequest);

// Now decrypt the hash
//
   if (edOK)
      {rc = authProt->Decrypt((const char *)inHash, dlen, &myReq.bP);
       if (rc < 0) return strerror(-rc);
       if (myReq.bP->size != (int)sizeof(secHash))
          return "Invalid signature hash length";
       inHash = (unsigned char *)myReq.bP->buffer;
      } else {
       if (dlen != (int)sizeof(secHash))
          return "Invalid signature hash length";
      }

// Fill out the iovec to recompute the hash
//
   iov[0].iov_base = (char *)&secreq.sigver.seqno;
   iov[0].iov_len  = sizeof(secreq.sigver.seqno);
   iov[1].iov_base = (char *)&thereq;
   iov[1].iov_len  = sizeof(ClientRequest);
   if (thereq.header.dlen == 0 || secreq.sigver.flags & kXR_nodata) n = 2;
      else {iov[2].iov_base = (char *)thedata;
            iov[2].iov_len  = ntohl(thereq.header.dlen);
            n = 3;
           }

// Compute the hash
//
   if (!GetSHA2(secHash, iov, n))
      return "Signature hash computation failed";

// Compare this hash with the hash we were given
//
   if (memcmp(secHash, inHash, sizeof(secHash)))
      return "Signature hash mismatch";

// This request has been verified (update the seqno)
//
   lastSeqno = secreq.sigver.seqno;
   return 0;
}
