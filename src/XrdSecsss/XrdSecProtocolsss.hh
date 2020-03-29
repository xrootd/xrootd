#ifndef _SECPROTOCOLSSS_
#define _SECPROTOCOLSSS_
/******************************************************************************/
/*                                                                            */
/*                  X r d S e c P r o t o c o l s s s . h h                   */
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

#include "XrdCrypto/XrdCryptoLite.hh"
#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSecsss/XrdSecsssID.hh"
#include "XrdSecsss/XrdSecsssKT.hh"
#include "XrdSecsss/XrdSecsssRR.hh"

class XrdOucErrInfo;

struct XrdSecsssEnt;

class XrdSecProtocolsss : public XrdSecProtocol
{
public:
friend class XrdSecProtocolDummy; // Avoid stupid gcc warnings about destructor


        int                Authenticate  (XrdSecCredentials *cred,
                                          XrdSecParameters **parms,
                                          XrdOucErrInfo     *einfo=0);

        void               Delete();

static  int                eMsg(const char *epn, int rc, const char *txt1,
                                const char *txt2=0,      const char *txt3=0, 
                                const char *txt4=0);

static  int                Fatal(XrdOucErrInfo *erP, const char *epn, int rc,
                                                     const char *etxt);

        XrdSecCredentials *getCredentials(XrdSecParameters  *parms=0,
                                          XrdOucErrInfo     *einfo=0);

        int   Init_Client(XrdOucErrInfo *erp, const char *Parms);

        int   Init_Server(XrdOucErrInfo *erp, const char *Parms);

static  char *Load_Client(XrdOucErrInfo *erp, const char *Parms);

static  char *Load_Server(XrdOucErrInfo *erp, const char *Parms);

        XrdSecProtocolsss(const char *hname, XrdNetAddrInfo &endPoint)
                         : XrdSecProtocol("sss"),
                           keyTab(0), Crypto(0), idBuff(0), dataOpts(0),
                           Sequence(0), v2EndPnt(false)
                         {urName = strdup(hname); setIP(endPoint);}

struct Crypto {const char *cName; char cType;};

private:
       ~XrdSecProtocolsss() {} // Delete() does it all

int                Decode(XrdOucErrInfo *error, XrdSecsssKT::ktEnt &decKey,
                          char *iBuff, XrdSecsssRR_DataHdr *rrDHdr, int iSize);
XrdSecCredentials *Encode(XrdOucErrInfo *error, XrdSecsssKT::ktEnt &encKey,
                          XrdSecsssRR_Hdr *rrHdr, XrdSecsssRR_DataHdr *rrDHdr,
                          int dLen);

int            getCred(XrdOucErrInfo *, XrdSecsssRR_DataHdr *&,
                       const char    *, const char *);
int            getCred(XrdOucErrInfo *, XrdSecsssRR_DataHdr *&,
                       const char    *, const char *, XrdSecParameters *);

char          *getLID(char *buff, int blen);
static
XrdCryptoLite *Load_Crypto(XrdOucErrInfo *erp, const char *eN);
static
XrdCryptoLite *Load_Crypto(XrdOucErrInfo *erp, const char  eT);
int            myClock();
char          *setID(char *id, char **idP);
void           setIP(XrdNetAddrInfo &endPoint);

static struct Crypto  CryptoTab[];

       char          *urName;
       char           urIP[48];  // New format
       char           urIQ[48];  // Old format
static int            deltaTime;
static bool           isMutual;
static bool           isMapped;
static bool           ktFixed;
       XrdNetAddrInfo *epAddr;

static XrdSecsssKT   *ktObject;  // Both:   Default Key Table object
       XrdSecsssKT   *keyTab;    // Both:   Active  Key Table

static XrdCryptoLite *CryptObj;  // Both:   Default Cryptogrophy object
       XrdCryptoLite *Crypto;    // Both:   Active  Cryptogrophy object

static XrdSecsssID   *idMap;     // Client: Registry
static char          *aProts;    // Server: Allowable cloned auth protocols
       char          *idBuff;    // Server: Underlying buffer for XrdSecEntity
static XrdSecsssEnt  *staticID;  // Client: Static identity
       int            dataOpts;  // Client: idMap Find() options
       char           Sequence;  // Client: Check for sequencing
       bool           v2EndPnt;  // Server: Client is version 2
                                 // Client: Server is version 2
};
#endif
