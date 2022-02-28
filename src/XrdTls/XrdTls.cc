/******************************************************************************/
/*                                                                            */
/*                             X r d T l s . c c                              */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstring>
#include <iostream>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysTrace.hh"
#include "XrdTls/XrdTls.hh"
#include "XrdTls/XrdTlsTrace.hh"

/******************************************************************************/
/*                    G l o b a l   D e f i n i t i o n s                     */
/******************************************************************************/

namespace
{
void ToStdErr(const char *tid, const char *msg, bool sslerr)
{
   std::cerr <<"TLS: " <<msg <<'\n' <<std::flush;
}
XrdTls::msgCB_t msgCB = ToStdErr;
bool echoMsg = false;
}

namespace XrdTlsGlobal
{
XrdSysTrace      SysTrace("TLS",0);
};

/******************************************************************************/
/*                       L o c a l   F u n c t i o n s                        */
/******************************************************************************/

//namespace
//{
//int RC2SSL_Error(XrdTls::RC rc)
//{
//   switch(rc)
//         {case XrdTls::TLS_AOK:             return SSL_ERROR_NONE;
//               break;
//          case XrdTls::TLS_CON_Closed:      return SSL_ERROR_ZERO_RETURN;
//               break;
//          case XrdTls::TLS_SSL_Error:       return SSL_ERROR_SSL;
//               break;
//          case XrdTls::TLS_SYS_Error:       return SSL_ERROR_SYSCALL;
//               break;
//          case XrdTls::TLS_WantRead:        return SSL_ERROR_WANT_READ;
//               break;
//          case XrdTls::TLS_WantWrite:       return SSL_ERROR_WANT_WRITE;
//               break;
//          case XrdTls::TLS_WantAccept:      return SSL_ERROR_WANT_ACCEPT;
//               break;
//          case XrdTls::TLS_WantConnect:     return SSL_ERROR_WANT_CONNECT;
//               break;
//          default: break;
//         }
//   return SSL_ERROR_SSL;
//}
//}
  
/******************************************************************************/
/*                                  E m s g                                   */
/******************************************************************************/

namespace
{
int ssl_msg_CB(const char *str, size_t len, void *u)
{   const char *tid = (const char *)u;
    msgCB(tid, str, true);
    if (echoMsg && msgCB != ToStdErr) ToStdErr(tid, str, true);
    return 0;
}
}

void XrdTls::Emsg(const char *tid, const char *msg, bool flush)
{

// Setup the trace ID
//
   if (!tid) tid = "TLS";

// Print passed in error, if any
//
  if (msg)
     {msgCB(tid, msg, false);
      if (echoMsg && msgCB != ToStdErr) ToStdErr(tid, msg, false);
     }

// Flush all openssl errors if so wanted
//
  if (flush) ERR_print_errors_cb(ssl_msg_CB, (void *)tid);
}
  
/******************************************************************************/
/*                               R C 2 T e x t                                */
/******************************************************************************/
  
std::string XrdTls::RC2Text(XrdTls::RC rc, bool dbg)
{
   switch(rc)
         {case TLS_CON_Closed:
               return std::string("connection closed");
               break;
          case TLS_CRT_Missing:
               return std::string("x509 certificate is missing");
               break;
          case TLS_CTX_Missing:
               return std::string("context is missing");
               break;
          case TLS_HNV_Error:
               return std::string("host name verification failed");
               break;
          case TLS_SSL_Error:
               return std::string("TLS fatal error");
               break;
          case TLS_SYS_Error:
               if (errno == 0) return std::string("socket error");
               return std::string( XrdSysE2T(errno));
               break;
          case TLS_UNK_Error:
               return std::string("unknown error occurred, sorry!");
               break;
          case TLS_VER_Error:
               return std::string("x509 certificate verification failed");
               break;
          case TLS_WantAccept:
               return std::string("unhandled TLS accept");
               break;
          case TLS_WantConnect:
               return std::string("unhandled TLS connect");
               break;
          case TLS_WantRead:
               return std::string("unhandled TLS read want");
               break;
          case TLS_WantWrite:
               return std::string("unhandled TLS write want");
               break;

          default: break;
         }
  return std::string("unfathomable error occurred!");
}

/******************************************************************************/
/*                              S e t D e b u g                               */
/******************************************************************************/

void XrdTls::SetDebug(int opts, XrdSysLogger *logP)
{
   XrdTlsGlobal::SysTrace.SetLogger(logP);
   XrdTlsGlobal::SysTrace.What = opts;
   echoMsg = (opts & dbgOUT) != 0;
}

/******************************************************************************/

void XrdTls::SetDebug(int opts, XrdTls::msgCB_t cbP)
{
   XrdTlsGlobal::SysTrace.SetLogger(cbP);
   XrdTlsGlobal::SysTrace.What = opts;
}
  
/******************************************************************************/
/*                              S e t M s g C B                               */
/******************************************************************************/

void XrdTls::SetMsgCB(XrdTls::msgCB_t cbP)
{
   msgCB = (cbP ? cbP : ToStdErr);
}

/******************************************************************************/
/*                                s s l 2 R C                                 */
/******************************************************************************/
  
XrdTls::RC XrdTls::ssl2RC(int sslrc)
{
// Convert SSL error code to the TLS one
//
   switch(sslrc)
         {case SSL_ERROR_NONE:           return TLS_AOK;
               break;
          case SSL_ERROR_ZERO_RETURN:    return TLS_CON_Closed;
               break;
          case SSL_ERROR_WANT_READ:      return TLS_WantRead;
               break;
          case SSL_ERROR_WANT_WRITE:     return TLS_WantWrite;
               break;
          case SSL_ERROR_WANT_ACCEPT:    return TLS_WantAccept;
               break;
          case SSL_ERROR_WANT_CONNECT:   return TLS_WantConnect;
               break;
          case SSL_ERROR_SYSCALL:        return TLS_SYS_Error;
               break;
          case SSL_ERROR_SSL:            return TLS_SSL_Error;
               break;
          default: break;
         }
   return TLS_UNK_Error;
}

/******************************************************************************/
/*                              s s l 2 T e x t                               */
/******************************************************************************/
  
const char *XrdTls::ssl2Text(int sslrc, const char *dflt)
{
// Convert SSL error code to the TLS one
//
   switch(sslrc)
         {case SSL_ERROR_NONE:             return "error_none";
               break;
          case SSL_ERROR_ZERO_RETURN:      return "zero_return";
               break;
          case SSL_ERROR_WANT_READ:        return "want_read";
               break;
          case SSL_ERROR_WANT_WRITE:       return "want_write";
               break;
          case SSL_ERROR_WANT_ACCEPT:      return "want_accept";
               break;
          case SSL_ERROR_WANT_CONNECT:     return "want_connect";
               break;
          case SSL_ERROR_WANT_X509_LOOKUP: return "want_x509_lookup";
               break;
          case SSL_ERROR_SYSCALL:          return "error_syscall";
               break;
          case SSL_ERROR_SSL:              return "error_ssl";
               break;
          default:                         return dflt;
         }
}

/******************************************************************************/
/*                      C l e a r E r r o r Q u e u e                         */
/******************************************************************************/
void XrdTls::ClearErrorQueue()
{
  ERR_clear_error();
}
