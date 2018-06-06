/******************************************************************************/
/*                                                                            */
/*                       X r d X r o o t d X e q . h h                        */
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

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/

#define CRED (const XrdSecEntity *)Client

#define TRACELINK Link

#define STATIC_REDIRECT(xfnc) \
        if (Route[xfnc].Port[rdType]) \
           return Response.Send(kXR_redirect,Route[xfnc].Port[rdType],\
                                             Route[xfnc].Host[rdType])

/******************************************************************************/
/*                     C o m m o n   S t r u c t u r e s                      */
/******************************************************************************/
  
struct XrdXrootdFHandle
       {kXR_int32 handle;

        void Set(kXR_char *ch)
            {memcpy((void *)&handle, (const void *)ch, sizeof(handle));}
        XrdXrootdFHandle() {}
        XrdXrootdFHandle(kXR_char *ch) {Set(ch);}
       ~XrdXrootdFHandle() {}
       };
