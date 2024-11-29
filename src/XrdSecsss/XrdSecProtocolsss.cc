/******************************************************************************/
/*                                                                            */
/*                  X r d S e c P r o t o c o l s s s . c c                   */
/*                                                                            */
/* (c) 2008 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#if !defined(__FreeBSD__)
#include <alloca.h>
#endif
#include <cctype>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <cstdio>
#include <sys/param.h>
#include <unistd.h>

#include "XrdVersion.hh"

#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucPup.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdSecsss/XrdSecsssEnt.hh"
#include "XrdSecsss/XrdSecProtocolsss.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/
  
#define XrdsssPROTOIDENT    "sss"

#define CLDBG(x) if (sssDEBUG) std::cerr<<"sec_sss: "<<x<<'\n'<<std::flush

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
namespace
{
class Persona
{
public:
XrdSecsssKT::ktEnt *kTab;
char               *xAuth;
char               *xUser;
char               *xGrup;
char               *name;
char               *host;
char               *vorg;
char               *role;
char               *grps;
char               *caps;
char               *endo;
char               *creds;
int                 credslen;
char               *pident;

                    Persona(XrdSecsssKT::ktEnt *kP)
                           {memset(this, 0, sizeof(Persona));
                            kTab = kP;
                           }
                   ~Persona() {}

bool Clonable(const char *aTypes)
             {char aKey[XrdSecPROTOIDSIZE+2];
              if (!xAuth || !name || !pident
              ||  !(kTab->Data.Opts & XrdSecsssKT::ktEnt::allUSR)) return false;
              int n = strlen(xAuth);
              if (n < 2 || n >= XrdSecPROTOIDSIZE) return false;
              *aKey = ':';
              strcpy(aKey+1, xAuth);
              return strstr(aTypes, aKey) != 0;
             }
      };

// Struct to manage dynamically allocated data buffer
//
struct sssRR_DataHdr
{
XrdSecsssRR_DataHdr *P;

                     sssRR_DataHdr() : P(0) {}
                    ~sssRR_DataHdr() {if (P) free(P);}
};
}

/******************************************************************************/
/*                           S t a t i c   D a t a                            */
/******************************************************************************/

XrdCryptoLite *XrdSecProtocolsss::CryptObj   = 0;
XrdSecsssKT   *XrdSecProtocolsss::ktObject   = 0;
XrdSecsssID   *XrdSecProtocolsss::idMap      = 0;
char          *XrdSecProtocolsss::aProts     = 0;
XrdSecsssEnt  *XrdSecProtocolsss::staticID   = 0;
int            XrdSecProtocolsss::deltaTime  =13;
bool           XrdSecProtocolsss::isMutual   = false;
bool           XrdSecProtocolsss::isMapped   = false;
bool           XrdSecProtocolsss::ktFixed    = false;

struct XrdSecProtocolsss::Crypto XrdSecProtocolsss::CryptoTab[] = {
       {"bf32", XrdSecsssRR_Hdr::etBFish32},
       {0, '0'}
       };

namespace
{
XrdSysMutex initMutex;

bool sssDEBUG = false;
bool sssUseKN = false;
}
  
/******************************************************************************/
/*                          A u t h e n t i c a t e                           */
/******************************************************************************/

int XrdSecProtocolsss::Authenticate(XrdSecCredentials *cred,
                                    XrdSecParameters **parms,
                                    XrdOucErrInfo     *einfo)
{
   static const int minLen = sizeof(XrdSecsssRR_Hdr) + XrdSecsssRR_Data_HdrLen;
   static const int maxLen = XrdSecsssRR_Data::MaxDSz + minLen;
   static const int Special= XrdSecsssKT::ktEnt::anyUSR
                           | XrdSecsssKT::ktEnt::allUSR;

   XrdSecsssRR_Hdr    *rrHdr = (XrdSecsssRR_Hdr *)(cred->buffer);
   XrdSecsssRR_Data   *rrData;
   XrdSecsssKT::ktEnt  decKey;
   Persona             myID(&decKey);

   char *idP, *dP, *eodP, *theIP = 0, *theHost = 0, *atKey = 0, eType;
   int idNum = 0, idTLen, idSz, dLen;
   bool badAttr = false;

// Make sure we have atleast the header plus the data header
//
   if (cred->size < minLen)
      return Fatal(einfo, "Auth", EINVAL, "Credentials too small.");

// Make sure the credentials are not too big (people misuse sss)
//
   if (cred->size > maxLen)
      return Fatal(einfo, "Auth", EINVAL, "Credentials too big.");

// Allocate the buffer from the stack
//
   rrData = (XrdSecsssRR_Data *)alloca(cred->size);

// Decode the credentials
//
   if ((dLen = Decode(einfo, decKey, cred->buffer, rrData, cred->size)) <= 0)
      return -1;

// Check if we should echo back the LID
//
   if (rrData->Options == XrdSecsssRR_DataHdr::SndLID)
      {XrdSecsssRR_DataResp rrResp;
       char lidBuff[16];
       rrResp.Options = 0;
       getLID(lidBuff, sizeof(lidBuff));
       dP = rrResp.Data;
       *dP++ = XrdSecsssRR_Data::theLgid;
       XrdOucPup::Pack(&dP, lidBuff);
       int n =  dP-rrResp.Data +  XrdSecsssRR_Data_HdrLen;
       *parms = Encode(einfo, decKey, rrHdr, &rrResp, n);
       return (*parms ? 1 : -1);
      }

// Extract out the entity information
//
   dP = rrData->Data; eodP = dP + dLen - XrdSecsssRR_Data_HdrLen;
   CLDBG("Processing " <<dLen <<" byes");
   while(dP < eodP)
        {eType = *dP++;
         CLDBG("eType=" <<static_cast<int>(eType)
               <<" Used " <<dP-rrData->Data <<" left " <<eodP-dP);
         if (!XrdOucPup::Unpack(&dP, eodP, &idP, idSz) || (idP && *idP == '\0'))
            {Fatal(einfo, "Authenticate", EINVAL, "Invalid id string.");
             return -1;
            }
         idNum++;
         switch(eType)
               {case XrdSecsssRR_Data::theName: myID.name     = idP; break;
                case XrdSecsssRR_Data::theVorg: myID.vorg     = idP; break;
                case XrdSecsssRR_Data::theRole: myID.role     = idP; break;
                case XrdSecsssRR_Data::theGrps: myID.grps     = idP; break;
                case XrdSecsssRR_Data::theEndo: myID.endo     = idP; break;
                case XrdSecsssRR_Data::theCred: myID.creds    = idP;
                                                myID.credslen = idSz;break;
                case XrdSecsssRR_Data::theHost: 
                                     if (*idP == '[')
                                        myID.host =   theIP   = idP;

                                        else          theHost = idP;
                                                                     break;
                case XrdSecsssRR_Data::theRand: idNum--;             break;

                case XrdSecsssRR_Data::theAuth: myID.xAuth    = idP; break;

                case XrdSecsssRR_Data::theTID:  myID.pident   = idP; break;
                case XrdSecsssRR_Data::theAKey: if (atKey) badAttr = true;
                                                atKey         = idP; break;
                case XrdSecsssRR_Data::theAVal:
                     if (!atKey) badAttr = true;
                        else {Entity.eaAPI->Add(std::string(atKey),
                                         std::string(idP), true);
                              atKey = 0;
                             }
                     break;
                case XrdSecsssRR_Data::theUser: myID.xUser    = idP; break;
                case XrdSecsssRR_Data::theGrup: myID.xGrup    = idP; break;
                case XrdSecsssRR_Data::theCaps: myID.caps     = idP; break;
                default: break;
               }
        }

// Verify that we have some kind of identification
//
   if (!idNum)
      {Fatal(einfo, "Authenticate", ENOENT, "No identification specified.");
       return -1;
      }

// Make sure we didn't encounter any attribute errors
//
   if (badAttr)
      {Fatal(einfo, "Authenticate", EINVAL, "Invalid attribute specification.");
       return -1;
      }

// Verify the source of the information to largely prevent packet stealing. New
// version of the protocol will send an IP address which we prefrentially use.
// Older version used a hostname. This causes problems for multi-homed machines.
//
if (!(decKey.Data.Opts & XrdSecsssKT::ktEnt::noIPCK))
  {if (!theHost && !theIP)
      {Fatal(einfo,"Authenticate",ENOENT,"No hostname or IP address specified.");
       return -1;
      }
   CLDBG(urName <<' ' <<urIP <<" or " <<urIQ << " must match " 
         <<(theHost ? theHost : "?") <<' ' <<(theIP ? theIP : "[?]"));
   if (theIP)
      {if (strcmp(theIP, urIP) && strcmp(theIP, urIQ))
          {Fatal(einfo, "Authenticate", EINVAL, "IP address mismatch.");
           return -1;
          }
      } else if (strcmp(theHost, urName))
          {Fatal(einfo, "Authenticate", EINVAL, "Hostname mismatch.");
           return -1;
          }
  } else {
   CLDBG(urName <<' ' <<urIP <<" or " <<urIQ << " forwarded token from "
         <<(theHost ? theHost : "?") <<' ' <<(theIP ? theIP : "[?]"));
  }

// At this point we need to check if this identity can be passed as a clone
//
   if (aProts && myID.Clonable(aProts))
      {strlcpy(Entity.prot, myID.xAuth, sizeof(Entity.prot));
       Entity.prot[XrdSecPROTOIDSIZE-1] = 0;
       if (myID.xUser) XrdOucUtils::getUID(myID.xUser,Entity.uid,&Entity.gid);
       if (myID.xGrup) XrdOucUtils::getGID(myID.xGrup,Entity.gid);
      } else {
       // Set correct username
       //
               if (decKey.Data.Opts & Special)
                  {if (!myID.name) myID.name = (char *)"nobody";}
          else myID.name = decKey.Data.User;

       // Set correct group
       //
                if (decKey.Data.Opts & XrdSecsssKT::ktEnt::usrGRP) myID.grps = 0;
          else {if (decKey.Data.Opts & XrdSecsssKT::ktEnt::anyGRP)
                   {if (!myID.grps) myID.grps = (char *)"nogroup";}
                   else myID.grps = decKey.Data.Grup;
               }

       // Set corresponding uid and gid
       //
          if (myID.name) XrdOucUtils::getUID(myID.name, Entity.uid, &Entity.gid);
          if (myID.grps) XrdOucUtils::getGID(myID.grps, Entity.gid);
       }

// Calculate the amount of space we will need
//
   idTLen = strlen(urName)
          + (myID.name   ? strlen(myID.name)+1   : 0)
          + (myID.vorg   ? strlen(myID.vorg)+1   : 0)
          + (myID.role   ? strlen(myID.role)+1   : 0)
          + (myID.grps   ? strlen(myID.grps)+1   : 0)
          + (myID.caps   ? strlen(myID.caps)+1   : 0)
          + (myID.endo   ? strlen(myID.endo)+1   : 0)
          + (myID.creds  ? myID.credslen         : 0)
          + (myID.pident ? strlen(myID.pident)+1 : 0);

// Complete constructing our identification
//
   if (idBuff) free(idBuff);
   idBuff = idP = (char *)malloc(idTLen);
   Entity.host         = urName;
   Entity.name         = setID(myID.name, &idP);
   Entity.vorg         = setID(myID.vorg, &idP);
   Entity.role         = setID(myID.role, &idP);
   Entity.grps         = setID(myID.grps, &idP);
   Entity.caps         = setID(myID.caps, &idP);
   Entity.endorsements = setID(myID.endo, &idP);

   if (myID.pident)
      {strcpy(idP, myID.pident);
       Entity.pident = idP;
       idP += strlen(myID.pident) + 1;
      }

   if (myID.creds)
      {memcpy(idP, myID.creds, myID.credslen);
       Entity.creds = idP;
       Entity.credslen = myID.credslen;
      }

// All done
//
   return 0;
}
  
/******************************************************************************/
/* Private:                       D e c o d e                                 */
/******************************************************************************/

int                XrdSecProtocolsss::Decode(XrdOucErrInfo       *error,
                                             XrdSecsssKT::ktEnt  &decKey,
                                             char                *iBuff,
                                             XrdSecsssRR_DataHdr *rrDHdr,
                                             int                  iSize)
{
   XrdSecsssRR_Hdr  *rrHdr  = (XrdSecsssRR_Hdr  *)iBuff;
   char *iData = iBuff+sizeof(XrdSecsssRR_Hdr);
   int rc, genTime, dLen = iSize - sizeof(XrdSecsssRR_Hdr);

// Check if this is a recognized protocol
//
   if (strcmp(rrHdr->ProtID, XrdsssPROTOIDENT))
      {char emsg[256];
       snprintf(emsg, sizeof(emsg),
                "Authentication protocol id mismatch (%.4s != %.4s).",
                XrdsssPROTOIDENT,  rrHdr->ProtID);
       return Fatal(error, "Decode", EINVAL, emsg);
      }

// Verify decryption method
//
   if (rrHdr->EncType != Crypto->Type())
      return Fatal(error, "Decode", ENOTSUP, "Crypto type not supported.");

// Check if this is a V2 client. V2 client always supply the keyname of the
// key which we may or may not use. If specified, make sure it's correct.
//
   if (rrHdr->knSize)
      {int knSize = static_cast<int>(rrHdr->knSize);
       v2EndPnt = true;
       if (knSize > XrdSecsssKT::ktEnt::NameSZ || knSize & 0x07
       ||  knSize >= dLen || iData[knSize-1])
          return Fatal(error, "Decode", EINVAL, "Invalid keyname specified.");
       if (sssUseKN) strcpy(decKey.Data.Name, iData);
          else decKey.Data.Name[0] = '\0';
       CLDBG("V2 client using keyname '" <<iData <<"' dLen=" <<dLen
             <<(sssUseKN ? "" : " (ignored)"));
       iData += knSize; dLen -= knSize;
      } else decKey.Data.Name[0] = '\0';

// Get the key ID
//
   decKey.Data.ID = ntohll(rrHdr->KeyID);
   if (keyTab->getKey(decKey, *decKey.Data.Name))
      return Fatal(error, "Decode", ENOENT, "Decryption key not found.");

// Decrypt
//
   CLDBG("Decode keyid: " <<decKey.Data.ID <<" bytes " <<dLen);
   if ((rc = Crypto->Decrypt(decKey.Data.Val, decKey.Data.Len, iData, dLen,
                             (char *)rrDHdr, dLen)) <= 0)
      return Fatal(error, "Decode", -rc, "Unable to decrypt credentials.");

// Verify that the packet has not expired (OK to do before CRC check)
//
   genTime = ntohl(rrDHdr->GenTime);
   if (genTime + deltaTime <= myClock())
      return Fatal(error, "Decode", ESTALE,
                   "Credentials expired (check for clock skew).");

// Return success (size of decrypted info)
//
   return rc;
}

/******************************************************************************/
/*                                D e l e t e                                 */
/******************************************************************************/
  
void XrdSecProtocolsss::Delete()
{
// Delete things that get re-allocated every time. The staticID is allocated
// only once so it must stick around for every instance of this object.
//
     if (urName)              free(urName); // Same pointer as Entity.host
     if (idBuff)              free(idBuff);
     if (Crypto && Crypto != CryptObj) delete Crypto;
     if (keyTab && keyTab != ktObject) delete keyTab;

     delete this;
}

/******************************************************************************/
/* Private:                         e M s g                                   */
/******************************************************************************/

int XrdSecProtocolsss::eMsg(const char *epname, int rc,
                            const char *txt1, const char *txt2,
                            const char *txt3, const char *txt4)
{
              std::cerr <<"Secsss (" << epname <<"): ";
              std::cerr <<txt1;
   if (rc>0)  std::cerr <<"; " <<XrdSysE2T(rc);
   if (txt2)  std::cerr <<txt2;
   if (txt3)  std::cerr <<txt3;
   if (txt4) {std::cerr <<txt4;}
              std::cerr <<"\n" <<std::flush;

   return (rc ? (rc < 0 ? rc : -rc) : -1);
}
  
/******************************************************************************/
/* Private:                       E n c o d e                                 */
/******************************************************************************/

XrdSecCredentials *XrdSecProtocolsss::Encode(XrdOucErrInfo       *einfo,
                                             XrdSecsssKT::ktEnt  &encKey,
                                             XrdSecsssRR_Hdr     *rrHdr,
                                             XrdSecsssRR_DataHdr *rrDHdr,
                                             int                  dLen)
{
   char *credP;
   int knum, cLen, hdrSZ = sizeof(XrdSecsssRR_Hdr) + rrHdr->knSize;

// Make sure we don't overrun a 1 server's buffer. V2 servers are forgiving.
//
   if (!v2EndPnt && dLen > (int)sizeof(XrdSecsssRR_Data))
      {Fatal(einfo,"Encode",ENOBUFS,"Insufficient buffer space for credentials.");
       return (XrdSecCredentials *)0;
      }

// Complete the packet
//
   XrdSecsssKT::genKey(rrDHdr->Rand, sizeof(rrDHdr->Rand));
   rrDHdr->GenTime = htonl(myClock());
   memset(rrDHdr->Pad, 0, sizeof(rrDHdr->Pad));

// Allocate an output buffer
//
   cLen = hdrSZ + dLen + Crypto->Overhead();
   if (!(credP = (char *)malloc(cLen)))
      {Fatal(einfo, "Encode", ENOMEM, "Insufficient memory for credentials.");
       return (XrdSecCredentials *)0;
      }

// Copy the header and encrypt the data
//
   memcpy(credP, (const void *)rrHdr, hdrSZ);
   CLDBG("Encode keyid: " <<encKey.Data.ID <<" bytes " <<cLen-hdrSZ);
   if ((dLen = Crypto->Encrypt(encKey.Data.Val, encKey.Data.Len, (char *)rrDHdr,
                               dLen, credP+hdrSZ, cLen-hdrSZ)) <= 0)
      {Fatal(einfo, "Encode", -dLen, "Unable to encrypt credentials.");
       return (XrdSecCredentials *)0;
      }

// Return new credentials
//
   dLen += hdrSZ; knum = encKey.Data.ID&0x7fffffff;
   CLDBG("Ret " <<dLen <<" bytes of credentials; k=" <<knum);
   return new XrdSecCredentials(credP, dLen);
}

/******************************************************************************/
/* Private:                        F a t a l                                  */
/******************************************************************************/

int XrdSecProtocolsss::Fatal(XrdOucErrInfo *erP, const char *epn, int rc,
                                                 const char *etxt)
{
   if (erP) {erP->setErrInfo(rc, etxt);
             CLDBG(epn <<": " <<etxt);
            }
      else  eMsg(epn, rc, etxt);
   return 0;
}
  
/******************************************************************************/
/* Private:                      g e t C r e d                                */
/******************************************************************************/

int XrdSecProtocolsss::getCred(XrdOucErrInfo *einfo, XrdSecsssRR_DataHdr *&dP,
                               const char *myUrlID, const char *myIP)
{
   int dLen;

// Indicate we have been here
//
   Sequence = 1;

// For mutual authentication, the server needs to first send back a handshake.
//
   if (isMutual)
      {dP = (XrdSecsssRR_DataHdr *)malloc(XrdSecsssRR_Data_HdrLen);
       dP->Options = XrdSecsssRR_DataHdr::SndLID;
       return XrdSecsssRR_Data_HdrLen;
      }

// Otherwise we use a static ID or a mapped id. Note that we disallow sending
// credentials unless mutual authentication occurs.
//
   if (myUrlID && idMap)
      {if ((dLen = idMap->Find(myUrlID, (char *&)dP, myIP, dataOpts)) <= 0)
          return Fatal(einfo, "getCred", ESRCH, "No loginid mapping.");
      } else {
       int theOpts = dataOpts & ~XrdSecsssEnt::addCreds;
       dLen = staticID->RR_Data((char *&)dP, myIP, theOpts);
      }

// Return response length
//
   dP->Options = XrdSecsssRR_DataHdr::UseData;
   return dLen;
}

/******************************************************************************/

int XrdSecProtocolsss::getCred(XrdOucErrInfo       *einfo,
                               XrdSecsssRR_DataHdr *&dP,
                               const char          *myUrlID,
                               const char          *myIP,
                               XrdSecParameters    *parm)
{
   XrdSecsssKT::ktEnt  decKey;
   XrdSecsssRR_Data    prData;
   char *lidP = 0, *bP, *idP, *eodP, idType;
   int idSz, dLen, theOpts;

// Make sure we can decode this and not overrun our buffer
//
   if (parm->size > (int)sizeof(prData.Data))
      return Fatal(einfo, "getCred", EINVAL, "Invalid server response size.");

// Decode the credentials
//
   if ((dLen = Decode(einfo, decKey, parm->buffer, &prData, parm->size)) <= 0)
      return Fatal(einfo, "getCred", EINVAL, "Unable to decode server response.");

// Extract out the loginid. This messy code is for backwards compatibility.
//
   bP = prData.Data; eodP = dLen + (char *)&prData;
   while(bP < eodP)
        {idType = *bP++;
         if (!XrdOucPup::Unpack(&bP, eodP, &idP, idSz) || !idP || *idP == 0)
            return Fatal(einfo, "getCred", EINVAL, "Invalid id string.");
         switch(idType)
               {case XrdSecsssRR_Data::theLgid: lidP = idP; break;
                case XrdSecsssRR_Data::theHost:             break;
                case XrdSecsssRR_Data::theRand:             break;
                default: return Fatal(einfo,"getCred",EINVAL,"Invalid id type.");
               }
        }

// Verify that we have the loginid
//
   if (!lidP) return Fatal(einfo, "getCred", ENOENT, "No loginid returned.");

// Try to map the id appropriately
//
   if (!idMap) return staticID->RR_Data((char *&)dP, myIP, dataOpts);

// Map the loginid. We disallow sending credentials unless the key allows it.
//
   if (!myUrlID) myUrlID = lidP;
   if (!(decKey.Data.Opts & XrdSecsssKT::ktEnt::allUSR))
      theOpts = dataOpts & ~XrdSecsssEnt::addCreds;
      else theOpts = dataOpts;
   if ((dLen = idMap->Find(lidP, (char *&)dP, myIP, theOpts)) <= 0)
      return Fatal(einfo, "getCred", ESRCH, "No loginid mapping.");

// All done
//
   dP->Options = XrdSecsssRR_DataHdr::UseData;
   return dLen;
}

/******************************************************************************/
/*                        g e t C r e d e n t i a l s                         */
/******************************************************************************/

XrdSecCredentials *XrdSecProtocolsss::getCredentials(XrdSecParameters *parms,
                                                      XrdOucErrInfo   *einfo)
{
   static const int   nOpts = XrdNetUtils::oldFmt;
   XrdSecsssRR_Hdr2   rrHdr;
   sssRR_DataHdr      rrDataHdr;
   XrdSecsssKT::ktEnt encKey;
   XrdOucEnv         *errEnv;

   const char *myIP = 0, *myUD = 0;
   char ipBuff[64];
   int dLen;

// Make sure we can extract out required information and get it as needed
//
   if (einfo && (errEnv=einfo->getEnv()))
      {if (isMapped) myUD = errEnv->Get("username");
       if (!(myIP=errEnv->Get("sockname")))
          {int fd = epAddr->SockFD();
           if (fd > 0 && XrdNetUtils::IPFormat(-fd,ipBuff,sizeof(ipBuff),nOpts))
              myIP = ipBuff;
              else myIP = 0;
          }
       }

// Do some debugging here
//
   CLDBG("getCreds: " <<static_cast<int>(Sequence)
                      << " ud: '" <<(myUD ? myUD : "")
                      <<"' ip: '" <<(myIP ? myIP : "") <<"'");

// Get the actual data portion
//
   if (Sequence) dLen = getCred(einfo, rrDataHdr.P, myUD, myIP, parms);
      else       dLen = getCred(einfo, rrDataHdr.P, myUD, myIP);
   if (!dLen) return (XrdSecCredentials *)0;

// Get an encryption key
//
   if (keyTab->getKey(encKey))
      {Fatal(einfo, "getCredentials", ENOENT, "Encryption key not found.");
       return (XrdSecCredentials *)0;
      }

// Fill out the header
//
   strcpy(rrHdr.ProtID, XrdsssPROTOIDENT);
   memset(rrHdr.Pad, 0, sizeof(rrHdr.Pad));
   rrHdr.KeyID = htonll(encKey.Data.ID);
   rrHdr.EncType = Crypto->Type();

// Determine if we should send the keyname (v2 servers only)
//
   if (v2EndPnt)
      {int k = strlen(encKey.Data.Name), n = (k + 8) & ~7;
       strcpy(rrHdr.keyName, encKey.Data.Name);
       if (n - k > 1) memset(rrHdr.keyName + k, 0, n - k);
       rrHdr.knSize = static_cast<uint8_t>(n);
      } else rrHdr.knSize = 0;

// Now simply encode the data and return the result
//
   return Encode(einfo, encKey, &rrHdr, rrDataHdr.P, dLen);
}

/******************************************************************************/
/* Private:                       g e t L I D                                 */
/******************************************************************************/
  
char *XrdSecProtocolsss::getLID(char *buff, int blen)
{
   const char *dot;

// Extract out the loginid from the trace id
//
   if (!Entity.tident 
   ||  !(dot = index(Entity.tident,'.'))
   ||  dot == Entity.tident
   ||  dot >= (Entity.tident+blen)) strcpy(buff,"nobody");
      else {int idsz = dot - Entity.tident;
            strncpy(buff, Entity.tident, idsz);
            *(buff+idsz) = '\0';
           }

// All done
//
   return buff;
}

/******************************************************************************/
/*                           I n i t _ C l i e n t                            */
/******************************************************************************/

int XrdSecProtocolsss::Init_Client(XrdOucErrInfo *erp, const char *pP)
{
   XrdSysMutexHelper initMon(&initMutex);
   XrdSecsssKT *ktP;
   struct stat buf;
   char *Colon;
   int lifeTime;

// We must have <enccode>.[+]<lifetime>:<keytab>
//
   if (!pP || !*pP) return Fatal(erp, "Init_Client", EINVAL,
                                 "Client parameters missing.");

// Get encryption object
//
   if (!*pP || *(pP+1) != '.') return Fatal(erp, "Init_Client", EINVAL,
                                 "Encryption type missing.");
   if (!(Crypto = Load_Crypto(erp, *pP))) return 0;
   pP += 2;

// Check if this is a v2 server and if credentials are to be sent
//
   if (*pP == '+')
      {v2EndPnt = true;
       dataOpts |= XrdSecsssEnt::addExtra;
       if (*(pP+1) == '0') dataOpts |= XrdSecsssEnt::addCreds;
      }

// The next item is the cred lifetime
//
   lifeTime = strtol(pP, &Colon, 10);
   if (!lifeTime || *Colon != ':') return Fatal(erp, "Init_Client", EINVAL,
                                          "Credential lifetime missing.");
   deltaTime = lifeTime; pP = Colon+1;

// Get the correct keytab
//
        if (ktFixed || (ktObject && ktObject->Same(pP))) keyTab = ktObject;
   else if (*pP == '/' && !stat(pP, &buf))
           {if (!(ktP=new XrdSecsssKT(erp,pP,XrdSecsssKT::isClient,3600)))
               return Fatal(erp, "Init_Client", ENOMEM,
                                 "Unable to create keytab object.");
            if (erp->getErrInfo()) {delete ktP; return 0;}
            if (!ktObject) ktObject = ktP;
            keyTab = ktP;
            CLDBG("Client keytab='" <<pP <<"'");
           } else keyTab = ktObject;

   if (!keyTab)
      return Fatal(erp, "Init_Client", ENOENT, 
                        "Unable to determine keytab location.");

// All done
//
   return 1;
}

/******************************************************************************/
/*                           I n i t _ S e r v e r                            */
/******************************************************************************/

int XrdSecProtocolsss::Init_Server(XrdOucErrInfo *erp, const char *pP)
{

// This is a trivial init
//
   keyTab = ktObject;
   Crypto = CryptObj;
   return 1;
}

/******************************************************************************/
/*                           L o a d _ C l i e n t                            */
/******************************************************************************/
  
char *XrdSecProtocolsss::Load_Client(XrdOucErrInfo *erp, const char *parms)
{
   static const char *KTPath = XrdSecsssKT::genFN();
   static const int   rfrHR = 60*60;
   struct stat buf;
   XrdSecsssID::authType aType = XrdSecsssID::idStatic;
   const char *kP = 0;
   char *myName;

// Get our full host name
//
   if (!(myName = XrdNetUtils::MyHostName(0)))
      {Fatal(erp, "Load_Client", ENOENT, "Unable to obtain local hostname.");
       return (char *)0;
      }

// Tell the entity serialization object who we are
//
   XrdSecsssEnt::setHostName(myName);
   free(myName);

// Check for the presence of a registry object
//
   idMap = XrdSecsssID::getObj(aType, staticID);
   switch(aType)
         {case XrdSecsssID::idDynamic:  isMutual = true; break;
          case XrdSecsssID::idStaticM:  isMutual = true;
                                        idMap    = 0;    break;
          case XrdSecsssID::idStatic:   idMap    = 0;    break;
          case XrdSecsssID::idMapped:   isMapped = true; break;
          case XrdSecsssID::idMappedM:  isMapped = true; break;
               default:                 idMap    = 0;    break;
          }

// We want to establish the default location of the keytable. First check
// the environment passed from the client then the envar. We support two
// version of the envar for backward compatibility due to an early mistake.
//
   if( erp && erp->getEnv() && ( kP = erp->getEnv()->Get( "xrd.sss" ) ) )
     ktFixed = true;
   else if ( ( (kP = getenv("XrdSecSSSKT")) || (kP = getenv("XrdSecsssKT")) )
             &&  *kP && !stat(kP, &buf))
     ktFixed = true;
   else kP = 0;

   if (!kP && !stat(KTPath, &buf)) kP = KTPath;

// Build the keytable if we actual have a path (if none, then the server
// will have to supply the path)
//
   if (kP)
      {if (!(ktObject=new XrdSecsssKT(erp,kP,XrdSecsssKT::isClient,rfrHR)))
          {Fatal(erp, "Load_Client", ENOMEM, "Unable to create keytab object.");
           return (char *)0;
          }
       if (erp->getErrInfo())
          {delete ktObject, ktObject = 0; return (char *)0;}
       CLDBG("Client keytab='" <<kP <<"'");
      }

// All done
//
   return (char *)"";
}
  
/******************************************************************************/
/* Private:                  L o a d _ C r y p t o                            */
/******************************************************************************/
  
XrdCryptoLite *XrdSecProtocolsss::Load_Crypto(XrdOucErrInfo *erp,
                                              const char    *eN)
{
   XrdCryptoLite *cP;
   char buff[128];
   int rc, i = 0;

// Find correct crypto object
//
   while(CryptoTab[i].cName && strcmp(CryptoTab[i].cName, eN)) i++;

// If we didn't find it, complain
//
   if (!CryptoTab[i].cName)
      {sprintf(buff, "Secsss: %s cryptography not supported.", eN);
       Fatal(erp, "Load_Crypto", EINVAL, buff);
       return (XrdCryptoLite *)0;
      }

// Return load result
//
   if ((cP = XrdCryptoLite::Create(rc, eN, CryptoTab[i].cType))) return cP;
   sprintf(buff,"Secsss: %s cryptography load failed; %s",eN,XrdSysE2T(rc));
   Fatal(erp, "Load_Crypto", EINVAL, buff);
   return (XrdCryptoLite *)0;
}

/******************************************************************************/
  
XrdCryptoLite *XrdSecProtocolsss::Load_Crypto(XrdOucErrInfo *erp,
                                              const char     eT)
{
   XrdCryptoLite *cP;
   char buff[128];
   int rc, i = 0;

// Check if we can use the satic object
//
   if (CryptObj && eT == CryptObj->Type()) return CryptObj;

// Find correct crypto object
//
   while(CryptoTab[i].cName && CryptoTab[i].cType != eT) i++;

// If we didn't find it, complain
//
   if (!CryptoTab[i].cName)
      {sprintf(buff, "Secsss: 0x%hhx cryptography not supported.", eT);
       Fatal(erp, "Load_Crypto", EINVAL, buff);
       return (XrdCryptoLite *)0;
      }

// Return load result
//
   if ((cP = XrdCryptoLite::Create(rc, CryptoTab[i].cName, eT))) return cP;
   sprintf(buff,"Secsss: 0x%hhx cryptography load failed; %s",eT,XrdSysE2T(rc));
   Fatal(erp, "Load_Crypto", EINVAL, buff);
   return (XrdCryptoLite *)0;
}

/******************************************************************************/
/*                           L o a d _ S e r v e r                            */
/******************************************************************************/
  
char *XrdSecProtocolsss::Load_Server(XrdOucErrInfo *erp, const char *parms)
{
   const char *msg = 0;
   const char *encName = "bf32", *ktClient = "", *ktServer = 0;
   char buff[2048], parmbuff[2048], *op, *od, *eP;
   int lifeTime = 13, rfrTime = 60*60;
   XrdOucTokenizer inParms(parmbuff);
   const char *ask4Creds = "";

// Duplicate the parms
//
   if (parms) strlcpy(parmbuff, parms, sizeof(parmbuff));

// Expected parameters: [{-c | --clientkt} <ckt_path>]
//                      [{-e | --encrypt}  <enctype>]
//                      [{-g | --getcreds}]
//                      [{-k | --keyname}]
//                      [{-l | --lifetime} <seconds>] 
//                      [{-p | --proxy}  <prots>]
//                      [{-r | --refresh}  <minutes>]
//                      [{-s | --serverkt} <skt_path>]
//
   if (parms && inParms.GetLine())
      while((op = inParms.GetToken()))
           {if (!strcmp("-k", op) || !strcmp("--keyname", op))
               {sssUseKN = true;
                continue;
               }
            if (!strcmp("-g", op) || !strcmp("--getcreds", op))
               {ask4Creds = "0";
                continue;
               }
            if (!(od = inParms.GetToken()))
               {sprintf(buff,"Secsss: Missing %s parameter argument",op);
                msg = buff; break;
               }
                 if (!strcmp("-c", op) || !strcmp("--clientkt", op))
                    ktClient = od;
            else if (!strcmp("-e", op) || !strcmp("--encrypt", op))
                    encName  = od;
            else if (!strcmp("-l", op) || !strcmp("--lifetime", op))
                    {lifeTime = strtol(od, &eP, 10) * 60;
                     if (errno || *eP || lifeTime < 1)
                        {msg = "Secsss: Invalid life time"; break;}
                    }
            else if (!strcmp("-p", op) || !strcmp("--proxy", op))
                    {int n = strlen(od) + 2;
                     aProts = (char *)malloc(n);
                     *aProts = ':';
                     strcpy(aProts+1, od);
                    }
            else if (!strcmp("-r", op) || !strcmp("--rfresh", op))
                    {rfrTime = strtol(od, &eP, 10) * 60;
                     if (errno || *eP || rfrTime < 600)
                        {msg = "Secsss: Invalid refresh time"; break;}
                    }
            else if (!strcmp("-s", op) || !strcmp("-serverkt", op))
                    ktServer = od;
            else {sprintf(buff,"Secsss: Invalid parameter - %s",op);
                  msg = buff; break;
                 }
           }

// Check for errors
//
   if (msg) {Fatal(erp, "Load_Server", EINVAL, msg); return (char *)0;}

// Load the right crypto object
//
   if (!(CryptObj = Load_Crypto(erp, encName))) return (char *)0;

// Supply default keytab location if not specified
//
   if (!ktServer) ktServer = XrdSecsssKT::genFN();

// Set the delta time used to expire credentials
//
   deltaTime = lifeTime;

// Create a keytab object (only one for the server)
//
   if (!(ktObject = new XrdSecsssKT(erp, ktServer, XrdSecsssKT::isServer,
                                         rfrTime)))
      {Fatal(erp, "Load_Server", ENOMEM, "Unable to create keytab object.");
       return (char *)0;
      }
   if (erp->getErrInfo()) return (char *)0;
   ktFixed = true;
   CLDBG("Server keytab='" <<ktServer <<"'");

// Construct client parameter <enccode>.+<lifetime>:<keytab>
// Note: The plus preceding the <lifetime> indicates that we are a V2 server.
// V1 clients will simply ignore this and treat us as a V1 server.
//
   sprintf(buff, "%c.+%s%d:%s", CryptObj->Type(),ask4Creds,lifeTime,ktClient);
   CLDBG("client parms='" <<buff <<"'");
   return strdup(buff);
}

/******************************************************************************/
/*                               m y C l o c k                                */
/******************************************************************************/
  
int XrdSecProtocolsss::myClock()
{
   static const time_t baseTime = 1222183880;

   return static_cast<int>(time(0)-baseTime);
}

/******************************************************************************/
/*                                 s e t I D                                  */
/******************************************************************************/
  
char *XrdSecProtocolsss::setID(char *id, char **idP)
{
   if (id)
      {int n = strlen(id);
       strcpy(*idP, id); id = *idP; *idP = *idP + n + 1;
      }
   return id;
}

/******************************************************************************/
/*                                 s e t I P                                  */
/******************************************************************************/
  
void XrdSecProtocolsss::setIP(XrdNetAddrInfo &endPoint)
{
   if (!endPoint.Format(urIP, sizeof(urIP), XrdNetAddrInfo::fmtAdv6))  *urIP=0;
   if (!endPoint.Format(urIQ, sizeof(urIQ), XrdNetAddrInfo::fmtAdv6,
                                            XrdNetAddrInfo::old6Map4)) *urIQ=0;
   Entity.addrInfo = epAddr = &endPoint;
}
  
/******************************************************************************/
/*                 X r d S e c P r o t o c o l s s s I n i t                  */
/******************************************************************************/
  
extern "C"
{
char  *XrdSecProtocolsssInit(const char     mode,
                             const char    *parms,
                             XrdOucErrInfo *erp)
{

// Set debug option
//
   if (getenv("XrdSecDEBUG")) sssDEBUG = true;

// Perform load-time initialization
//
   return (mode == 'c' ? XrdSecProtocolsss::Load_Client(erp, parms)
                       : XrdSecProtocolsss::Load_Server(erp, parms));
}
}

/******************************************************************************/
/*               X r d S e c P r o t o c o l s s s O b j e c t                */
/******************************************************************************/

XrdVERSIONINFO(XrdSecProtocolsssObject,secsss);
  
extern "C"
{
XrdSecProtocol *XrdSecProtocolsssObject(const char              mode,
                                        const char             *hostname,
                                              XrdNetAddrInfo   &endPoint,
                                        const char             *parms,
                                              XrdOucErrInfo    *erp)
{
   XrdSecProtocolsss *prot;
   int Ok;

// Get a new protocol object
//
   if (!(prot = new XrdSecProtocolsss(endPoint.Name(hostname), endPoint)))
      XrdSecProtocolsss::Fatal(erp, "sss_Object", ENOMEM,
                         "Secsss: Insufficient memory for protocol.");
      else {Ok = (mode == 'c' ? prot->Init_Client(erp, parms)
                              : prot->Init_Server(erp, parms));

            if (!Ok) {prot->Delete(); prot = 0;}
           }

// All done
//
   return (XrdSecProtocol *)prot;
}
}
