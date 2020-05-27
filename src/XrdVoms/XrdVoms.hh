#ifndef ___XRD_VOMS_H___
#define ___XRD_VOMS_H___
/******************************************************************************/
/*                                                                            */
/*                            X r d V o m s . h h                             */
/*                                                                            */
/*  (C) 2013  G. Ganis, CERN                                                  */
/*                                                                            */
/*  All rights reserved. The copyright holder's institutional names may not   */
/*  be used to endorse or promote products derived from this software without */
/*  specific prior written permission.                                        */
/*                                                                            */
/*  This file is part of the VOMS extraction XRootD plug-in software suite,   */
/*  here after called VOMS-XRootD (see https://github.com/gganis/voms).       */
/*                                                                            */
/*  VOMS-XRootD is free software: you can redistribute it and/or modify it    */
/*  under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or (at     */
/*  your option) any later version.                                           */
/*                                                                            */
/*  VOMS-XRootD is distributed in the hope that it will be useful, but        */
/*  WITHOUT ANY WARRANTY, not even the implied warranty of MERCHANTABILITY or */
/*  FITNESS FOR A PARTICULAR PURPOSE.                                         */
/*  See the GNU Lesser General Public License for more details.               */
/*                                                                            */
/*  You should have received a copy of the GNU Lesser General Public License  */
/*  along with VOMS-XRootD in a file called COPYING.LGPL (LGPL license) and   */
/*  file COPYING (GPL license). If not, see <http://www.gnu.org/licenses/>.   */
/*                                                                            */
/******************************************************************************/


#include "voms/voms_api.h"
#include "openssl/x509.h"
#include "openssl/pem.h"

// Structure for interpreting input to the VOMS function when format is set to
// STACK_OF(X509)
typedef struct {
   X509           *cert;
   STACK_OF(X509) *chain;
} Voms_x509_in_t;

#endif
