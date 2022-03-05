/******************************************************************************/
/*                                                                            */
/*                        X r d S y s T r a c e . h h                         */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

#include <cstdio>

#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysTrace.hh"
  
/******************************************************************************/
/*                  C o n v e r s i o n   F u n c t i o n s                   */
/******************************************************************************/

namespace
{
XrdSysTrace::msgCB_t msgCB = 0;

void ToMsgCB(int iovcnt, struct iovec *iov)
{
   std::string msg;
   int n = 0;

   for (int i = 2; i < iovcnt; i++) n += iov[i].iov_len;

   msg.reserve(n+1);

   for (int i = 2; i < iovcnt; i++)
       msg.append((const char *)iov[i].iov_base, iov[i].iov_len);

   msgCB((const char *)iov[1].iov_base, msg.c_str(), true);
}
}
  
/******************************************************************************/
/*                             S e t L o g g e r                              */
/******************************************************************************/

void XrdSysTrace::SetLogger(XrdSysLogger *logp) {logP = logp;}

void XrdSysTrace::SetLogger(msgCB_t cbP) {msgCB = cbP;}

/******************************************************************************/
/*                                   B e g                                    */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::Beg(const char *usr,
                              const char *epn,
                              const char *txt)
{
   char fmt[16];
   const char *fmt1, *fmt2, *fmt3;
   int n;

// Generate prefix format (way too complicated)
//
   if (usr) fmt1 = "%s ";
      else {usr  = "", fmt1 = "%s";}
   if (epn) fmt2 = "%s_%s: ";
      else {epn = ""; fmt2 =  "%s%s: ";}
   if (txt) fmt3 = "%s";
      else {txt = ""; fmt3 = "";}
   sprintf(fmt, "%s%s%s", fmt1, fmt2, fmt3);

// Format the header
//
   myMutex.Lock();
   n = snprintf(pBuff, sizeof(pBuff), fmt, usr, iName, epn, txt);
   if (n >= (int)sizeof(pBuff)) n = sizeof(pBuff)-1;

// Start the trace procedure
//
   ioVec[0].iov_base = 0;     ioVec[0].iov_len = 0;
   ioVec[1].iov_base = pBuff; ioVec[1].iov_len = n;

// Reset ourselves
//
   dPnt  = 0;
   dFree = txtMax;
   vPnt  = 2;

// All done
//
   return *this;
}

/******************************************************************************/
/*                                   E n d                                    */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::operator<<(XrdSysTrace *val)
{
   (void)val;

// Make sure an endline character appears
//
   if (vPnt >= iovMax) vPnt = iovMax-1;
   ioVec[vPnt]  .iov_base = (char *)"\n";
   ioVec[vPnt++].iov_len  = 1;

// Output the line
//
        if (logP)  logP->Put(vPnt, ioVec);
   else if (msgCB) ToMsgCB(vPnt, ioVec);
   else {static XrdSysLogger tLog(XrdSysFD_Dup(STDERR_FILENO), 0);
         tLog.Put(vPnt, ioVec);
        }

// All done
//
   myMutex.UnLock();
   return *this;
}

/******************************************************************************/
/*                                < < b o o l                                 */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::operator<<(bool val)
{

// If we have enough space then format the value
//
   if (vPnt < iovMax)
      {if (val)
          {ioVec[vPnt]  .iov_base = (char *)"True";
           ioVec[vPnt++].iov_len  = 4;
          } else {
           ioVec[vPnt]  .iov_base = (char *)"False";
           ioVec[vPnt++].iov_len  = 5;
          }
      }
   return *this;
}

/******************************************************************************/
/*                                < < c h a r                                 */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::operator<<(char val)
{
   static char hv[] = "0123456789abcdef";

// If we have enough space then format the value
//
   if (vPnt < iovMax && dFree > 1)
      {if (doFmt)
          {if (doFmt & Xrd::hex)
              {ioVec[vPnt].iov_base = (char *)(&dBuff[dPnt]);
               if (doFmt & Xrd::hex)
                  {ioVec[vPnt++].iov_len  = 2;
                   dBuff[dPnt++] = hv[(val >> 4) & 0x0f];
                   dBuff[dPnt++] = hv[ val       & 0xf0];
                   dFree -= 2;
                  } else if (doFmt & Xrd::oct && dFree > 2) {
                   ioVec[vPnt++].iov_len  = 3;
                   dBuff[dPnt+2] = hv[val & 0x07]; val >>= 3;
                   dBuff[dPnt+1] = hv[val & 0x07]; val >>= 3;
                   dBuff[dPnt+1] = hv[val & 0x03];
                   dPnt  += 3;
                   dFree -= 3;
                 }
              }
           if (doFmt & doOne) doFmt = Xrd::dec;
          } else {
           ioVec[vPnt]  .iov_base = (char *)(&dBuff[dPnt]);
           ioVec[vPnt++].iov_len  = 1;
           dBuff[dPnt++] = val; dFree--;
          }
      }
   return *this;
}

/******************************************************************************/
/*                        < < c o n s t   c h a r   *                         */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::operator<<(const char *val)
{

// If we have enough space then format the value
//
   if (vPnt < iovMax)
      {ioVec[vPnt]  .iov_base = (char *)val;
       ioVec[vPnt++].iov_len  = strlen(val);
      }
   return *this;
}


/******************************************************************************/
/*                        < < std::string                                     */
/******************************************************************************/
XrdSysTrace& XrdSysTrace::operator<<(const std::string& val)
{
   return (*this << val.c_str());
}

/******************************************************************************/
/*                               < < s h o r t                                */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::operator<<(short val)
{
   static const int xSz = sizeof("-32768");

// If we have enough space then format the value
//
   if (dFree >= xSz && vPnt < iovMax)
      {const char *fmt = (doFmt ? (doFmt & Xrd::hex ? "%hx" : "%ho") : "%hd");
       int n = snprintf(&dBuff[dPnt], dFree, fmt, val);
       if (n > dFree) dFree = 0;
          else {ioVec[vPnt]  .iov_base = &dBuff[dPnt];
                ioVec[vPnt++].iov_len  = n;
                dPnt += n; dFree -= n;
               }
      }
   if (doFmt & doOne) doFmt = Xrd::dec;
   return *this;
}

/******************************************************************************/
/*                                 < < i n t                                  */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::operator<<(int val)
{
   static const int xSz = sizeof("-2147483648");

// If we have enough space then format the value
//
   if (dFree >= xSz && vPnt < iovMax)
      {const char *fmt = (doFmt ? (doFmt & Xrd::hex ? "%x" : "%o") : "%d");
       int n = snprintf(&dBuff[dPnt], dFree, fmt, val);
       if (n > dFree) dFree = 0;
          else {ioVec[vPnt]  .iov_base = &dBuff[dPnt];
                ioVec[vPnt++].iov_len  = n;
                dPnt += n; dFree -= n;
               }
      }
   if (doFmt & doOne) doFmt = Xrd::dec;
   return *this;
}

/******************************************************************************/
/*                                < < l o n g                                 */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::operator<<(long val)
{

// Fan out based on length of a long
//
   if (sizeof(long) > 4) return *this<<static_cast<long long>(val);
      else               return *this<<static_cast<int>(val);
}

/******************************************************************************/
/*                           < < l o n g   l o n g                            */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::operator<<(long long val)
{
   static const int xSz = sizeof("-9223372036854775808");

// If we have enough space then format the value
//
   if (dFree >= xSz && vPnt < iovMax)
      {const char *fmt = (doFmt ? (doFmt & Xrd::hex ? "%llx" : "%llo") : "%lld");
       int n = snprintf(&dBuff[dPnt], dFree, fmt, val);
       if (n > dFree) dFree = 0;
          else {ioVec[vPnt]  .iov_base = &dBuff[dPnt];
                ioVec[vPnt++].iov_len  = n;
                dPnt += n; dFree -= n;
               }
      }
   if (doFmt & doOne) doFmt = Xrd::dec;
   return *this;
}

/******************************************************************************/
/*                      < < u n s i g n e d   s h o r t                       */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::operator<<(unsigned short val)
{
   static const int xSz = sizeof("65535");

// If we have enough space then format the value
//
   if (dFree >= xSz && vPnt < iovMax)
      {const char *fmt = (doFmt ? (doFmt & Xrd::hex ? "%hx" : "%ho") : "%hu");
       int n = snprintf(&dBuff[dPnt], dFree, fmt, val);
       if (n > dFree) dFree = 0;
          else {ioVec[vPnt]  .iov_base = &dBuff[dPnt];
                ioVec[vPnt++].iov_len  = n;
                dPnt += n; dFree -= n;
               }
      }
   if (doFmt & doOne) doFmt = Xrd::dec;
   return *this;
}

/******************************************************************************/
/*                        < < u n s i g n e d   i n t                         */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::operator<<(unsigned int val)
{
   static const int xSz = sizeof("4294967295");

// If we have enough space then format the value
//
   if (dFree >= xSz && vPnt < iovMax)
      {const char *fmt = (doFmt ? (doFmt & Xrd::hex ? "%x" : "%o") : "%u");
       int n = snprintf(&dBuff[dPnt], dFree, fmt, val);
       if (n > dFree) dFree = 0;
          else {ioVec[vPnt]  .iov_base = &dBuff[dPnt];
                ioVec[vPnt++].iov_len  = n;
                dPnt += n; dFree -= n;
               }
      }
   if (doFmt & doOne) doFmt = Xrd::dec;
   return *this;
}

/******************************************************************************/
/*                       < < u n s i g n e d   l o n g                        */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::operator<<(unsigned long val)
{

// Fan out based on length of a long
//
   if (sizeof(long) > 4) return *this<<static_cast<unsigned long long>(val);
      else               return *this<<static_cast<unsigned int>(val);
}

/******************************************************************************/
/*                  < < u n s i g n e d   l o n g   l o n g                   */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::operator<<(unsigned long long val)
{
   static const int xSz = sizeof("18446744073709551615");

// If we have enough space then format the value
//
   if (dFree >= xSz && vPnt < iovMax)
      {const char *fmt = (doFmt ? (doFmt & Xrd::hex ? "%llx" : "%llo") : "%llu");
       int n = snprintf(&dBuff[dPnt], dFree, fmt, val);
       if (n > dFree) dFree = 0;
          else {ioVec[vPnt]  .iov_base = &dBuff[dPnt];
                ioVec[vPnt++].iov_len  = n;
                dPnt += n; dFree -= n;
               }
      }
   if (doFmt & doOne) doFmt = Xrd::dec;
   return *this;
}

/******************************************************************************/
/*                              < < v o i d   *                               */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::operator<<(void *val)
{
   static const int xSz = sizeof(void *)*2+1;

// If we have enough space then format the value
//
   if (dFree >= xSz && vPnt < iovMax)
      {int n = snprintf(&dBuff[dPnt], dFree, "%p", val);
       if (n > dFree) dFree = 0;
          else {ioVec[vPnt]  .iov_base = &dBuff[dPnt];
                ioVec[vPnt++].iov_len  = n;
                dPnt += n; dFree -= n;
               }
      }
   return *this;
}

/******************************************************************************/
/*                         < < l o n g   d o u b l e                          */
/******************************************************************************/
  
XrdSysTrace& XrdSysTrace::Insert(long double val)
{
   char tmp[32];
   int  n;

// Gaurd against iovec overflows
//
if (vPnt < iovMax)
  {

// Convert the value into the temporary buffer
//
   n = snprintf(tmp, sizeof(tmp), "%Lg", val);

// If we have enough space then format the value
//
   if (dFree > n && n < (int)sizeof(tmp))
      {ioVec[vPnt]  .iov_base = &dBuff[dPnt];
       ioVec[vPnt++].iov_len  = n;
       strcpy(&dBuff[dPnt], tmp);
       dPnt += n; dFree -= n;
      }
  }
   return *this;
}
