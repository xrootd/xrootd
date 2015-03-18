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

class XrdSsiRRInfo
{
public:

enum   Opc {Rxq = 0, Rwt = 1, Can = 2};

inline void               Cmd(Opc cmd) {reqCmd  = static_cast<char>(cmd);}

inline Opc                Cmd()        {return    static_cast<Opc>(reqCmd);}

inline void               Id(int id)   {reqId   = static_cast<char>(id);}

inline int                Id()         {return    static_cast<int>(reqId)&0xff;}

inline void               Size(int sz) {reqSize = htonl(sz);}

inline int                Size()       {return    ntohl(reqSize);}

inline unsigned long long Info()
       {return (static_cast<unsigned long long>(reqCmd)<<56LL)
              |(static_cast<unsigned long long>(reqId )<<32LL)
              |(static_cast<unsigned long long>(reqSize) & 0xffffffffLL);
       }

       XrdSsiRRInfo(unsigned long long ival=0)
                   : reqCmd(ival>>56), reqId(ival>>32),
                     reqSize(ival & 0xffffffff) {}

      ~XrdSsiRRInfo() {}

private:
char               reqCmd;
char               Rsv[2];
char               reqId;
int                reqSize;
};
#endif
