#ifndef __XROOTD_RESPONSE_H__
#define __XROOTD_RESPONSE_H__
/******************************************************************************/
/*                                                                            */
/*                  X r d X r o o t d R e s p o n s e . h h                   */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
  
#include "XProtocol/XProtocol.hh"
#include "XProtocol/XPtypes.hh"
#include "XrdXrootd/XrdXrootdReqID.hh"

/******************************************************************************/
/*                       x r o o t d _ R e s p o n s e                        */
/******************************************************************************/
  
class XrdLink;
class XrdOucSFVec;
class XrdXrootdTransit;

class XrdXrootdResponse
{
public:

const  char *ID() {return (const char *)trsid;}

       int   Send(void);
       int   Send(const char *msg);
       int   Send(XErrorCode ecode, const char *msg);
       int   Send(void *data, int dlen);
       int   Send(struct iovec *, int iovcnt, int iolen=-1);
       int   Send(XResponseType rcode, void *data, int dlen);
       int   Send(XResponseType rcode, struct iovec *IOResp,
                 int iornum, int iolen=-1);
       int   Send(XResponseType rcode, int info, const char *data, int dsz=-1);
       int   Send(int fdnum, long long offset, int dlen);
       int   Send(XrdOucSFVec *sfvec, int sfvnum, int dlen);
static int   Send(XrdXrootdReqID &ReqID,  XResponseType Status,
                  struct iovec   *IOResp, int           iornum, int  iolen);

inline void  Set(XrdLink *lp) {Link = lp;}
inline void  Set(XrdXrootdTransit *tp) {Bridge = tp;}
       void  Set(kXR_char *stream);

       bool  isOurs() {return Bridge == 0;}

       XrdLink *theLink()               {return Link;}
       void     StreamID(kXR_char *sid) {sid[0] = Resp.streamid[0];
                                         sid[1] = Resp.streamid[1];
                                        }

       XrdXrootdResponse(XrdXrootdResponse &rhs) {Set(rhs.Link);
                                                  Set(rhs.Bridge);
                                                  Set(rhs.Resp.streamid);
                                                 }

       XrdXrootdResponse() {Link = 0; Bridge = 0; *trsid = '\0';
                          RespIO[0].iov_base = (caddr_t)&Resp;
                          RespIO[0].iov_len  = sizeof(Resp);
                         }
      ~XrdXrootdResponse() {}

       XrdXrootdResponse &operator =(const XrdXrootdResponse &rhs)
                                   {Set(rhs.Link);
                                    Set(rhs.Bridge);
                                    Set((unsigned char *)rhs.Resp.streamid);
                                    return *this;
                                   }

private:

       XrdXrootdTransit    *Bridge;
       ServerResponseHeader Resp;
       XrdLink             *Link;
struct iovec                RespIO[3];

       char                 trsid[8];  // sizeof() does not work here
static const char          *TraceID;
};
#endif
