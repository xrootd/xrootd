/******************************************************************************/
/*                                                                            */
/*                        X r d O u c E C M s g . c c                         */
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

#include <cstdio>
#include <string.h>

#include "XrdOuc/XrdOucECMsg.hh"
#include "XrdSys/XrdSysE2T.hh"

/******************************************************************************/
/*                                   G e t                                    */
/******************************************************************************/
  
int XrdOucECMsg::Get(std::string& ecm, bool rst)
{
   if (!rst)
      {ecm = ecMsg;
       return eCode;
      }

   int ec = eCode;
   eCode = 0;
   ecm = std::move(ecMsg);
   ecMsg.erase();
   return ec;
}

/******************************************************************************/
/*                                   M s g                                    */
/******************************************************************************/
  
void XrdOucECMsg::Msg(const char *pfx,  const char *txt1,
                      const char *txt2, const char *txt3,
                      const char *txt4, const char *txt5)
{

   const char *vecP[10];
   int n = 0;
   bool xSpace = false;

   if (txt1 && *txt1) {vecP[n++] = txt1; xSpace = true;}
   if (txt2 && *txt2) {if (xSpace) vecP[n++] = " ";
                       vecP[n++] = txt2; xSpace = true;
                      }
   if (txt3 && *txt3) {if (xSpace) vecP[n++] = " ";
                       vecP[n++] = txt3; xSpace = true;
                      }
   if (txt4 && *txt4) {if (xSpace) vecP[n++] = " ";
                       vecP[n++] = txt4; xSpace = true;
                      }
   if (txt5 && *txt5) {if (xSpace) vecP[n++] = " ";
                       vecP[n++] = txt5;
                      }

// Route the message appropriately
//
   MsgVec(pfx, vecP, n);
}

/******************************************************************************/
/*                                  M s g f                                   */
/******************************************************************************/

void XrdOucECMsg::Msgf(const char *pfx, const char *fmt, ...)
{
   char buffer[2048];
   va_list  args;
   va_start (args, fmt);

// Format the message
//
   int n = vsnprintf(buffer, sizeof(buffer), fmt, args);

// Append as needed
//
   if (n > (int)sizeof(buffer)) n = sizeof(buffer);
   Setup(pfx, n);
   ecMsg.append(buffer);
}

/******************************************************************************/
/*                                 M s g V A                                  */
/******************************************************************************/

void XrdOucECMsg::MsgVA(const char *pfx, const char *fmt, va_list aP)
{
   char buffer[2048];

// Format the message
//
   int n = vsnprintf(buffer, sizeof(buffer), fmt, aP);

// Append as needed
//
   if (n > (int)sizeof(buffer)) n = sizeof(buffer);
   Setup(pfx, n);
   ecMsg.append(buffer);
}

/******************************************************************************/
/*                                M s g V e c                                 */
/******************************************************************************/
  
void XrdOucECMsg::MsgVec(const char* pfx, char const* const* vecP, int vecN)
{
   int n = 0;

   for (int i = 0; i < vecN; i++) n += strlen(vecP[i]);
   Setup(pfx, n);
   for (int i = 0; i < vecN; i++) ecMsg.append(vecP[i]);
}

/******************************************************************************/
/*                              S e t E r r n o                               */
/******************************************************************************/

int XrdOucECMsg::SetErrno(int ecc, int ret, const char *alt)
{
   if (!alt || *alt != '*')
      {if (!msgID) ecMsg = (alt ? alt : XrdSysE2T(ecc));
          else Msgf(msgID, XrdSysE2T(ecc));
      }
   errno = eCode = ecc;
   return ret;
}
  
/******************************************************************************/
/*                                 S e t u p                                  */
/******************************************************************************/
  
void XrdOucECMsg::Setup(const char* pfx, int n)
{
   int k = (pfx && *pfx ? strlen(pfx)+2 : 0);
      
   if (Delim)
      {ecMsg.reserve(ecMsg.length() + n + k + 2);
       ecMsg.append(&Delim, 1);
       Delim = 0;
      } else {
       ecMsg.reserve(n + k + 1);
       ecMsg = "";
      }

    if (k)
       {ecMsg.append(pfx);
        ecMsg.append(": ");
       }
}
