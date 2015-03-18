/******************************************************************************/
/*                                                                            */
/*                       X r d S s i L o g g e r . c c                        */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/* Produced by Andrew Hanushevsky for Stanford University under contract      */
/*            DE-AC02-76-SFO0515 with the Deprtment of Energy                 */
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

#include <stdio.h>
#include <stdarg.h>

#include "XrdSsi/XrdSsiLogger.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysHeaders.hh"
 
/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdSsi
{
extern XrdSysError     Log;
extern XrdSysLogger   *Logger;
};

using namespace XrdSsi;

/******************************************************************************/
/*                                   M s g                                    */
/******************************************************************************/
  
void XrdSsiLogger::Msg(const char *pfx,  const char *txt1,
                       const char *txt2, const char *txt3)
{

// Route the message appropriately
//
   if (pfx) Log.Emsg(pfx, txt1, txt2, txt3);
      else {const char *tout[6] = {txt1, 0};
            int i = 1;
            if (txt2) {tout[i++] = " "; tout[i++] = txt2;}
            if (txt3) {tout[i++] = " "; tout[i++] = txt3;}
            tout[i] = txt3;
            Log.Say(tout[0], tout[1], tout[2], tout[3], tout[4], tout[5]);
           }
}

/******************************************************************************/
/*                                  M s g f                                   */
/******************************************************************************/

void XrdSsiLogger::Msgf(const char *pfx, const char *fmt, ...)
{
   char buffer[2048];
   va_list  args;
   va_start (args, fmt);

// Format the message
//
   vsnprintf(buffer, sizeof(buffer), fmt, args);

// Route it
//
   if (pfx) Log.Emsg(pfx, buffer);
      else  Log.Say(buffer);
}

/******************************************************************************/
/*                                  M s g v                                   */
/******************************************************************************/

void XrdSsiLogger::Msgv(const char *pfx, const char *fmt, va_list aP)
{
   char buffer[2048];

// Format the message
//
   vsnprintf(buffer, sizeof(buffer), fmt, aP);

// Route it
//
   if (pfx) Log.Emsg(pfx, buffer);
      else  Log.Say(buffer);
}

/******************************************************************************/
/*                                  T B e g                                   */
/******************************************************************************/
  
const char *XrdSsiLogger::TBeg() {return Logger->traceBeg();}

/******************************************************************************/
/*                                  T E n d                                   */
/******************************************************************************/
  
void XrdSsiLogger::TEnd()
{
   cerr <<endl;
   Logger->traceEnd();
}
