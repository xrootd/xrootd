#ifndef __CRYPTO_AUX_H__
#define __CRYPTO_AUX_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d C r y p t o A u x . h h                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Gerri Ganis for CERN                                         */
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

#include <cstdio>
#include <ctime>
#ifndef WIN32
#include "XrdSys/XrdSysHeaders.hh"
#endif
#include "XProtocol/XProtocol.hh"

/******************************************************************************/
/*                 M i s c e l l a n e o u s   D e f i n e s                  */
/******************************************************************************/
#define ABSTRACTMETHOD(x) {cerr <<"Method "<<x<<" must be overridden!" <<endl;}

/******************************************************************************/
/*          E r r o r   L o g g i n g / T r a c i n g   F l a g s             */
/******************************************************************************/
#define cryptoTRACE_ALL       0x0007
#define cryptoTRACE_Dump      0x0004
#define cryptoTRACE_Debug     0x0002
#define cryptoTRACE_Notify    0x0001

// RSA parameters
#define XrdCryptoMinRSABits 512
#define XrdCryptoDefRSABits 1024
#define XrdCryptoDefRSAExp  0x10001

/******************************************************************************/
/*                     U t i l i t y   F u n c t i o n s                      */
/******************************************************************************/
typedef int (*XrdCryptoKDFunLen_t)();
typedef int (*XrdCryptoKDFun_t)(const char *pass, int plen,
                                const char *salt, int slen,
                                char *key, int klen);
int XrdCryptoKDFunLen();
int XrdCryptoKDFun(const char *pass, int plen, const char *salt, int slen,
                   char *key, int klen);


/******************************************************************************/
/*  X r d C r y p t o S e t T r a c e                                         */
/*                                                                            */
/*  Set trace flags according to 'trace'                                      */
/*                                                                            */
/******************************************************************************/
//______________________________________________________________________________
void XrdCryptoSetTrace(kXR_int32 trace);


/******************************************************************************/
/*  X r d C r y p t o T Z C o r r                                             */
/*                                                                            */
/*  Time Zone correction (calculated once)                                    */
/*                                                                            */
/******************************************************************************/
//______________________________________________________________________________
time_t XrdCryptoTZCorr();
const time_t XrdCryptoDSTShift = 3600;

#endif
