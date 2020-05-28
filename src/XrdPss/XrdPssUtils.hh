#ifndef __XRDPSS_UTILS_HH__
#define __XRDPSS_UTILS_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d P s s U t i l s . h h                         */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <vector>
  
class XrdPssUtils
{
public:

// Get the domain associated with he hostname
//
static
const char *getDomain(const char *hName);

// Check if protocol will send this to an xrootd protocol server
//
static bool is4Xrootd(const char *pname);

// Validate a protocol (i.e. it is one we actually support)
//
static
const char *valProt(const char *pname, int &plen, int adj=0);

// Vectorize a comma separated list and return pointers into the input string
// to each element in the list. The list itself is modified.
//
static bool Vectorize(char *str, std::vector<char *> &vec, char sep);

                   XrdPssUtils() {}
                  ~XrdPssUtils() {}
};

// A quick inline to test for xroot path forwarding
//
#define IS_FWDPATH(x) ((*(x+1) == 'x' || *(x+1) == 'r') \
                    && (!strncmp("/xroot:/", x,8) || !strncmp("/root:/", x,7)\
                    ||  !strncmp("/xroots:/",x,9) || !strncmp("/roots:/",x,8)))
#endif
