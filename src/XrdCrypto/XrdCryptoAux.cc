/******************************************************************************/
/*                                                                            */
/*                      X r d C r y p t o A u x . c c                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Geri Ganis for CERN                                          */
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

#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysError.hh"

#include "XrdCrypto/XrdCryptoAux.hh"
#include "XrdCrypto/XrdCryptoTrace.hh"

//
// For error logging and tracing
static XrdSysLogger Logger;
static XrdSysError eDest(0,"crypto_");
XrdOucTrace *cryptoTrace = 0;
//
// Time Zone correction (wrt UTC)
static time_t TZCorr = 0;
static bool TZInitialized = 0;

/******************************************************************************/
/*  X r d C r y p t o S e t T r a c e                                         */
/******************************************************************************/
//______________________________________________________________________________
void XrdCryptoSetTrace(kXR_int32 trace)
{
   // Set trace flags according to 'trace'

   //
   // Initiate error logging and tracing
   eDest.logger(&Logger);
   if (!cryptoTrace)
      cryptoTrace = new XrdOucTrace(&eDest);
   if (cryptoTrace) {
      // Set debug mask
      cryptoTrace->What = 0;
      // Low level only
      if ((trace & cryptoTRACE_Notify))
         cryptoTrace->What |= cryptoTRACE_Notify;
      // Medium level
      if ((trace & cryptoTRACE_Debug))
         cryptoTrace->What |= (cryptoTRACE_Notify | cryptoTRACE_Debug);
      // High level
      if ((trace & cryptoTRACE_Dump))
         cryptoTrace->What |= cryptoTRACE_ALL;
   }
}

/******************************************************************************/
/*  X r d C r y p t o T i m e G m                                             */
/******************************************************************************/
//______________________________________________________________________________
time_t XrdCryptoTZCorr()
{
   // Time Zone correction (wrt UTC)
   // Assumes no DST, the correction is not expected to change during the year
   
   if (!TZInitialized) {
      time_t now = time(0);
      struct tm ltn, gtn;
      if (localtime_r(&now, &ltn) != 0 && gmtime_r(&now, &gtn) != 0) {
         TZCorr = time_t(difftime(mktime(&ltn), mktime(&gtn)));
         TZInitialized = 1;
      }
   }
   // Done
   return TZCorr;
}
