#ifndef __XROOTD_RESPONSE_H__
#define __XROOTD_RESPONSE_H__
/******************************************************************************/
/*                                                                            */
/*                     x r o o t d _ R e s p o n s e . h                      */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//      $Id$

#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
  
#include "XProtocol/XProtocol.hh"

/******************************************************************************/
/*                       x r o o t d _ R e s p o n s e                        */
/******************************************************************************/
  
class XrdLink;

class XrdXrootdResponse
{
public:

const  char *ID() {return (const char *)trsid;}

       int   Push(void *data, int dlen);
       int   Push(void);
       int   Send(void);
       int   Send(const char *msg);
       int   Send(XErrorCode ecode, const char *msg);
       int   Send(void *data, int dlen);
       int   Send(struct iovec *, int iovcnt, int iolen=-1);
       int   Send(XResponseType rcode, void *data, int dlen);
       int   Send(XResponseType rcode, int info, char *data);

inline void  Set(XrdLink *lp) {Link = lp;}
       void  Set(unsigned char *stream);

       XrdXrootdResponse(XrdXrootdResponse &rhs) {Set(rhs.Link);
                                              Set(&rhs.Resp.streamid[0]);
                                             }
       XrdXrootdResponse() {Link = 0; *trsid = '\0';
                          RespIO[0].iov_base = (caddr_t)&Resp;
                          RespIO[0].iov_len  = sizeof(Resp);
                         }
      ~XrdXrootdResponse() {}

private:

       ServerResponseHeader Resp;
       XrdLink             *Link;
struct iovec                RespIO[3];

       char                 trsid[8];  // sizeof() does not work here
static const char          *TraceID;
};
#endif
