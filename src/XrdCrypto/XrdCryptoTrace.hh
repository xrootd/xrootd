#ifndef ___CRYPTO_TRACE_H___
#define ___CRYPTO_TRACE_H___
/******************************************************************************/
/*                                                                            */
/*                    X r d C r y p t o T r a c e . h h                       */
/*                                                                            */
/* (C) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

#include "XrdOuc/XrdOucTrace.hh"
#include "XrdCrypto/XrdCryptoAux.hh"

#ifndef NODEBUG

#include "XrdSys/XrdSysHeaders.hh"

#define QTRACE(act) (cryptoTrace && (cryptoTrace->What & cryptoTRACE_ ## act))
#define PRINT(y)    {if (cryptoTrace) {cryptoTrace->Beg(epname); \
                                       cerr <<y; cryptoTrace->End();}}
#define TRACE(act,x) if (QTRACE(act)) PRINT(x)
#define DEBUG(y)     TRACE(Debug,y)
#define EPNAME(x)    static const char *epname = x;

#else

#define QTRACE(x)
#define  PRINT(x)
#define  TRACE(x,y)
#define  DEBUG(x)
#define EPNAME(x)

#endif

//
// For error logging and tracing
extern XrdOucTrace *cryptoTrace;

#endif
