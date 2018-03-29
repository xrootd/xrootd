/******************************************************************************/
/*                                                                            */
/*                        X r d O u c T r a c e . c c                         */
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

#include "XrdOuc/XrdOucTrace.hh"

/******************************************************************************/
/*                               b i n 2 h e x                                */
/******************************************************************************/
  
char *XrdOucTrace::bin2hex(char *inbuff, int dlen, char *buff)
{
    static char hv[] = "0123456789abcdef";
    static char xbuff[56];
    char *outbuff = (buff ? buff : xbuff);
    int i;
    if (dlen > 24) dlen = 24;
    for (i = 0; i < dlen; i++) {
        *outbuff++ = hv[(inbuff[i] >> 4) & 0x0f];
        *outbuff++ = hv[ inbuff[i]       & 0x0f];
        if ((i & 0x03) == 0x03 || i+1 == dlen) *outbuff++ = ' ';
        }
     *outbuff = '\0';
     return xbuff;
}
