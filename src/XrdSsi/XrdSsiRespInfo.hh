#ifndef __XRDSSIRESPINFO_HH__
#define __XRDSSIRESPINFO_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d S s i R e s p I n f o . h h                      */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

//-----------------------------------------------------------------------------
//! The RespInfo structure describes the response to be posted to a request.
//! It is used mainly server-side via the inherieted XrdSsiResponder class (see
//! XrdSsiResponder::SetResponse). It generally hidden on the client-side since
//! many of the response types valid server-side are converted to simpler
//! responses client-side and requiring a client to fully deal withthis struct
//! is largely over-kill and unnecessary.
//-----------------------------------------------------------------------------

class XrdSsiStream;

struct  XrdSsiRespInfo
       {union {const char   *buff;    //!< ->buffer     when rType == isData;
               const char   *eMsg;    //!< ->msg text   when rType == isError;
               long long     fsize;   //!< ->file size  when rType == isFile;
               XrdSsiStream *strmP;   //!< ->SsiStream  when rType == isStream;
              };
        union {      int     blen;    //!<   buffer len When rType == isData;
                     int     eNum;    //!<   errno      When rType == isError;
                     int     fdnum;   //!<   fdnum      When rType == isFile;
              };
        enum   Resp_t {isNone = 0, isData, isError, isFile, isStream};
        Resp_t rType;

        inline void  Init()  {fsize = 0; blen = 0; rType = isNone;}

        const  char *State() const {if (rType == isData  ) return "isData";
                                    if (rType == isError ) return "isError";
                                    if (rType == isFile  ) return "isFile";
                                    if (rType == isStream) return "isStream";
                                    if (rType == isNone  ) return "isNone";
                                    return "isUndef";
                                   }

        XrdSsiRespInfo() {Init();}
       ~XrdSsiRespInfo() {}
       };
#endif
