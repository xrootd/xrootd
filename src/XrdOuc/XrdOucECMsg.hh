#ifndef __OUC_ECMSG_H__
#define __OUC_ECMSG_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d O u c E C M s g . h h                         */
/*                                                                            */
/* (c) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
/*                                                                            */
/******************************************************************************/

#include <cstdarg>
#include <string>

#include "XrdSys/XrdSysPthread.hh"

class XrdOucECMsg
{
public:

//-----------------------------------------------------------------------------
//! Append subsequent message to contents of ecMsg using delimeter. Append
//! allows method chaining for a more natural progamming style.
//!
//! @param  dlm  !0 -> The character to use as message separator.
//!         dlm  =0 -> Turns off appending, next message replaces ecMsg.
//!
//! @return Reference to the subject object.
//-----------------------------------------------------------------------------

XrdOucECMsg& Append(char dlm='\n')
                   {ecMTX.Lock(); Delim = dlm; ecMTX.UnLock(); return *this;}

//-----------------------------------------------------------------------------
//! Get err code and error message.
//!
//! @param  ecm  Reference to std:string where message is to be placed.
//! @param  rst  When true, the eCode and internal string are set to null.
//!
//! @return the error code, eCode, and associated error message.
//-----------------------------------------------------------------------------

int   Get(std::string& ecm, bool rst=true);
int   Get() {return eCode;}

//-----------------------------------------------------------------------------
//! Determine if an error text message exists.
//!
//! @return true if an error text message exists, false otherwise.
//-----------------------------------------------------------------------------

bool  hasMsg() const {ecMTX.Lock(); bool hm = !ecMsg.empty();
                      ecMTX.UnLock(); return hm;
                     }

//-----------------------------------------------------------------------------
//! Return the message string.
//!
//! @return A reference to the message string.
//-----------------------------------------------------------------------------

std::string Msg() {ecMTX.Lock(); std::string m = ecMsg; ecMTX.UnLock();
                   return m;
                  }

//-----------------------------------------------------------------------------
//! Insert a space delimited error message into ecMsg string.
//!
//! @param  pfx  !0 -> the text to prefix the message; the message is formed as
//!                    pfx: txt1 [txt2] [txt3]
//!         pfx  =0 -> insert message into ecMsg without a prefix.
//! @param  txt1,txt2,txt3,txt4,txt5 the message to be inserted to ecMsg.
//-----------------------------------------------------------------------------

void  Msg(const char* pfx,    const char* txt1,
          const char* txt2=0, const char* txt3=0,
          const char* txt4=0, const char* txt5=0);

//-----------------------------------------------------------------------------
//! Insert a formated error message into ecMsg using variable args.
//!
//! @param  pfx  !0 -> the text to prefix the message; the message is formed as
//!                    <pfx>: <formated_text>
//!         pfx  =0 -> insert message without a prefix.
//! @param  fmt  the message formatting template (i.e. sprintf format).
//! @param  ...  the arguments that should be used with the template. The
//!              formatted message is truncated at 2048 bytes.
//-----------------------------------------------------------------------------

void  Msgf(const char *pfx, const char *fmt, ...);

//-----------------------------------------------------------------------------
//! Insert a formated error message into the ecMsg using a va_list.
//!
//! @param  pfx  !0 -> the text to prefix the message; the message is formed as
//!                    <pfx>: <formated_text>
//!         pfx  =0 -> add message to the log without a prefix.
//! @param  fmt  the message formatting template (i.e. sprintf format).
//! @param  aP   the arguments that should be used with the template. The
//!              formatted message is truncated at 2048 bytes.
//-----------------------------------------------------------------------------

void  MsgVA(const char *pfx, const char *fmt, std::va_list aP);

//-----------------------------------------------------------------------------
//! Insert a formated error message into ecMsg using an iovec.
//!
//! @param  pfx  !0 -> the text to prefix the message; the message is formed as
//!                    pfx: <iovec>
//!         pfx  =0 -> insert message into ecMsg without a prefix.
//! @param  vecP pointer to a vector strings to insert into the message.
//!              Spaces are not inserted between the elements.
//! @param  vecN the number of elements in vecP.
//-----------------------------------------------------------------------------

void  MsgVec(const char* pfx, char const* const* vecP, int vecN);

//-----------------------------------------------------------------------------
//! Set error message and error code.
//!
//! @param  ecc  The error code.
//! @param  ecm  The error message, if nil then message is not changed.
//-----------------------------------------------------------------------------

void  Set(int ecc, const char*  ecm="")
         {ecMTX.Lock(); eCode = ecc; if (ecm) ecMsg = ecm; ecMTX.UnLock();}

void  Set(int ecc, std::string& ecm)
         {ecMTX.Lock(); eCode = ecc; ecMsg = ecm; ecMTX.UnLock();}

//-----------------------------------------------------------------------------
//! Set default error message, error code, and errno.
//!
//! @param  ecc  The error code.
//! @param  ret  The value to be returned, default -1.
//! @param  alt  Alternative message, default text of ecc error.
//!
//! @return The ret parameter value is returned.
//-----------------------------------------------------------------------------

int   SetErrno(int ecc, int ret=-1, const char *alt=0);

//-----------------------------------------------------------------------------
//! Assignment operators for convenience.
//-----------------------------------------------------------------------------

   XrdOucECMsg& operator=(const int rhs)
           {ecMTX.Lock(); eCode = rhs; ecMTX.UnLock(); return *this;}

   XrdOucECMsg& operator=(const std::string& rhs)
           {ecMTX.Lock(); ecMsg = rhs; ecMTX.UnLock(); return *this;}

   XrdOucECMsg& operator=(const char* rhs)
           {ecMTX.Lock(); ecMsg = rhs; ecMTX.UnLock(); return *this;}

   XrdOucECMsg& operator=(XrdOucECMsg& rhs)
           {ecMTX.Lock(); ecMsg = rhs.ecMsg; eCode = rhs.eCode; ecMTX.UnLock();
            return *this;
           }

      XrdOucECMsg(const char *msgid=0) : msgID(msgid), eCode(0), Delim(0)  {}
     ~XrdOucECMsg() {}

private:

void        Setup(const char *pfx, int n);
mutable
XrdSysMutex ecMTX;
const char* msgID;
std::string ecMsg;
int         eCode;
char        Delim;
};
#endif
