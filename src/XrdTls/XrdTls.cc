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

#include <string.h>
#include <iostream>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "XrdSys/XrdSysE2T.hh"
#include "XrdTls/XrdTls.hh"

/******************************************************************************/
/*                    G l o b a l   D e f i n i t i o n s                     */
/******************************************************************************/

namespace
{
void ToStdErr(const char *tid, const char *msg, bool sslerr)
{
   std::cerr <<"TLS: " <<msg <<'\n' <<std::flush;
}
}

namespace XrdTlsGlobal
{
XrdTls::msgCB_t  msgCB = ToStdErr;
};

/******************************************************************************/
/*                       L o c a l   F u n c t i o n s                        */
/******************************************************************************/

namespace
{
int RC2SSL_Error(XrdTls::RC rc)
{
   switch(rc)
         {case XrdTls::TLS_AOK:             return SSL_ERROR_NONE;
               break;
          case XrdTls::TLS_CON_Closed:      return SSL_ERROR_ZERO_RETURN;
               break;
          case XrdTls::TLS_SSL_Error:       return SSL_ERROR_SSL;
               break;
          case XrdTls::TLS_SYS_Error:       return SSL_ERROR_SYSCALL;
               break;
          case XrdTls::TLS_WantRead:        return SSL_ERROR_WANT_READ;
               break;
          case XrdTls::TLS_WantWrite:       return SSL_ERROR_WANT_WRITE;
               break;
          case XrdTls::TLS_WantAccept:      return SSL_ERROR_WANT_ACCEPT;
               break;
          case XrdTls::TLS_WantConnect:     return SSL_ERROR_WANT_CONNECT;
               break;
          default: break;
         }
   return SSL_ERROR_SSL;
}
}
  
/******************************************************************************/
/*                           F l u s h E r r o r s                            */
/******************************************************************************/

void XrdTls::Emsg(const char *tid, const char *msg, bool flush)
{
  char emsg[2040];
  unsigned long eCode;

// Setup the trace ID
//
   if (!tid) tid = "TLS";

// Print passed in error, if any
//
  if (msg) XrdTlsGlobal::msgCB(tid, msg, false);

// Flush all openssl errors if so wanted
//
  if (flush)
     {while((eCode = ERR_get_error()))
           {ERR_error_string_n(eCode, emsg, sizeof(emsg));
            XrdTlsGlobal::msgCB(tid, emsg, true);
           }
     } else ERR_clear_error();
}
  
/******************************************************************************/
/*                               R C 2 T e x t                                */
/******************************************************************************/
  
std::string XrdTls::RC2Text(XrdTls::RC rc, bool dbg)
{
   char *eP, eBuff[1024];
   int ec;

   switch(rc)
         {case TLS_CRT_Missing:
               return std::string("x509 certificate is missing");
               break;
          case TLS_CTX_Missing:
               return std::string("context is missing");
               break;
          case TLS_SYS_Error:
               ec = errno;
               if (!ec) ec = EPIPE;
               snprintf(eBuff, sizeof(eBuff), "%s", XrdSysE2T(ec));
               *eBuff = tolower(*eBuff);
               eP = eBuff;
               break;
          case TLS_VER_Error:
               return std::string("x509 certificate verification failed");
               break;
          default:
               ERR_error_string_n(RC2SSL_Error(rc), eBuff, sizeof(eBuff));
               if (dbg) eP = eBuff;
                  else {char *colon = rindex(eBuff, ':');
                        eP = (colon ? colon+1 : eBuff);
                       }
         }
   return std::string(eP);
}

/******************************************************************************/
/*                              S e t M s g C B                               */
/******************************************************************************/

void XrdTls::SetMsgCB(XrdTls::msgCB_t cbP)
{
   XrdTlsGlobal::msgCB = (cbP ? cbP : ToStdErr);
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
