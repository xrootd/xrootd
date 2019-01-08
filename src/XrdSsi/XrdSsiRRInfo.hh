#ifndef _XRDSSIRRINFO_H
#define _XRDSSIRRINFO_H
/******************************************************************************/
/*                                                                            */
/*                       X r d S s i R R I n f o . h h                        */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <arpa/inet.h>

#include "XrdSsi/XrdSsiRespInfo.hh"

class XrdSsiRRInfo
{
public:

static const unsigned int idMax = 16777215;

enum   Opc {Rxq = 0, Rwt = 1, Can = 2};

inline void                 Cmd(Opc cmd)
                               {reqCmd  = static_cast<unsigned char>(cmd);}

inline Opc                  Cmd() {return    static_cast<Opc>(reqCmd);}

inline const unsigned char *Data()       {return &reqCmd;}

inline void                 Id(unsigned int id)
                              {unsigned char tmp = reqCmd;
                               reqId  = htonl(id & idMask);
                               reqCmd = tmp;
                              }

inline unsigned int         Id() {return ntohl(reqId) & idMask;}

inline void                 Size(unsigned int sz) {reqSize = htonl(sz);}

inline unsigned int         Size()                {return    ntohl(reqSize);}

inline unsigned long long Info()
       {return (static_cast<unsigned long long>(reqId   & 0xffffffff) <<32LL)
              |(static_cast<unsigned long long>(reqSize & 0xffffffff));

       }

       XrdSsiRRInfo(unsigned long long ival=0)
                   : reqId(static_cast<unsigned int>( (ival>>32) & 0xffffffff)),
                     reqSize(static_cast<unsigned int>(ival & 0xffffffff)) {}

      ~XrdSsiRRInfo() {}

private:
static const int idMask = 0x00ffffff;

union {unsigned char reqCmd;
       unsigned int  reqId;
      };
       unsigned int  reqSize;
};

/******************************************************************************/
/*                      X r d S s i R R I n f o A t t n                       */
/******************************************************************************/

struct  XrdSsiRRInfoAttn
{
static   const int  alrtResp = '!';  // In tag: response data is an alert
static   const int  fullResp = ':';  // In tag: response data is present
static   const int  pendResp = '*';  // In tag: response data is pending

         char  tag;
         char  flags;
unsigned short pfxLen;   // Length of prefix
unsigned int   mdLen;    // Length of metadata
         int   rsvd1;
         int   rsvd2;
};
#endif
