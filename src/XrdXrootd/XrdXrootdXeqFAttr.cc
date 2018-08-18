/******************************************************************************/
/*                                                                            */
/*                  X r d X r o o t d X e q F A t t r . c c                   */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/uio.h>

#include "XProtocol/XProtocol.hh"
#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSfs/XrdSfsFAttr.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdMonData.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdXeq.hh"
#include "XrdXrootd/XrdXrootdXPath.hh"

/******************************************************************************/
/*                      L o c a l   S t r u c t u r e s                       */
/******************************************************************************/

namespace
{
struct faCTL
{
XrdSfsFAInfo *info;   // Pointer to attribute information
char         *buff;   // Buffer to be decoded
char         *bend;   // Pointer to last byte of buffer + 1
int           vnsz;   // Size of variable name segment
short         iNum;   // Number of info entries
short         iEnd;   // Index number of last entry processed
bool          verr;   // True if a value is in error, otherwise it's the name

              faCTL(char *bp, char *bz, int anum)
                   : info(new XrdSfsFAInfo[anum]), buff(bp), bend(bz),
                     vnsz(0), iNum(anum), iEnd(0), verr(false) {}
             ~faCTL() {if (info) delete [] info;}
};
}

#define CRED (const XrdSecEntity *)Client

#define FATTR_NAMESPACE 'U'

/******************************************************************************/
/*                                D e c o d e                                 */
/******************************************************************************/
  
namespace
{
XErrorCode Decode(faCTL &ctl, int MaxNsz, int MaxVsz)
{
   char *bP = ctl.buff, *bend = ctl.bend;
   int n, vsize;

// Decode variable names as kXR_unt16 0 || kXR_char var[n] || kXR_char 0
//
   ctl.verr = false;

   for (int i = 0; i < ctl.iNum; i++)
       {ctl.iEnd = i;
        if (bP+sizeof(kXR_unt16) >= bend) return kXR_ArgMissing;

        // Validate name prefix and force variable into the user namespace
        //
        if (*bP || *(bP+1)) return kXR_ArgInvalid;
        ctl.info[i].Name = bP;
        *bP++ = FATTR_NAMESPACE;
        *bP++ = '.';

        // Process the name (null terminated string)
        //
        n = strlen(bP);
        if (!n || n > MaxNsz)
           return (n ? kXR_ArgTooLong : kXR_ArgMissing);
        ctl.info[i].NLen = n;
        bP += n+1;
       }

// If there are  no values, then we are done
//
   ctl.vnsz = bP - ctl.buff;
   if (!MaxVsz) return (bP != bend ? kXR_BadPayload : (XErrorCode)0);
   ctl.verr = true;

// Decode variable values as kXR_int32 n || kXR_char val[n]
//
   for (int i = 0; i < ctl.iNum; i++)
       {ctl.iEnd = i;

        // Get the length
        //
        if (bP+sizeof(kXR_int32) > bend) return kXR_ArgInvalid;
        memcpy(&vsize, bP, sizeof(kXR_int32));
        vsize = ntohl(vsize);
        if (vsize < 0 || vsize > MaxVsz) return kXR_ArgTooLong;
        bP += sizeof(kXR_int32);

        // Get the value
        //
        ctl.info[i].Value = bP;
        ctl.info[i].VLen  = vsize;
        bP += vsize;
        if (bP > bend) return kXR_ArgInvalid;
       }

// Make sure nothing remains in the buffer
//
   if (bP != bend) return kXR_BadPayload;
   return (XErrorCode)0;
}
}

/******************************************************************************/
/*                                F i l l R C                                 */
/******************************************************************************/
  
namespace
{
void FillRC(kXR_char *faRC, XrdSfsFAInfo *info, int inum)
{
   kXR_unt16 rc;
   int nerrs = 0;

// Set status code for each element
//
   for (int i = 0; i < inum; i++)
       {if (info[i].faRC == 0) info[i].Name[0] = info[i].Name[1] = '\0';
           else {nerrs++;
                 rc = htons(XProtocol::mapError(info[i].faRC));
                 memcpy(info[i].Name, &rc, sizeof(rc));
                }
       }

// Complete vector and return length
//
   faRC[0] = nerrs;
   faRC[1] = inum;
}
}

/******************************************************************************/
/*                                 I O V e c                                  */
/******************************************************************************/
  
namespace
{
class IOVec
{
public:
struct iovec *Alloc(int &num)
                   {if (num > IOV_MAX) num = IOV_MAX;
                    theIOV = new struct iovec[num];
                    return theIOV;
                   }

       IOVec() : theIOV(0) {}
      ~IOVec() {if (theIOV) delete [] theIOV;}

private:
struct iovec *theIOV;
};
}
  
/******************************************************************************/
/*                               S e n d E r r                                */
/******************************************************************************/
  
namespace
{
int SendErr(XrdXrootdResponse &Resp, faCTL &ctl, XErrorCode eCode)
{
   char eBuff[1024];

   snprintf(eBuff, sizeof(eBuff), "%s processing fattr %s argument #%d",
            XProtocol::errName(eCode), (ctl.verr ? "data" : "name"), ctl.iNum);

   return Resp.Send(eCode, eBuff);
}

  
/* For future use
int SendErr(XrdXrootdResponse &Resp, const char *what, const char *path, int rc)
{
   int eCode = XProtocol::mapError(rc);
   char eBuff[2048];

   snprintf(eBuff, sizeof(eBuff), "%s processing fattr %s %s",
            XProtocol::errName(eCode), what, path);

   return Resp.Send((XErrorCode)eCode, eBuff);
}
*/
}
  
/******************************************************************************/
/*                              d o _ F A t t r                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_FAttr()
{
   const char *eTxt;
   char *fn, *fnCgi;
   int faCode = static_cast<int>(Request.fattr.subcode); // Is unsigned
   int popt, ropt, n, dlen = Request.header.dlen;
   bool isRO;

// Make sure we are configured for extended attributes
//
   if (!usxMaxNsz)
      return Response.Send(kXR_Unsupported, "fattr request is not supported");

// Prevalidate the subcode (it is unsigned)
//
   if (faCode > kXR_fatrrMaxSC)
      return Response.Send( kXR_ArgInvalid, "fattr subcode is invalid");

// Determine whether we will be reading or writing attributes
//
   if (faCode == kXR_fattrGet || faCode == kXR_fattrList)
      {isRO = true;
       eTxt = "Inspecting file attributes";
      } else {
       isRO = false;
       eTxt = "Modifying file attributes";
      }

// Make sure we have the right number of arguments
//
   if (faCode != kXR_fattrList && !dlen)
      return Response.Send(kXR_ArgMissing,
                          "Required arguments for fattr request not present");

// This operation may refer to an open file. Make sure it exists and is
// opened in a compatible mode. Otherwise, verify that the target file
// can be properly accessed by the client. If so, process the request.
//
   if (!dlen || argp->buff[0] == 0)
      {XrdXrootdFile *fp;
       XrdXrootdFHandle fh(Request.fattr.fhandle);
       char *theArg = argp->buff;

       if (!FTab || !(fp = FTab->Get(fh.handle)))
          return Response.Send(kXR_FileNotOpen, 
                               "fattr does not refer to an open file");
       if (!isRO && fp->FileMode != 'w')
          return Response.Send(kXR_InvalidRequest,
                          "fattr request modifies a file open for reading");
       if (dlen) {dlen--; theArg++;}

       return ProcFAttr(fp->FileKey, 0, theArg, dlen, faCode, false);
      }

// The operation is being targetted to a file path. First, get path length.
//
   fn = argp->buff;
   n  = strlen(argp->buff); // Always ends with a null byte!

// Prescreen the path and handle any redirects
//
   if (rpCheck(fn, &fnCgi)) return rpEmsg(eTxt, fn);
   if (!(popt = Squash(fn))) return vpEmsg(eTxt, fn);
   if (Route[RD_open1].Host[rdType] && (ropt = RPList.Validate(fn)))
      return Response.Send(kXR_redirect, Route[ropt].Port[rdType],
                                         Route[ropt].Host[rdType]);

// Hand this off to the attribute processor
//
   return ProcFAttr(fn, fnCgi, argp->buff+n+1, dlen-n-1, faCode, true);
}

/******************************************************************************/
/*                             P r o c F A t t r                              */
/******************************************************************************/
  
int XrdXrootdProtocol::ProcFAttr(char *faPath, char *faCgi,  char *faArgs,
                                 int   faALen, int   faCode, bool  doAChk)
{
   int fNumAttr = static_cast<int>(Request.fattr.numattr);

// Prevalidate the number of attributes (list must have zero)
//
   if ((faCode == kXR_fattrList &&  fNumAttr != 0)
   ||  (faCode != kXR_fattrList && (fNumAttr == 0 || fNumAttr > kXR_faMaxVars)))
      return Response.Send( kXR_ArgInvalid, "fattr numattr is invalid");

// Allocate an SFS control object now
//
   XrdSfsFACtl sfsCtl(faPath, faCgi, fNumAttr);
   sfsCtl.nPfx[0] = FATTR_NAMESPACE;
   sfsCtl.nPfx[1] = '.';
   if (doAChk) sfsCtl.opts = XrdSfsFACtl::accChk;

// If this is merely a list then go do it as there is nothing to parse
//
   if (faCode == kXR_fattrList) return XeqFALst(sfsCtl);

// Parse the request buffer as needed
//
   faCTL ctl(faArgs, faArgs+faALen, fNumAttr);
   XErrorCode rc =
              Decode(ctl, usxMaxNsz, (faCode == kXR_fattrSet ? usxMaxVsz : 0));
   if (rc) return SendErr(Response, ctl, rc);

// Transfer info ownership
//
   sfsCtl.info = ctl.info;
   ctl.info = 0;

// Perform the requested action
//
   if (faCode == kXR_fattrDel)  return XeqFADel(sfsCtl, faArgs, ctl.vnsz);
   if (faCode == kXR_fattrGet)  return XeqFAGet(sfsCtl, faArgs, ctl.vnsz);
   if (faCode == kXR_fattrSet)  return XeqFASet(sfsCtl, faArgs, ctl.vnsz);

   return Response.Send(kXR_Unsupported, "fattr request is not supported");
}
  
/******************************************************************************/
/*                              X e q F A D e l                               */
/******************************************************************************/
  
int XrdXrootdProtocol::XeqFADel(XrdSfsFACtl &ctl, char *faVars, int faVLen)
{
   XrdOucErrInfo eInfo(Link->ID, Monitor.Did, clientPV);
   struct iovec iov[3];
   kXR_char  faRC[2];
   int rc;

// Set correct subcode
//
   ctl.rqst = XrdSfsFACtl::faDel;

// Execute the action
//
   if ((rc = osFS->FAttr(&ctl, eInfo, CRED)))
      return fsError(rc, XROOTD_MON_OPENW, eInfo, ctl.path, (char *)ctl.pcgi);

// Format the response
//
   FillRC(faRC, ctl.info, ctl.iNum);

// Send off the response
//
   iov[1].iov_base = faRC;
   iov[1].iov_len  = sizeof(faRC);
   iov[2].iov_base = faVars;
   iov[2].iov_len  = faVLen;
   return Response.Send(iov, 3, sizeof(faRC) + faVLen);
}
  
/******************************************************************************/
/*                              X e q F A G e t                               */
/******************************************************************************/
  
int XrdXrootdProtocol::XeqFAGet(XrdSfsFACtl &ctl, char *faVars, int faVLen)
{
   XrdOucErrInfo eInfo(Link->ID, Monitor.Did, clientPV);
   IOVec iovHelper;
   struct iovec *iov;
   kXR_char  faRC[2];
   XResponseType rcode;
   int k, rc, dlen, vLen;

// Set correct subcode
//
   ctl.rqst = XrdSfsFACtl::faGet;

// Execute the action
//
   if ((rc = osFS->FAttr(&ctl, eInfo, CRED)))
      return fsError(rc, XROOTD_MON_OPENR, eInfo, ctl.path, (char *)ctl.pcgi);

// Format the common response
//
   FillRC(faRC, ctl.info, ctl.iNum);

// Allocate an iovec. We need two elements for each info entry.
//
   int iovNum = ctl.iNum*2+3;
   iov = iovHelper.Alloc(iovNum);

// Prefill the io vector (number of errors, vars, followed the rc-names
//
   iov[1].iov_base = faRC;
   iov[1].iov_len  = sizeof(faRC);
   iov[2].iov_base = faVars;
   iov[2].iov_len  = faVLen;
   dlen = sizeof(faRC) + faVLen;
   k = 3;

// Return the value for for each variable, segment the response, if need be
//
   for (int i = 0; i < ctl.iNum; i++)
       {iov[k  ].iov_base = &ctl.info[i].VLen;
        iov[k++].iov_len  = sizeof(ctl.info[i].VLen);
        dlen += sizeof(ctl.info[i].VLen);
        if (ctl.info[i].faRC || ctl.info[i].VLen == 0) ctl.info[i].VLen = 0;
           else {vLen = ctl.info[i].VLen;
                 ctl.info[i].VLen  = htonl(ctl.info[i].VLen);
                 iov[k  ].iov_base = (void *)ctl.info[i].Value;
                 iov[k++].iov_len  = vLen;
                 dlen += vLen;
                }
        if (k+1 >= iovNum)
          {rcode = (i+1 == ctl.iNum ? kXR_ok : kXR_oksofar);
           if ((rc = Response.Send(rcode, iov, k, dlen))) return rc;
           k = 1; dlen = 0;
          }
       }

// Check if we need to send out the last amount of data
//
   return (dlen ? Response.Send(iov, k, dlen) : 0);
}
  
/******************************************************************************/
/*                              X e q F A L s d                               */
/******************************************************************************/
  
int XrdXrootdProtocol::XeqFALsd(XrdSfsFACtl &ctl)
{
   IOVec iovHelper;
   struct iovec *iov;
   XResponseType rcode;
   int k = 1, rc = 0, dlen = 0, vLen;
   bool xresp = false;

// Make sure we have something to send
//
   if (!ctl.iNum) return Response.Send();

// Allocate an iovec. We need three elements for each info entry.
//
   int iovNum = ctl.iNum*3+1;
   iov = iovHelper.Alloc(iovNum);

// Return the value for for each variable, segment the response, if need be
//
   for (int i = 0; i < ctl.iNum; i++)
       {if (ctl.info[i].faRC) continue;
        iov[k  ].iov_base = ctl.info[i].Name;
        iov[k++].iov_len  = ctl.info[i].NLen+1;
        dlen += ctl.info[i].NLen+1;

        vLen = ctl.info[i].VLen;
        ctl.info[i].VLen = htonl(vLen);
        iov[k  ].iov_base = &ctl.info[i].VLen;
        iov[k++].iov_len  = sizeof(ctl.info[i].VLen);
        dlen += sizeof(ctl.info[i].VLen);

        iov[k  ].iov_base = (void *)ctl.info[i].Value;
        iov[k++].iov_len  = vLen;
        dlen += vLen;

        if (k+2 >= iovNum)
          {rcode = (i+1 == ctl.iNum ? kXR_ok : kXR_oksofar);
           if ((rc = Response.Send(rcode, iov, k, dlen))) return rc;
           k = 1; dlen = 0; xresp = true;
          }
       }

// Check if we need to send out the last amount of data
//
   return (dlen ? Response.Send(iov, k, dlen) : 0);

// Check if anything was sent at all
//
   return (xresp ? 0 : Response.Send());
}
  
/******************************************************************************/
/*                              X e q F A L s t                               */
/******************************************************************************/
  
int XrdXrootdProtocol::XeqFALst(XrdSfsFACtl &ctl)
{
   struct iovec iov[16];
   XrdOucErrInfo eInfo(Link->ID, Monitor.Did, clientPV);
   int rc;

// Set correct subcode
//
   ctl.rqst = XrdSfsFACtl::faLst;

// Set correct options
//
   if (Request.fattr.options & ClientFattrRequest::aData)
      ctl.opts |= XrdSfsFACtl::retval;

// Execute the action
//
   if ((rc = osFS->FAttr(&ctl, eInfo, CRED)))
      return fsError(rc, XROOTD_MON_OPENR, eInfo, ctl.path, (char *)ctl.pcgi);

// Check for more complicated return
//
   if (ctl.opts & XrdSfsFACtl::retval) return XeqFALsd(ctl);

// If there is only a single buffer, hen we can do a simple response
//
   if (!ctl.fabP) return Response.Send();
   if (ctl.fabP->next == 0)
      return Response.Send(ctl.fabP->data, ctl.fabP->dlen);

// Send of the response in as many segments as we need
//
   int dlen = 0, i = 1;
   XrdSfsFABuff *dP = ctl.fabP;

   while(dP)
      {iov[i].iov_base = dP->data;
       iov[i].iov_len  = dP->dlen;
       dlen += dP->dlen;
       dP = dP->next;
       i++;
       if (i == (int)sizeof(iov))
          {rc = Response.Send((dP ? kXR_oksofar : kXR_ok), iov, i, dlen);
           if (rc || dP == 0) return rc;
           dlen = 0;
           i = 1;
          }
      }

// Check if we need to send out the last amount of data
//
   return (dlen ? Response.Send(iov, i, dlen) : 0);
}

/******************************************************************************/
/*                              d o _ F A S e t                               */
/******************************************************************************/

int XrdXrootdProtocol::XeqFASet(XrdSfsFACtl &ctl, char *faVars, int faVLen)
{
   XrdOucErrInfo eInfo(Link->ID, Monitor.Did, clientPV);
   struct iovec iov[3];
   kXR_char  faRC[2];
   int rc;

// Set correct subcode and options
//
   ctl.rqst = XrdSfsFACtl::faSet;
   if (Request.fattr.options & ClientFattrRequest::isNew)
      ctl.opts |= XrdSfsFACtl::newAtr;

// Execute the action
//
   if ((rc = osFS->FAttr(&ctl, eInfo, CRED)))
      return fsError(rc, XROOTD_MON_OPENW, eInfo, ctl.path, (char *)ctl.pcgi);

// Format the response
//
   FillRC(faRC, ctl.info, ctl.iNum);

// Send off the response
//
   iov[1].iov_base = faRC;
   iov[1].iov_len  = sizeof(faRC);
   iov[2].iov_base = faVars;
   iov[2].iov_len  = faVLen;
   return Response.Send(iov, 3, sizeof(faRC) + faVLen);
}
