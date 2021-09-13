/******************************************************************************/
/*                                                                            */
/*                  X r d S e c P r o t o c o l z t n . c c                   */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#define __STDC_FORMAT_MACROS 1
#include <alloca.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <fcntl.h>
#include <cinttypes>
#include <iostream>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <ctime>
#include <vector>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "XrdVersion.hh"

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSciTokens/XrdSciTokensHelper.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSec/XrdSecInterface.hh"

#ifndef EAUTH
#define EAUTH EBADE
#endif
/******************************************************************************/
/*                   V e r s i o n   I n f o r m a t i o n                    */
/******************************************************************************/

XrdVERSIONINFO(XrdSecProtocolztnObject,secztn);

/******************************************************************************/
/*                       L o c a l   F u n c t i o n s                        */
/******************************************************************************/
/******************************************************************************/
/*                                 F a t a l                                  */
/******************************************************************************/

namespace
{
XrdSecCredentials *Fatal(XrdOucErrInfo *erp, const char *eMsg, int rc,
                         bool hdr=true)
{
   if (!erp) std::cerr <<"Secztn: " <<eMsg <<"\n" <<std::flush;
      else {const char *eVec[2] = {(hdr ? "Secztn: " : ""), eMsg};
            erp->setErrInfo(rc, eVec, 2);
           }
   return 0;
}

/******************************************************************************/
/*                        m o n o t o n i c _ t i m e                         */
/******************************************************************************/
  
inline uint64_t monotonic_time() {
  struct timespec tp;
#ifdef CLOCK_MONOTONIC_COARSE
  clock_gettime(CLOCK_MONOTONIC_COARSE, &tp);
#else
  clock_gettime(CLOCK_MONOTONIC, &tp);
#endif
  return tp.tv_sec + (tp.tv_nsec >= 500000000);
}

/******************************************************************************/
/*                    G l o b a l   S t a t i c   D a t a                     */
/******************************************************************************/
  
int expiry = 1;
}

/******************************************************************************/
/*                            g e t L i n k a g e                             */
/******************************************************************************/

namespace
{
   XrdSciTokensHelper **sth_Linkage = 0;
   char                *sth_piName  = 0;

bool getLinkage(XrdOucErrInfo *erp, const char *piName)
{
   char eMsgBuff[2048];
   XrdVersionInfo *myVer = &XrdVERSIONINFOVAR(XrdSecProtocolztnObject);
   XrdOucPinLoader myLib(eMsgBuff, sizeof(eMsgBuff), myVer,
                         "ztn.tokenlib", piName);

// Get the address of the pointer to the helper we need
//
   sth_Linkage = (XrdSciTokensHelper **)(myLib.Resolve("SciTokensHelper"));

// If we succeeded, record the name of the plugin and return success
//
   if (sth_Linkage)
      {sth_piName = strdup(piName);
       return true;
      }

// We failed to find this for one reason or another
//
   erp->setErrInfo(ESRCH, eMsgBuff);
   return false;
  }
}

/******************************************************************************/
/*                     L o c a l   D e f i n i t i o n s                      */
/******************************************************************************/
  
namespace
{
int MaxTokSize = 4096;

// Option flags
//
static const uint64_t srvVNum  = 0x00000000000000ffULL;
static const uint64_t useFirst = 0x0000000000000100ULL;
static const uint64_t useLast  = 0x0000000000000200ULL;
static const uint64_t srvRTOK  = 0x0000000000000800ULL;
}

/******************************************************************************/
/*                     E x t e r n a l   L i n k a g e s                      */
/******************************************************************************/
  
namespace XrdSecztn
{
extern bool isJWT(const char *);
}

/******************************************************************************/
/*               X r d S e c P r o t o c o l z t n   C l a s s                */
/******************************************************************************/

class XrdSecProtocolztn : public XrdSecProtocol
{
public:

        int                Authenticate  (XrdSecCredentials *cred,
                                          XrdSecParameters **parms,
                                          XrdOucErrInfo     *einfo=0);

        void               Delete() {delete this;}

        XrdSecCredentials *getCredentials(XrdSecParameters  *parms,
                                          XrdOucErrInfo     *einfo=0);

        bool               needTLS() {return true;}

// Client-side constructor
//
        XrdSecProtocolztn(const char *parms, XrdOucErrInfo *erp, bool &aOK);


// Server-side constructor
//
        XrdSecProtocolztn(const char *hname, XrdNetAddrInfo &endPoint,
                          XrdSciTokensHelper *sthp)
                         : XrdSecProtocol("ztn"), sthP(sthp), tokName(""),
                           maxTSize(MaxTokSize), cont(false),
                           rtGet(false), verJWT(false)
                         {Entity.host = strdup(hname);
                          Entity.name = strdup("anon");
                          Entity.addrInfo = &endPoint;
                         }

       ~XrdSecProtocolztn() {if (Entity.host) free(Entity.host);
                             if (Entity.name) free(Entity.name);
                            } // via Delete()

static const int ztnVersion = 0;

private:


struct TokenHdr
      {char     id[4];
       char     ver;
       char     opr;
       char     rsvd[2]; // Reserved bytes (note struct is 8 bytes long)

       static const char SndAI = 'S';
       static const char IsTkn = 'T';

       void     Fill(char opc) {strcpy(id, "ztn"); ver = ztnVersion;
                                opr = opc; rsvd[0] = rsvd[1] = 0;
                               }
      };

struct TokenResp
      {TokenHdr hdr;
       uint16_t len;
       char     tkn[1];  // Sized to actual token length
      };

XrdSecCredentials *findToken(XrdOucErrInfo *erp,
                             std::vector<XrdOucString> &Vec, bool &isbad);
XrdSecCredentials *getToken(XrdOucErrInfo *erp, XrdSecParameters *parms);
XrdSecCredentials *readFail(XrdOucErrInfo *erp, const char *path, int rc);
XrdSecCredentials *readToken(XrdOucErrInfo *erp, const char *path, bool &isbad);
XrdSecCredentials *retToken(XrdOucErrInfo *erp, const char *tkn, int tsz);
int                SendAI(XrdOucErrInfo *erp, XrdSecParameters **parms);
const char        *Strip(const char *bTok, int &sz);

XrdSciTokensHelper *sthP;
const char         *tokName;
uint64_t            ztnInfo;
int                 maxTSize;
bool                cont;
bool                rtGet;
bool                verJWT;
};

/******************************************************************************/
/*             C l i e n t   O r i e n t e d   F u n c t i o n s              */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdSecProtocolztn::XrdSecProtocolztn(const char *parms, XrdOucErrInfo *erp,
                                     bool       &aOK)
                                    : XrdSecProtocol("ztn"), sthP(0),
                                      tokName(""), ztnInfo(0), maxTSize(0),
                                      cont(false), rtGet(false), verJWT(false)
{
   char *endP;

// Assume we will fail
//
   aOK = false;

// If there are no parameters then fail as the server must supply them
//
   if (!parms || !(*parms))
      {Fatal(erp, "Client parameters not specified.", EINVAL);
       return;
      }

// Server supplied parms: <opts+ver>:<maxtsz>:

// The first parameter is the options and version number.
//
   ztnInfo = strtoll(parms, &endP, 10);
   if (*endP != ':')
      {Fatal(erp, "Malformed client parameters.", EINVAL);
       return;
      }
   parms = endP+1;

// The second parameter is the maximum token size
//
   maxTSize = strtol(parms, &endP, 10);
   if (maxTSize <= 0 || *endP != ':')
      {Fatal(erp, "Invalid or missing maxtsz parameter.", EINVAL);
       return;
      }
   endP++;

// All done here
//
   aOK = true;
}

/******************************************************************************/
/* Private:                    f i n d T o k e n                              */
/******************************************************************************/
  
XrdSecCredentials *XrdSecProtocolztn::findToken(XrdOucErrInfo *erp,
                                                std::vector<XrdOucString> &Vec,
                                                bool &isbad)
{
   XrdSecCredentials *resp;
   const char *aTok, *bTok;
   int sz;

// Look through all of the possible envars
//
   for (int i = 0; i < (int)Vec.size(); i++)
       {tokName = Vec[i].c_str();

        if (Vec[i].beginswith('/') == 1)
           {char tokPath[MAXPATHLEN+8];
            snprintf(tokPath, sizeof(tokPath), tokName,
                     Vec[i].length(), int(geteuid()));
            resp = readToken(erp, tokPath, isbad);
            if (resp || isbad) return resp;
            continue;
           }

        if (!(aTok = getenv(Vec[i].c_str())) || !*(aTok)) continue;

        if (Vec[i].endswith("_DIR"))
           {char tokPath[MAXPATHLEN+8];
            snprintf(tokPath,sizeof(tokPath),"%s/bt_u%d",aTok,int(geteuid()));
            resp = readToken(erp, tokPath, isbad);
            if (resp || isbad) return resp;
            continue;
           }

        if (Vec[i].endswith("_FILE"))
           {if ((resp = readToken(erp, aTok, isbad)) || isbad) return resp;
            continue;
           }

        if ((bTok = Strip(aTok, sz))) return retToken(erp, bTok, sz);
       }

// Nothing found
//
  isbad = false;
  return 0;
}

/******************************************************************************/
/*                        g e t C r e d e n t i a l s                         */
/******************************************************************************/

XrdSecCredentials *XrdSecProtocolztn::getCredentials(XrdSecParameters *parms,
                                                     XrdOucErrInfo    *error)
{
   static const char  *dfltLoc[] = {"BEARER_TOKEN",    "BEARER_TOKEN_FILE",
                                    "XDG_RUNTIME_DIR", "/tmp/bt_u%d"};
   static const char **dfltLocEnd = dfltLoc + sizeof(dfltLoc)/sizeof(char*);
   static std::vector<XrdOucString> dfltVec(dfltLoc, dfltLocEnd);

   XrdSecCredentials *resp;
   bool isbad;

// If this is a continuation, then handle as such
//
   if (cont) return getToken(error, parms);

// Handle the default search
//
   resp = findToken(error, dfltVec, isbad);
   if (resp || isbad) return resp;

// We do not have a envar value then ask the server for a list of
// token issuers so we can get one, if allowed. Otherwise, it's an error.
//
   if (rtGet)
      {TokenHdr *tHdr = (TokenHdr *)malloc(sizeof(TokenHdr));
       tHdr->Fill(TokenHdr::SndAI);
       cont = true;
       return new XrdSecCredentials((char *)tHdr, sizeof(TokenHdr));
      }
   Fatal(error, "No token found; runtime fetch disallowed.", ENOPROTOOPT);
   return 0;
}
  
/******************************************************************************/
/* Private:                     g e t T o k e n                               */
/******************************************************************************/

XrdSecCredentials *XrdSecProtocolztn::getToken(XrdOucErrInfo    *erp,
                                               XrdSecParameters *parms)
{
// We currently do not support dynamic token creation
//
   return Fatal(erp, "Realtime token creation not supported.", ENOTSUP);
}
  
/******************************************************************************/
/* Private:                     r e a d F a i l                               */
/******************************************************************************/

XrdSecCredentials *XrdSecProtocolztn::readFail(XrdOucErrInfo *erp,
                                               const char *path, int rc)
{
   const char *mVec[7];
   int k = 6;

   mVec[0] = "Secztn: Unable to find token via ";
   mVec[1] = tokName;
   mVec[2] = "=";
   mVec[3] = path;
   mVec[4] = "; ";
   mVec[5] = XrdSysE2T(rc);
   if (rc == EPERM) mVec[k++] = " because of excessive permissions";


   if (erp) erp->setErrInfo(rc, mVec, k);
      else {for (int k = 0; k < 6; k++) std::cerr <<mVec[k];
            std::cerr <<"\n" <<std::flush;
           }

   return 0;
}
  
/******************************************************************************/
/* Private:                    r e a d T o k e n                              */
/******************************************************************************/

XrdSecCredentials *XrdSecProtocolztn::readToken(XrdOucErrInfo *erp,
                                                const char *path, bool &isbad)
{
   struct stat Stat;
   const char *bTok;
   char *buff;
   int rdLen, sz, tokFD;

// Be pessimistic
//
   isbad = true;

// Get the size of the file
//
   if (stat(path, &Stat))
      {if (errno != ENOENT) return readFail(erp, path, errno);
       isbad = false;
       return 0;
      }

// Make sure token is not too big
//
   if (Stat.st_size > maxTSize) return readFail(erp, path, EMSGSIZE);
   buff = (char *)alloca(Stat.st_size+1);

// Open the token file
//
   if ((tokFD = open(path, O_RDONLY)) < 0)
      return readFail(erp, path, errno);

// Read in the token
//
   if ((rdLen = read(tokFD, buff, Stat.st_size)) != Stat.st_size)
      {int rc = (rdLen < 0 ? errno : EIO);
       close(tokFD);
       return readFail(erp, path, rc);
      }
   close(tokFD);

// Make sure the token ends with a null byte
//
   buff[Stat.st_size] = 0;

// Strip the token
//
   if (!(bTok = Strip(buff, sz)))
      {isbad = false;
       return 0;
      }

// Make sure the file is not accessible to anyone but the owner
//
   if (Stat.st_mode & (S_IRWXG | S_IRWXO)) return readFail(erp, path, EPERM);

// Return response
//
   return retToken(erp, bTok, sz);
}
  
/******************************************************************************/
/* Private:                     r e t T o k e n                               */
/******************************************************************************/

XrdSecCredentials *XrdSecProtocolztn::retToken(XrdOucErrInfo *erp,
                                               const char    *tkn, int tsz)
{
   TokenResp *tResp;
   int rspLen = sizeof(TokenResp) + tsz + 1;

// Make sure token is not too big
//
   if (tsz >= maxTSize) return Fatal(erp, "Token is too big", EMSGSIZE);

// Verify that this is actually a JWT if so wanted
//
   if (verJWT && !XrdSecztn::isJWT(tkn)) return 0;

// Get sufficient storage to assemble the full response
//
   tResp = (TokenResp *)malloc(rspLen);
   if (!tResp)
      {Fatal(erp, "Insufficient memory.", ENOMEM);
       return 0;
      }

// Fill out the response
//
   tResp->hdr.Fill(TokenHdr::IsTkn);
   tResp->len = htons(tsz+1);
   memcpy(tResp->tkn, tkn, tsz);
   *((tResp->tkn)+tsz) = 0;

// Now return it
//
   return new XrdSecCredentials((char *)tResp, rspLen);
}
  
/******************************************************************************/
/* Private:                        S t r i p                                  */
/******************************************************************************/

const char *XrdSecProtocolztn::Strip(const char *bTok, int &sz)
{
   int j, k, n = strlen(bTok);

// Make sure we have at least one character here
//
   if (!n) return 0;

// Find first non-whitespace character
//
   for (j = 0; j < n; j++) if (!isspace(static_cast<int>(bTok[j]))) break;

// Make sure we have at least one character
//
   if (j >= n) return 0;

// Find last non-whitespace character
//
   for (k = n-1; k > j; k--) if (!isspace(static_cast<int>(bTok[k]))) break;

// Compute length and allocate enough storage to copy the token
//
   if (k <= j) return 0;

// Compute length and return pointer to the token
//
   sz = k - j + 1;
   return bTok + j;
}
  
/******************************************************************************/
/*               S e r v e r   O r i e n t e d   M e t h o d s                */
/******************************************************************************/
/******************************************************************************/
/*                          A u t h e n t i c a t e                           */
/******************************************************************************/

int XrdSecProtocolztn::Authenticate(XrdSecCredentials *cred,
                                    XrdSecParameters **parms,
                                    XrdOucErrInfo     *erp)
{
   static const int pfxLen = sizeof(TokenHdr) + sizeof(uint16_t);
   TokenResp *tResp;

// Check if we have any credentials or if no credentials really needed.
// In either case, use host name as client name
//
   if (cred->size < (int)sizeof(TokenHdr) || !cred->buffer)
      {Fatal(erp, "Invalid ztn credentials", EINVAL, false);
       return -1;
      }
   tResp = (TokenResp *)cred->buffer;

// Check if this is our protocol
//
   if (strcmp(tResp->hdr.id, "ztn"))
      {char msg[256];
       snprintf(msg, sizeof(msg),
                "Authentication protocol id mismatch ('ztn' != '%.4s').",
                tResp->hdr.id);
       Fatal(erp, msg, EINVAL, false);
       return -1;
      }

// Check if caller wants the list of authorized issuers
//
   if (tResp->hdr.opr == TokenHdr::SndAI) return SendAI(erp, parms);

// If this is not a token response then this is an error
//
   if (tResp->hdr.opr != TokenHdr::IsTkn)
      {Fatal(erp, "Invalid ztn response code", EINVAL, false);
       return -1;
      }

// Make sure the response is consistent
//
   const char *isBad = 0;
   int tLen = ntohs(tResp->len);

        if (tResp->hdr.ver != ztnVersion) isBad = "version mismatch";
   else if (tLen < 1)                     isBad = "token length < 1";
   else if (pfxLen + tLen > cred->size)   isBad = "respdata > credsize";
   else if (!(tResp->tkn[0]))             isBad = "null token";
   else if (*(tResp->tkn+(tLen-1)))       isBad = "missing null byte";

   if (isBad)
      {char eText[80];
       snprintf(eText, sizeof(eText), "'ztn' token malformed; %s", isBad);
       Fatal(erp, eText, EINVAL, false);
       return -1;
      }

// Validate the token
//
   std::string msgRC;
   long long   eTime;
   if (Entity.name) {free(Entity.name); Entity.name = 0;}
   if (sthP->Validate(tResp->tkn, msgRC, (expiry ? &eTime : 0), &Entity))
      {if (expiry)
          {if (eTime < 0 && expiry > 0)
              {Fatal(erp, "'ztn' token expiry missing", EINVAL, false);
               return -1;
              }
           if ((monotonic_time() - eTime) <= 0)
              {Fatal(erp, "'ztn' token expired", EINVAL, false);
               return -1;
              }
      }
       if (!Entity.name) Entity.name = strdup("anon");
       return 0;
      }

// Validation failed, generate message and return failure
//
// msgRC.insert(0, "ztn validation failed; ");
   Fatal(erp, msgRC.c_str(), EAUTH, false);
   return -1;
}
  
/******************************************************************************/
/* Private:                       S e n d A I                                 */
/******************************************************************************/

int XrdSecProtocolztn::SendAI(XrdOucErrInfo *erp, XrdSecParameters **parms)
{
   Fatal(erp, "Authorized issuer request not supported", ENOTSUP);
   return -1;
}
  
/******************************************************************************/
/*              I n i t i a l i z a t i o n   F u n c t i o n s               */
/******************************************************************************/
/******************************************************************************/
/*                 X r d S e c P r o t o c o l z t n I n i t                  */
/******************************************************************************/
  
extern "C"
{
char  *XrdSecProtocolztnInit(const char     mode,
                             const char    *parms,
                             XrdOucErrInfo *erp)
{
   static char nilstr = 0;
   XrdOucString accPlugin("libXrdAccSciTokens.so");
   uint64_t opts = XrdSecProtocolztn::ztnVersion;

// This only makes sense for server initialization
//
   if (mode == 'c') return &nilstr;

// If there are no parameters, return the defaults
//
   if (!parms || !(*parms))
      {char buff[256];
       if (!getLinkage(erp, accPlugin.c_str())) return 0;
       snprintf(buff, sizeof(buff), "TLS:%" PRIu64 ":%d:", opts, MaxTokSize);
       return strdup(buff);
      }

// Copy the parameters as we will need modify them
//
   std::vector<XrdOucString> useVec;
   XrdOucString    cfgParms(parms);
   XrdOucTokenizer cfg(const_cast<char *>(cfgParms.c_str()));
   char *endP, *val;

// Setup to parse parameters
//
   cfg.GetLine();

// Parse the parameters: -expiry {none|optional|required} -maxsz <num>
//                       -tokenlib <libpath>
//
   while((val = cfg.GetToken()))
        {     if (!strcmp(val, "-maxsz"))
                 {if (!(val = cfg.GetToken()))
                     {Fatal(erp, "-maxsz argument missing", EINVAL);
                      return 0;
                     }
                  MaxTokSize = strtol(val, &endP, 10);
                  if (*endP == 'k' || *endP == 'K')
                     {MaxTokSize *= 1024; endP++;}
                  if (MaxTokSize <= 0 || MaxTokSize > 524288 || *endP)
                     {Fatal(erp, "-maxsz argument is invalid", EINVAL);
                      return 0;
                     }
                 }
         else if (!strcmp(val, "-expiry"))
                 {if (!(val = cfg.GetToken()))
                     {Fatal(erp, "-expiry argument missing", EINVAL);
                      return 0;
                     }
                       if (strcmp(val, "ignore"))   expiry =  0;
                  else if (strcmp(val, "optional")) expiry = -1;
                  else if (strcmp(val, "required")) expiry =  1;
                  else {Fatal(erp, "-expiry argument invalid", EINVAL);
                        return 0;
                       }
                 }

         else if (!strcmp(val, "-tokenlib"))
                 {if (!(val = cfg.GetToken()))
                     {Fatal(erp, "-acclib plugin path missing", EINVAL);
                      return 0;
                     }
                  accPlugin = val;
                 }

         else {XrdOucString eTxt("Invalid parameter - "); eTxt += val;
               Fatal(erp, eTxt.c_str(), EINVAL);
               return 0;
              }
        }

// We rely on the token authorization plugin to validate tokens. Load it to
// get the validation object pointer. This will be filled in later but we
// want to know that it's actually present.
//
   if (!getLinkage(erp, accPlugin.c_str())) return 0;

// Assemble the parameter line and return it
//
   char buff[256];
   snprintf(buff, sizeof(buff), "TLS:%" PRIu64 ":%d:", opts, MaxTokSize);
   return strdup(buff);
}
}

/******************************************************************************/
/*               X r d S e c P r o t o c o l z t n O b j e c t                */
/******************************************************************************/
  
extern "C"
{
XrdSecProtocol *XrdSecProtocolztnObject(const char              mode,
                                        const char             *hostname,
                                              XrdNetAddrInfo   &endPoint,
                                        const char             *parms,
                                              XrdOucErrInfo    *erp)
{
   XrdSecProtocolztn *protP;

// Whether this is a client of server, the connection must be using TLS.
//
   if (!endPoint.isUsingTLS())
      {Fatal(erp,"security protocol 'ztn' disallowed for non-TLS connections.",
             ENOTSUP, false);
       return 0;
      }

// Get a protocol object appropriate for the mode
//
   if (mode == 'c')
      {bool aOK;
       protP = new XrdSecProtocolztn(parms, erp, aOK);
       if (aOK) return protP;
       delete protP;
       return 0;
      }

// In server mode we need to make sure the token plugin was actually
// loaded and initialized as we need a pointer to the helper.
//
   XrdSciTokensHelper *sthP= *sth_Linkage;
   if (!sthP)
      {char msg[1024];
       snprintf(msg,sizeof(msg),"ztn required plugin (%s) has not been loaded!",
                sth_piName);
       Fatal(erp, msg, EIDRM,false);
       return 0;
      }

// Get an authentication object and return it
//
   if (!(protP = new XrdSecProtocolztn(hostname, endPoint, sthP)))
      Fatal(erp, "insufficient memory for protocol.", ENOMEM, false);

// All done
//
   return protP;
}
}
