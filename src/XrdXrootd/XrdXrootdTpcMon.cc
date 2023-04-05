/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d T p c M o n . c c                     */
/*                                                                            */
/* (c) 2022 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdio.h>
#include <stdlib.h>

#include "XrdSys/XrdSysError.hh"
#include "XrdXrootd/XrdXrootdGStream.hh"
#include "XrdXrootd/XrdXrootdTpcMon.hh"

/******************************************************************************/
/*                         J s o n   T e m p l a t e                          */
/******************************************************************************/
  
namespace
{
const char *json_fmt = "{\"TPC\":\"%s\",\"Client\":\"%s\","
"\"Xeq\":{\"Beg\":\"%s\",\"End\":\"%s\",\"RC\":%d,\"Strm\":%u,\"Type\":\"%s\","
        "\"IPv\":%c},"
"\"Src\":\"%s\",\"Dst\":\"%s\",\"Size\":%zu}";

const char *urlFMT = "";

XrdSysError eDest(0, "Ouc");
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdXrootdTpcMon::XrdXrootdTpcMon(const char       *proto,
                                 XrdSysLogger     *logP,
                                 XrdXrootdGStream &gStrm)
                                : protocol(proto), gStream(gStrm)
{
   const char *colon = ":";
   char buff[512];

// Initialize eror object
//
   eDest.logger(logP);

// Get our host:port
//
   const char *host = getenv("XRDHOST"); if (!host) host = "localhost";
   const char *port = getenv("XRDPORT"); if (!port) {colon = ""; port = "";}

   snprintf(buff, sizeof(buff), "%%s://%s%s%s/%%s", host, colon, port);
   urlFMT = strdup(buff);
}

/******************************************************************************/
/* Private:                       g e t U R L                                 */
/******************************************************************************/

const char *XrdXrootdTpcMon::getURL(const char *spec, const char *prot,
                                    char *buff, int bsz)
{
// Handle the spec
//
   if (*spec == '/')
      {snprintf(buff, bsz, urlFMT, prot, spec);
       spec = buff;
      }
   return spec;
}
  
/******************************************************************************/
/* Private:                       g e t U T C                                 */
/******************************************************************************/
  
const char *XrdXrootdTpcMon::getUTC(struct timeval& tod,
                                    char* utcBuff, int utcBLen)
{
   struct tm utcDT;
   char *bP;

// Get the time in UTC
//
   gmtime_r(&tod.tv_sec, &utcDT);

// Format this ISO 8601 style
//
   size_t n = strftime(utcBuff, utcBLen, "%FT%T", &utcDT);
   bP = utcBuff + n; utcBLen -= n;
   snprintf(bP, utcBLen, ".%03uZ", static_cast<unsigned int>(tod.tv_usec));

// Return result
//
   return utcBuff;
}

/******************************************************************************/
/*                                R e p o r t                                 */
/******************************************************************************/
  
void XrdXrootdTpcMon::Report(XrdXrootdTpcMon::TpcInfo &info)
{
   const char *srcURL, *dstURL;
   char bt_buff[40], et_buff[40], sBuff[1024], dBuff[1024], buff[8192];

// Get correct source and destination URLs
//
   srcURL = getURL(info.srcURL, protocol, sBuff, sizeof(sBuff));
   dstURL = getURL(info.dstURL, protocol, dBuff, sizeof(dBuff));

// Format the line
//
   int n = snprintf(buff, sizeof(buff), json_fmt, protocol, info.clID,
                    getUTC(info.begT, bt_buff, sizeof(bt_buff)),
                    getUTC(info.endT, et_buff, sizeof(et_buff)),
                    info.endRC, static_cast<unsigned int>(info.strm),
                    (info.opts & TpcInfo::isaPush ? "push" : "pull"),
                    (info.opts & TpcInfo::isIPv4 ? '4' : '6'),
                    srcURL, dstURL, info.fSize);

// Check for truncation
//
   if (n >= (int)sizeof(buff))
      eDest.Emsg("TpcMon", protocol, "invalid json; line truncated!");

// Send the message
//
   if (!gStream.Insert(buff, n+1))
      eDest.Emsg("TpcMon", protocol, "invalid json; gStream buffer rejected!");
}
