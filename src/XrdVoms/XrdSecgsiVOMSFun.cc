/******************************************************************************/
/*                                                                            */
/*                X r d S e c g s i V O M S F u n . c c                       */
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

/******************************************************************************/
/*                                                                            */
/*  See README.md for hints about usage of this library                       */
/*                                                                            */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "XrdSecgsiVOMS.hh"
#include "XrdSecgsiVOMSTrace.hh"

#ifdef HAVE_XRDCRYPTO
#include "XrdCrypto/XrdCryptoX509.hh"
#include "XrdCrypto/XrdCryptoX509Chain.hh"
#endif
#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSys/XrdSysLogger.hh"

#ifndef SafeFree
#define SafeFree(x) { if (x) free(x) ; x = 0; }
#endif

//
// These settings are configurable
#ifdef HAVE_XRDCRYPTO
static int gCertFmt = 0;                       //  certfmt:raw|pem|x509 [raw]
#else
static int gCertFmt = 1;                       //  certfmt:pem|x509 [pem]
#endif
static int gGrpSel = 0;                        //  grpopt's sel = 0|1 [0]
static int gGrpWhich = 2;                      //  grpopt's which = 0|1|2 [2]
static XrdOucHash<int> gGrps;                  //  hash table with grps=grp1[,grp2,...]
static XrdOucHash<int> gVOs;                   //  hash table with vos=vo1[,vo2,...]
static bool gDebug = 0;                        //  Verbosity control
static XrdOucString gRequire;                  //  String with configuration options
static XrdOucString gGrpFmt;                   //  String to be used to format the content of XrdSecEntity::grps
static XrdOucString gRoleFmt;                  //  String to be used to format the content of XrdSecEntity::role
static XrdOucString gVoFmt;                    //  String to be used to format the content of XrdSecEntity::vorg

static XrdSysError gDest(0, "secgsiVOMS_");
static XrdSysLogger gLogger;

#define VOMSDBG(m) \
   if (gDebug) { \
      PRINT(m); \
   } else { \
      DEBUG(m); \
   }
#define VOMSDBGSUBJ(m, c) \
   if (gDebug) { \
      XrdOucString subject; \
      NameOneLine(X509_get_subject_name(c), subject); \
      PRINT(m << subject); \
   }
#define VOMSREPLACE(a, f, e) \
   if (a.length() > 0) { \
      f.replace("<g>", e.grps); \
      f.replace("<r>", e.role); \
      f.replace("<vo>", e.vorg); \
      f.replace("<an>", e.endorsements); \
   }
#define VOMSSPTTAB(a) \
   if (a.length() > 0) { \
      int sp = -1; \
      while ((sp = a.find(' ', sp+1)) != STR_NPOS) { a[sp] = '\t'; } \
   }

//
// Function to convert X509_NAME into a one-line human readable string
static void NameOneLine(X509_NAME *nm, XrdOucString &s)
{
   BIO *mbio = BIO_new(BIO_s_mem());
   X509_NAME_print_ex(mbio, nm, 0, XN_FLAG_COMPAT);
   char *data = 0;
   long len = BIO_get_mem_data(mbio, &data);
   s = "/";
   s.insert(data, 1, len);
   BIO_free(mbio);
   s.replace(", ", "/");

   // Done
   return;
}

//
// Function to convert X509_NAME into a one-line human readable string
static void FmtReplace(XrdSecEntity &ent)
{
   XrdOucString gf(gGrpFmt), rf(gRoleFmt), vf(gVoFmt);

   VOMSREPLACE(gGrpFmt, gf, ent);
   VOMSREPLACE(gRoleFmt, rf, ent);
   VOMSREPLACE(gVoFmt, vf, ent);

   if (gf.length() > 0) {
      SafeFree(ent.grps);
      ent.grps = strdup(gf.c_str());
   }   
   if (rf.length() > 0) {
      SafeFree(ent.role);
      ent.role = strdup(rf.c_str());
   }   
   if (vf.length() > 0) {
      SafeFree(ent.vorg);
      ent.vorg = strdup(vf.c_str());
   }   
}

static void FmtExtract(XrdOucString &out, XrdOucString in, const char *tag)
{
   // Output group format string
   int igf = in.find(tag);
   if (igf != STR_NPOS) {
      int from = igf + strlen(tag);
      if (in[from+1] == '"') {
         out.assign(in, igf + from + 1);
         out.erase(out.find('"'));
      } else {
         out.assign(in, igf + from);
      }
   }
}

//
// Main function
//
extern "C"
{
int XrdSecgsiVOMSFun(XrdSecEntity &ent)
{
   // Implementation of XrdSecgsiAuthzFun extracting the information from the 
   // proxy chain in entity.creds
   EPNAME("Fun");

   vomsdata v;
   X509 *pxy = 0;
   STACK_OF(X509) *stk = 0;
   int freestk = 1;
   
   if (gCertFmt == 0) {
#ifdef HAVE_XRDCRYPTO
      //
      // RAW format
      //
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
      pxy = (X509 *) xp->Opaque();
      VOMSDBGSUBJ("proxy: ", pxy)
      freestk = 2;

      stk =sk_X509_new_null();
      XrdCryptoX509 *xxp = c->Begin();
      while (xxp) {
         if (xxp == c->End()) break;
         if (xxp->type != XrdCryptoX509::kCA) {
            VOMSDBGSUBJ("adding cert: ", (X509 *) xxp->Opaque())
            sk_X509_push(stk, (X509 *) xxp->Opaque());
         }
         xxp = c->Next();
      }
#else
      //
      // Do not have support for RAW format
      //
      PRINT("ERROR: compiled without support for RAW format! Re-run with 'certfmt=pem'");
      return -1;
#endif
   } else if (gCertFmt == 1) {
      //
      // PEM format
      //
      // Create a bio_mem to store the certificates
      BIO *bmem = BIO_new(BIO_s_mem());
      if (!bmem) {
         PRINT("unable to create BIO for memory operations");
         return -1; 
      }

      // Write data to BIO
      int nw = BIO_write(bmem, (const void *)(ent.creds), ent.credslen);
      if (nw != ent.credslen) {
         PRINT("problems writing data to memory BIO (nw: "<<nw<<")");
         BIO_free(bmem);
         return -1; 
      }

      // Get certificate from BIO
      if (!(pxy = PEM_read_bio_X509(bmem,0,0,0))) {
         PRINT("unable to read certificate to memory BIO");
         BIO_free(bmem);
         return -1;
      }
      VOMSDBGSUBJ("proxy: ", pxy)
      //
      // The chain now
      X509 *xc = 0;
      stk =sk_X509_new_null();
      while ((xc =  PEM_read_bio_X509(bmem,0,0,0))) {
         VOMSDBGSUBJ("adding cert: ", xc)
         sk_X509_push(stk, xc);
      }
      //
      // Free BIO
      BIO_free(bmem);

   } else {
      //
      // STACK_OF(X509) format
      //
      gsiVOMS_x509_in_t *voms_in = (gsiVOMS_x509_in_t *) ent.creds;
      pxy = voms_in->cert;
      stk = voms_in->chain;
      freestk = 0;
   }

   bool extfound = 0;
   XrdOucString endor, grps, role, vo, xendor, xgrps, xrole, xvo;
   if (v.Retrieve(pxy, stk, RECURSE_CHAIN)) {
      VOMSDBG("retrieval successful");
      extfound = 1;
      std::vector<voms>::iterator i = v.data.begin();
      for ( ; i != v.data.end(); i++) {
         VOMSDBG("found VO: " << (*i).voname);
         xvo = (*i).voname.c_str(); VOMSSPTTAB(xvo);
         // Filter the VO? (*i) is voms
         if (gVOs.Num() > 0 && !gVOs.Find((*i).voname.c_str())) continue;
         // Save VO name (in tuple mode this is done later, in the loop over groups)
         if (gGrpWhich < 2) vo = xvo;
         std::vector<data> dat = (*i).std;
         std::vector<data>::iterator idat = dat.begin();
         // Same size as std::vector<data> by construction (same information in compact form)
         std::vector<std::string> fqa = (*i).fqan;
         std::vector<std::string>::iterator ifqa = fqa.begin();
         grps = ""; role = "";
         for (; idat != dat.end(); idat++, ifqa++) {
            VOMSDBG(" ---> group: '"<<(*idat).group<<"', role: '"<<(*idat).role<<"', cap: '" <<(*idat).cap<<"'");
            VOMSDBG(" ---> fqan: '"<<(*ifqa)<<"'");
            xgrps = (*idat).group.c_str(); VOMSSPTTAB(xgrps);
            xrole = (*idat).role.c_str(); VOMSSPTTAB(xrole);
            xendor = (*ifqa).c_str(); VOMSSPTTAB(xendor);
            bool fillgrp = 1;
            if (gGrpSel == 1 && !gGrps.Find((*idat).group.c_str())) fillgrp = 0;
            if (fillgrp) {
               if (gGrpWhich == 2) {
                  if (vo.length() > 0) vo += " ";
                  vo += (*i).voname.c_str();
                  if (grps.length() > 0) grps += " ";
                  grps += (*idat).group.c_str();
                  if (role.length() > 0) role += " ";
                  role += (*idat).role.c_str();
                  if (endor.length() > 0) endor += ",";
                  endor += (*ifqa).c_str();
               } else {
                  grps = (*idat).group.c_str();
                  role = (*idat).role.c_str();
                  endor = (*ifqa).c_str();
               }
            }
            // If we are asked to take the first we break
            if (gGrpWhich == 0 && grps.length() > 0) break;            
         }
         if (grps.length() <= 0) {
            // Reset all the fields
            role = "";
            vo = "";
            endor = "";
         }
      }
      // Save the information found
      SafeFree(ent.vorg);
      SafeFree(ent.grps);
      SafeFree(ent.role);
      SafeFree(ent.endorsements);
      if (vo.length() > 0 && (gGrpSel == 0 || grps.length() > 0)) {
         ent.vorg = strdup(vo.c_str());
         // Save the groups
         if (grps.length() > 0) ent.grps = strdup(grps.c_str());
         if (role.length() > 0) ent.role = strdup(role.c_str());
         // Save the whole string in endorsements
         if (endor.length() > 0) ent.endorsements = strdup(endor.c_str());
      } else if (extfound) {
         VOMSDBG("VOMS extensions do not match required criteria ("<<gRequire<<")");
      }
   } else {
      PRINT("retrieval FAILED: "<< v.ErrorMessage());
   }

   // Fix spaces in XrdSecEntity::name
   char *sp = 0;
   while ((sp = strchr(ent.name, ' '))) { *sp = '\t'; }

   // Adjust the output format, if required
   FmtReplace(ent);

   // Free memory taken by the chain, if required
   if (stk && freestk > 0) {
      if (freestk == 1) {
         sk_X509_pop_free(stk, X509_free);
         X509_free(pxy);
      } else if (freestk == 2) {
         while (sk_X509_pop(stk)) { }
         sk_X509_free(stk);
      }
   }
   
   // Success or failure?
   int rc = !ent.vorg ? -1 : 0;
   if (rc == 0 && gGrpSel == 1 && !ent.grps) rc = -1;
   
   // Done
   return rc;
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
   // Otherwise, the return code indicates the format required by the main function
   // defined by 'certfmt' below.
   //
   // Supported options:
   //         certfmt=raw|pem|x509   Certificate format:  [raw]
   //                                  raw   to be used with XrdCrypto tools
   //                                  pem   PEM base64 format (as in cert files)
   //                                  x509  As a STACK_OF(X509)
   //         grpopt=opt               What to do with the group names:  [1]
   //                                    opt = sel * 10 + which
   //                                  with 'sel'
   //                                    0    consider all those present
   //                                    1    select among those specified by 'grps' (see below)
   //                                  and 'which'
   //                                    0    take the first one
   //                                    1    take the last
   //                                    2    take all (comma separated)
   //         grps=grp1[,grp2,...]   Group(s) for which the information is extracted; if specified
   //                                the grpopt 'sel' is set to 1 regardless of the setting.
   //         vos=vo1[,vo2,...]      VOs to be considered; the first match is taken
   //         grpfmt=<string>        String to be used to format the content of XrdSecEntity::grps
   //         rolefmt=<string>       String to be used to format the content of XrdSecEntity::role
   //         vofmt=<string>         String to be used to format the content of XrdSecEntity::vorg
   //                                Recognized place holders in the above format strings:
   //                                   <r>   role, as resulting from the parsing procedure
   //                                   <g>   group
   //                                   <vo>  VO
   //                                   <an>  Full Qualified Attribute Name
   //                                For example, rolefmt=<r>,grpfmt=<r> will inverse the group and
   //                                role in the output XrdSecEntity
   //         dbg                    To force verbose mode
   //
   EPNAME("Init");
   gDest.logger(&gLogger);
   gsiVOMSTrace = new XrdOucTrace(&gDest);

   XrdOucString oos(cfg);

   XrdOucString fmt, go, grps, voss, gfmt, rfmt, vfmt, sdbg;
   XrdOucString gr, vo, ss;
   if (oos.length() > 0) {

#define NTAG 8
      XrdOucString *var[NTAG] = { &fmt, &go, &grps, &voss, &gfmt, &rfmt, &vfmt, &sdbg};
      const char *tag[] = {"certfmt=", "grpopt=", "grps=", "vos=",
                           "grpfmt=", "rolefmt=", "vofmt=", "dbg"};
      int jb[NTAG], je[NTAG];

      // Begin of ranges
      int i = 0, j = -1;
      for(; i < NTAG; i++) {
         jb[i] = -1;
         int j = oos.find(tag[i]);
         if (j != STR_NPOS) jb[i] = j;
         DEBUG("i:"<<i<<" = "<<j);
      }
      // End of ranges
      for(i = 0; i < NTAG; i++) {
         je[i] = -1;
         DEBUG("-------------");
         if (jb[i] > -1) {
            int k = -1;
            for(j = 0; j < NTAG; j++) {
               if (j != i) {
                  if (jb[j] > jb[i] && (k < 0 || jb[j] < jb[k])) k = j; 
                  DEBUG("jb[" << j << "] = " << jb[j] <<" jb[ "<< i<<"] = "<<jb[i] << "  ->  k:" << k);
               }
            }
            if (k >= 0) {
               je[i] = jb[k] - 2;
            } else {
               je[i] = oos.length() - 1;
            }
            if (i != NTAG-1) {
               ss.assign(oos, jb[i], je[i]-jb[i]+1);
               FmtExtract(*var[i], ss, tag[i]);
               DEBUG(" s:\"" << ss << "\" (" << *var[i] <<")");
            } else {
               var[i]->assign(oos, jb[i], je[i]-jb[i]+1);
               DEBUG(" s:\"" << *var[i] << "\"");
            }
         }
         DEBUG("jb["<<i<<"] = "<<jb[i] <<"  --->  "<< "je["<<i<<"] = "<<je[i]);
      }


      // Certificate format
      if (fmt.length() > 0) {
         if (fmt == "raw") {
#ifdef HAVE_XRDCRYPTO
            gCertFmt = 0;
#else
            //
            // Do not have support for RAW format
            //
            PRINT("WARNING: support for RAW format not available: forcing PEM");
            gCertFmt = 1;
#endif
         } else if (fmt == "pem") {
            gCertFmt = 1;
         } else if (fmt == "x509") {
            gCertFmt = 2;
         }
      }

      // Group option
      if (go.length() > 0) {
         if (go.isdigit()) {
            int grpopt = go.atoi();
            gGrpSel = grpopt / 10;
            if (gGrpSel != 0 && gGrpSel != 1) {
               gGrpSel = 0;
               PRINT("WARNING: grpopt sel must be in [0,1] - ignoring");
            }
            gGrpWhich = grpopt % 10;
            if (gGrpWhich != 0 && gGrpWhich != 1 && gGrpWhich != 2) {
               gGrpWhich = 1;
               PRINT("WARNING: grpopt which must be in [0,2] - ignoring");
            }
         } else {
            PRINT("WARNING: you must pass a digit to grpopt: "<<go);
         }
         gRequire = "grpopt="; gRequire += go;
      }

      // Groups selection
      if (grps.length() > 0) {
         int from = 0, flag = 1;
         while ((from = grps.tokenize(gr, from, ',')) != -1) {
            // Analyse tok
            VOMSSPTTAB(gr);
            gGrps.Add(gr.c_str(), &flag);
            gGrpSel = 1;
         }
         if (gRequire.length() > 0) gRequire += ";";
         gRequire += "grps="; gRequire += grps;
      }

      // VO selection
      if (voss.length() > 0) {
         int from = 0, flag = 1;
         while ((from = voss.tokenize(vo, from, ',')) != -1) {
            // Analyse tok
            VOMSSPTTAB(vo);
            gVOs.Add(vo.c_str(), &flag);
         }
         if (gRequire.length() > 0) gRequire += ";";
         gRequire += "vos="; gRequire += voss;
      }

      // Output group format string
      FmtExtract(gGrpFmt, gfmt, "grpfmt=");
      // Output role format string
      FmtExtract(gRoleFmt, rfmt, "rolefmt=");
      // Output vo format string
      FmtExtract(gVoFmt, vfmt, "vofmt=");

      // Verbose mode
      if (sdbg == "dbg") gDebug = 1;
   }

   // Notify
   const char *cfmt[3] = { "raw", "pem base64", "STACK_OF(X509)" };
   const char *cgrs[2] = { "all", "specified group(s)"};
   const char *cgrw[3] = { "first", "last", "all" };
   PRINT("++++++++++++++++++ VOMS plug-in +++++++++++++++++++++++++++++++");
   PRINT("+++ proxy fmt:    "<< cfmt[gCertFmt]);
   PRINT("+++ group option: "<<cgrw[gGrpWhich]<<" of "<<cgrs[gGrpSel]);
   if (gGrpSel == 1) {
      if (grps.length() > 0) {
         PRINT("+++ group(s):     "<< grps);
      } else {
         PRINT("+++ group(s):      <not specified>");
      }
   }
   if (gGrpFmt.length() > 0)
      PRINT("+++ grps fmt:     "<< gGrpFmt);
   if (gRoleFmt.length() > 0)
      PRINT("+++ role fmt:     "<< gRoleFmt);
   if (gVoFmt.length() > 0)
      PRINT("+++ vorg fmt:     "<< gVoFmt);
   if (gVOs.Num() > 0) PRINT("+++ VO(s):        "<< voss);
   PRINT("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
   // Done
   return gCertFmt;
}}
