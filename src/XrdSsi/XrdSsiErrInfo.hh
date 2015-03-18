#ifndef __XRDSSIERRINFO_HH__
#define __XRDSSIERRINFO_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d S s i E r r I n f o . h h                       */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdlib.h>
#include <string.h>
  
//-----------------------------------------------------------------------------
//! The XrdSsiErrInfo object is used to hold error information for many ssi
//! client-oriented requests.
//-----------------------------------------------------------------------------

class XrdSsiErrInfo
{
public:

//-----------------------------------------------------------------------------
//! Reset and clear error information.
//-----------------------------------------------------------------------------

       void  Clr() {if (errText) {free(errText); errText = 0;};
                    errArg = errNum = 0;
                   }

//-----------------------------------------------------------------------------
//! Determine whether or not error information is present.
//!
//! @return true  Error information is  present.
//! @return false Error information not present.
//-----------------------------------------------------------------------------

       bool  Dirty() {return errText != 0;}

//-----------------------------------------------------------------------------
//! Get current error information.
//!
//! @param  eNum  place where the error number is to be placed.
//!
//! @return =0    no error information is available, eNum also is zero.
//! @return !0    pointer to a string describing the error associated with eNum.
//!               The pointer is valid until the object is deleted or Set().
//-----------------------------------------------------------------------------

const  char *Get(int &eNum)
                {if (!(eNum = errNum)) return 0;
                 return errText;
                }

//-----------------------------------------------------------------------------
//! Get current error argument.
//!
//! @return       the error argument value.
//-----------------------------------------------------------------------------

       int   GetArg() {return errArg;}

//-----------------------------------------------------------------------------
//! Set new error information.
//!
//! @param  eMsg  pointer to a string describing the error. If zero, the eNum
//!               is taken as errno and strerror(eNum) is used.
//! @param  eNum  the error number associated with the error.
//! @param  eArg  the error argument, if any (see XrdSsiService::Provision()).
//-----------------------------------------------------------------------------

       void  Set(const char *eMsg=0, int eNum=0, int eArg=0)
                {if (errText) free(errText);
                 errText = strdup((eMsg && *eMsg ? eMsg : strerror(eNum)));
                 errNum  = eNum;
                 errArg  = eArg;
                }

//------------------------------------------------------------------------------
//! Assignment operator
//------------------------------------------------------------------------------

XrdSsiErrInfo &operator=(XrdSsiErrInfo const &rhs)
               {if (&rhs != this)
                   {errArg = rhs.errArg;
                    errNum = rhs.errNum;
                    if (rhs.errText == 0) errText = 0;
                       else errText = strdup(rhs.errText);
                   }
                return *this;
               }

//------------------------------------------------------------------------------
//! Copy constructor
//------------------------------------------------------------------------------

               XrdSsiErrInfo(XrdSsiErrInfo const &oP) {*this = oP;}

//-----------------------------------------------------------------------------
//! Constructor and Destructor
//-----------------------------------------------------------------------------

      XrdSsiErrInfo() : errText(0), errNum(0), errArg(0) {}

     ~XrdSsiErrInfo() {Clr();}

private:

char *errText;
int   errNum;
int   errArg;
};
#endif
