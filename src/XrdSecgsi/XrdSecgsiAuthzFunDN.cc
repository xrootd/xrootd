/******************************************************************************/
/*                                                                            */
/*             X r d S e c g s i G M A P F u n D N . c c                      */
/*                                                                            */
/* (c) 2011, G. Ganis / CERN                                                  */
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
/*                                                                            */
/******************************************************************************/

/* ************************************************************************** */
/*                                                                            */
/* GMAP function implementation extracting info from the DN                   */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "XrdVersion.hh"

#include "XrdCrypto/XrdCryptosslAux.hh"
#include "XrdCrypto/XrdCryptoX509.hh"
#include "XrdCrypto/XrdCryptoX509Chain.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSecgsi/XrdSecgsiTrace.hh"
#include "XrdSut/XrdSutBucket.hh"

/******************************************************************************/
/*                   V e r s i o n   I n f o r m a t i o n                    */
/******************************************************************************/
  
XrdVERSIONINFO(XrdSecgsiAuthzFun,secgsiauthz);

XrdVERSIONINFO(XrdSecgsiAuthzKey,secgsiauthz);

XrdVERSIONINFO(XrdSecgsiAuthzInit,secgsiauthz);

/******************************************************************************/
/*                     G l o b a l s   &   S t a t i c s                      */
/******************************************************************************/
  
extern XrdOucTrace *gsiTrace;

static int gCertfmt = 1;

/******************************************************************************/
/*                     X r d S e c g s i A u t h z F u n                      */
/******************************************************************************/
  
//
// Main function
//
extern "C"
{
int XrdSecgsiAuthzFun(XrdSecEntity &entity)
{
   // Implementation of XrdSecgsiAuthzFun extracting the information from the 
   // proxy chain in entity.creds
   EPNAME("AuthzFunDN");

   // Notify
   DEBUG("dummy call for '"<<entity.name<<"'");

   // Add something for test
   entity.vorg = strdup("VO.dummy.test");
   DEBUG("setting vorg: '"<<entity.vorg<<"'");
  
   // Done
   return 0;
}}

//
// Key function
//
extern "C"
{
int XrdSecgsiAuthzKey(XrdSecEntity &entity, char **key)
{
   // Implementation of XrdSecgsiAuthzKey extracting the information from the 
   // proxy chain in entity.creds
   EPNAME("AuthzKeyDN");
   
   // Must have got something
   if (!key) {
      PRINT("ERROR: 'key' must be defined");
      return -1;
   }

   // We will key on the end proxy DN
   XrdCryptoX509Chain *chain = 0;
   if (!entity.creds) {
      PRINT("ERROR: 'entity.creds' must be defined");
      return -1;
   }
   if (gCertfmt == 0) {
      chain = (XrdCryptoX509Chain *) entity.creds;
   } else {
      XrdOucString s((const char *) entity.creds);
      XrdSutBucket *b = new XrdSutBucket(s);
      chain = new XrdCryptoX509Chain();
      if (XrdCryptosslX509ParseBucket(b, chain) <= 0) {
         PRINT("ERROR: no certificates in chain");
         delete b;
         delete chain; chain = 0;
         return -1;
      }
      if (chain->Reorder() < 0) {
         PRINT("ERROR: problems re-ordering proxy chain");
         delete b;
         delete chain; chain = 0;
         return -1;
      }
   }
   // Point to the last certificate
   XrdCryptoX509 *proxy = chain->End();
   if (!proxy) {
      PRINT("ERROR: chain is empty!");
      return -1;
   }
   // Get the DN
   const char *dn = proxy->Subject();
   int ldn = 0;
   if (!dn || (ldn = strlen(dn)) <= 0) {
      PRINT("ERROR: proxy dn undefined!");
      return -1;
   }
   
   // Set the key
   *key = new char[ldn+1];
   strcpy(*key, dn);
  
   // Done
   DEBUG("key is: '"<<*key<<"'");
   return 0;
}}

//
// Init the relevant parameters from a dedicated config file
//
extern "C"
{
int XrdSecgsiAuthzInit(const char *cfg)
{
   // Initialize the relevant parameters from the 'cfg' string.
   // Return -1 on failure.
   // Otherwise, the return code indicates the format required by the mai function for
   // the proxy chain:
   //                  0    proxy chain in 'raw' (opaque) format, to be processed
   //                       using the XrdCrypto tools 
   //                  1    proxy chain in 'PEM base64'
   EPNAME("AuthzInitDN");

   gCertfmt = 1;
   
   // Parse the config string
   XrdOucString cs(cfg), tkn;
   int from = 0;
   while ((from = cs.tokenize(tkn, from, ' ')) != -1) {
      if (tkn == "certfmt=raw") {
         gCertfmt = 0;
      }
   }
   // Notify
   PRINT("initialized! (certfmt:"<<gCertfmt<<")");
   
   // Done
   return gCertfmt;
}}

