#ifndef __SFS_FATTR_H__
#define __SFS_FATTR_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d S f s F A t t r . h h                         */
/*                                                                            */
/*(c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*Produced by Andrew Hanushevsky for Stanford University under contract       */
/*           DE-AC02-76-SFO0515 with the Deprtment of Energy                  */
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

#include <cstdlib>

//-----------------------------------------------------------------------------
//! This include file defines control strucres used to drive entended file
//! attribute handling via the fsctl() method.
//-----------------------------------------------------------------------------
  
/******************************************************************************/
/*                          X r d S f s F A I n f o                           */
/******************************************************************************/
  
struct XrdSfsFAInfo
{
char  *Name;   //!< Variable name
char  *Value;  //!< Variable value
int    VLen;   //!< Variable value length (aligned)
short  NLen;   //!< Length of name  not including null byte
int    faRC;   //!< Action return code for this element

       XrdSfsFAInfo() : Value(0), VLen(0), NLen(0), faRC(0) {}
      ~XrdSfsFAInfo() {}
};

/******************************************************************************/
/*                          X r d S f s F A B u f f                           */
/******************************************************************************/

struct XrdSfsFABuff
{
XrdSfsFABuff *next;
int           dlen;    //!< Data Length in subsequent buffer
char          data[4]; //!< Start of data
};
  
/******************************************************************************/
/*                           X r d S f s F A C t l                            */
/******************************************************************************/

class XrdOucEnv;
  
struct XrdSfsFACtl
{
const char      *path;    //!< The file path to act on (logical)
const char      *pcgi;    //!< Opaque information (null if none)
const char      *pfnP;    //!< The file path to act on (physical)
XrdSfsFAInfo    *info;    //!< Pointer to attribute information
XrdOucEnv       *envP;    //!< Optional environmental information
XrdSfsFABuff    *fabP;    //!<  -> Additional memory that was allocated
char             nPfx[2]; //!< The namespace being used
unsigned short   iNum;    //!< Number of info entries
unsigned char    rqst;    //!< Type of file attribute request (see below)
unsigned char    opts;    //!< Request options (see below)

enum RQST:char {faDel = 0, faGet, faLst, faSet, faFence};

static const int accChk = 0x01;   //!< Perform access check
static const int newAtr = 0x02;   //!< For set the attribute must not exist
static const int xplode = 0x04;   //!< Construct an info vec from faList
static const int retvsz = 0x0c;   //!< Above plus return size of attr value
static const int retval = 0x1c;   //!< Above plus return actual  attr value

              XrdSfsFACtl(const char *p, const char *opq, int anum)
                         : path(p), pcgi(opq), pfnP(0), info(0), envP(0),
                           fabP(0), iNum(anum), rqst(255), opts(0)
                           {nPfx[0] = 0; nPfx[1] = 0;}

             ~XrdSfsFACtl() {XrdSfsFABuff *dP, *nP = fabP;
                             while((dP = nP)) {nP = nP->next; free(dP);}
                             if (info) delete [] info;
                            }
};
#endif
