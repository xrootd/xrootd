#ifndef __XRDXROOTDREQID_HH_
#define __XRDXROOTDREQID_HH_
/******************************************************************************/
/*                                                                            */
/*                     X r d X r o o t d R e q I D . h h                      */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string.h>

class XrdXrootdReqID
{
public:

inline unsigned long long getID() {return Req.ID;}

inline void               getID(unsigned char *sid, int &lid,unsigned int &linst)
                               {memcpy(sid, Req.ids.Sid, sizeof(Req.ids.Sid));
                                lid = static_cast<int>(Req.ids.Lid);
                                linst = Req.ids.Linst;
                               }

inline void               setID(unsigned long long id) {Req.ID = id;}

inline void               setID(const unsigned char *sid,int lid,unsigned int linst)
                               {memcpy(Req.ids.Sid, sid, sizeof(Req.ids.Sid));
                                Req.ids.Lid = static_cast<unsigned short>(lid);
                                Req.ids.Linst = linst;
                               }

inline unsigned long long setID(const unsigned char *sid)
                               {memcpy(Req.ids.Sid, sid, sizeof(Req.ids.Sid));
                                return Req.ID;
                               }

inline unsigned char     *Stream() {return Req.ids.Sid;}

        XrdXrootdReqID(unsigned long long id) {setID(id);}
        XrdXrootdReqID(const unsigned char *sid, int lid, unsigned int linst)
                      {setID(sid ? (unsigned char *)"\0\0" : sid, lid, linst);}
        XrdXrootdReqID() {}

private:

union {unsigned long long     ID;
       struct {unsigned int   Linst;
               unsigned short Lid;
               unsigned char  Sid[2];
              } ids;
      } Req;
};
#endif
