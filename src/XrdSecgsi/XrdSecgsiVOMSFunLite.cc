// $Id$

/******************************************************************************/
/*                                                                            */
/*             X r d S e c g s i V O M S F u n L i t e . c c                  */
/*                                                                            */
/* (c) 2012, G. Ganis / CERN                                                  */
/*                                                                            */
/******************************************************************************/

/* ************************************************************************** */
/*                                                                            */
/* Example of fucntion extracting VOMS attributes                             */
/*                                                                            */
/* To get it build as libXrdSecgsiVOMSLite.so, add the following to           */
/* src/XrdSecgsi.cmake                                                        */
/*                                                                            */
/* #-------------------------------------------------------------------------------
/* # The XrdSecgsiVOMSLite library
/* #-------------------------------------------------------------------------------
/* 
/* set( XRD_SEC_GSI_VOMSLITE_VERSION    1.0.0 )
/* set( XRD_SEC_GSI_VOMSLITE_SOVERSION  0 )
/*
/* add_library(
/*   XrdSecgsiVOMSLite
/*   SHARED
/*   XrdSecgsi/XrdSecgsiVOMSFunLite.cc )
/*
/* target_link_libraries(
/*   XrdSecgsiVOMSLite
/*   XrdSecgsi
/*   XrdCryptossl
/*   XrdCrypto
/*   XrdUtils )
/*
/* set_target_properties(
/*   XrdSecgsiVOMSLite
/*   PROPERTIES
/*   VERSION   ${XRD_SEC_GSI_VOMSLITE_VERSION}
/*   SOVERSION ${XRD_SEC_GSI_VOMSLITE_SOVERSION}
/*   LINK_INTERFACE_LIBRARIES "" )
/*                                                                            */
/* and make sure that XrdSecgsiVOMSLite is added to TARGETS in 'install'      */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <XrdCrypto/XrdCryptosslAux.hh>
#include <XrdCrypto/XrdCryptosslgsiAux.hh>
#include <XrdCrypto/XrdCryptoX509.hh>
#include <XrdCrypto/XrdCryptoX509Chain.hh>
#include <XrdOuc/XrdOucString.hh>
#include <XrdSec/XrdSecEntity.hh>
#include <XrdSecgsi/XrdSecgsiTrace.hh>
#include <XrdSut/XrdSutBucket.hh>

extern XrdOucTrace *gsiTrace;

#ifndef SafeFree
#define SafeFree(x) { if (x) free(x) ; x = 0; }
#endif

//
// Main function
//
extern "C"
{
int XrdSecgsiVOMSFun(XrdSecEntity &ent)
{
   // Implementation of XrdSecgsiAuthzFun extracting the information from the 
   // proxy chain in entity.creds
   EPNAME("VOMSFunLite");

   XrdCryptoX509Chain *c = (XrdCryptoX509Chain *) ent.creds;
   if (!c) {
      PRINT("ERROR: no proxy chain found!");
      return -1;
   }

   XrdCryptoX509 *xp = c->End();
   if (!xp) {
      PRINT("ERROR: no proxy certificate in chain!");
      return -1;
   }
   
   // Extract the information
   XrdOucString vatts;
   int rc = 0;
   if ((rc = XrdSslgsiX509GetVOMSAttr(xp, vatts)) != 0) {
      if (strstr(xp->Subject(), "CN=limited proxy")) {
         xp = c->SearchBySubject(xp->Issuer());
         rc = XrdSslgsiX509GetVOMSAttr(xp, vatts);
      }
      if (rc != 0) {
         if (rc > 0) {
            DEBUG("No VOMS attributes in proxy chain");
         } else {
            PRINT("ERROR: problem extracting VOMS attributes");
         }
         return -1;
      }      
   }

   int from = 0;
   XrdOucString vat;
   while ((from = vatts.tokenize(vat, from, ',')) != -1) {
      XrdOucString vo, role, grp;
      if (vat.length() > 0) {
         // The attribute is in the form
         //        /VO[/group[/subgroup(s)]][/Role=role][/Capability=cap]
         int isl = vat.find('/', 1);
         if (isl != STR_NPOS) vo.assign(vat, 1, isl - 1);
         int igr = vat.find("/Role=", 1);
         if (igr != STR_NPOS) grp.assign(vat, 0, igr - 1);
         int irl = vat.find("Role=");
         if (irl != STR_NPOS) {
            role.assign(vat, irl + 5);
            isl = role.find('/');
            role.erase(isl);
         }
         if (ent.vorg) {
            if (vo != (const char *) ent.vorg) {
               DEBUG("WARNING: found a second VO ('"<<vo<<"'): keeping the first one ('"<<ent.vorg<<"')");
               // We do not mix-up role settings ...
               continue;
            }
         } else {
            if (vo.length() > 0) ent.vorg = strdup(vo.c_str());
         }
         if (grp.length() > 0 && (!ent.grps || grp.length() > strlen(ent.grps))) {
            SafeFree(ent.grps);
            ent.grps = strdup(grp.c_str());
         }
         if (role.length() > 0 && role != "NULL" && !ent.role) {
            ent.role = strdup(role.c_str());
         }
      }
   }

   // Save the whole string in endorsements
   SafeFree(ent.endorsements);
   if (vatts.length() > 0) ent.endorsements = strdup(vatts.c_str());
   
   // Notify if did not find the main info (the VO ...)
   if (!ent.vorg) {
      PRINT("WARNING: no VO found! (VOMS attributes: '"<<vatts<<"')");
   } else {
      PRINT("VOMS attributes: '"<<vatts<<"'");
   }

   // Done
   return (!ent.vorg ? -1 : 0);
}}

//
// Init the relevant parameters from a dedicated config file
//
extern "C"
{
int XrdSecgsiVOMSInit(const char *cfg)
{
   // Initialize the relevant parameters from the 'cfg' string.
   // Return -1 on failure.
   // Otherwise, the return code indicates the format required by the mai function for
   // the proxy chain:
   //                  0    proxy chain in 'raw' (opaque) format, to be processed
   //                       using the XrdCrypto tools 
   //                  1    proxy chain in 'PEM base64'
   EPNAME("VOMSInitLite");

   // Notify
   PRINT("initialized! (certfmt: 'raw')");
   
   // Done
   return 0;
}}

