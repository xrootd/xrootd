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

#include <string>
#include <cstring>
  
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

       void  Clr() {errText.clear(); errArg = errNum = 0;}

//-----------------------------------------------------------------------------
//! Get current error information.
//!
//! @param  eNum  place where the error number is to be placed.
//!
//! @return The error text and the error number value.
//-----------------------------------------------------------------------------

const
std::string &Get(int &eNum) const {eNum = errNum; return errText;}

//-----------------------------------------------------------------------------
//! Get current error text.
//!
//! @return The error text.
//-----------------------------------------------------------------------------

const
std::string &Get() const {return errText;}

//-----------------------------------------------------------------------------
//! Get current error argument.
//!
//! @return       the error argument value.
//-----------------------------------------------------------------------------

       int   GetArg() const {return errArg;}

//-----------------------------------------------------------------------------
//! Check if there is an error.
//!
//! @return       True if an error exists and false otherwise.
//-----------------------------------------------------------------------------

       bool  hasError() const {return errNum != 0;}

//-----------------------------------------------------------------------------
//! Check if there is no error.
//!
//! @return       True if no error exists and false otherwise.
//-----------------------------------------------------------------------------

       bool  isOK() const {return errNum == 0;}

//-----------------------------------------------------------------------------
//! Set new error information. There are two obvious variations.
//!
//! @param  eMsg  pointer to a string describing the error. If nil, the eNum
//!               is taken as errno and converted to corresponding description.
//! @param  eNum  the error number associated with the error.
//! @param  eArg  the error argument, if any (see XrdSsiService::Provision()).
//-----------------------------------------------------------------------------

       void  Set(const char *eMsg=0, int eNum=0, int eArg=0)
                {errText = (eMsg && *eMsg ? eMsg : Errno2Text(eNum));
                 errNum  = eNum;
                 errArg  = eArg;
                }

       void  Set(const std::string &eMsg, int eNum=0, int eArg=0)
                {errText = (eMsg.empty() ? Errno2Text(eNum) : eMsg);
                 errNum  = eNum;
                 errArg  = eArg;
                }

//------------------------------------------------------------------------------
//! Assignment operator
//------------------------------------------------------------------------------

XrdSsiErrInfo &operator=(XrdSsiErrInfo const &rhs)
               {if (&rhs != this) Set(rhs.errText, rhs.errNum, rhs.errArg);
                return *this;
               }

//------------------------------------------------------------------------------
//! Copy constructor
//------------------------------------------------------------------------------

               XrdSsiErrInfo(XrdSsiErrInfo const &oP)
                            {Set(oP.errText, oP.errNum, oP.errArg);}

//-----------------------------------------------------------------------------
//! Constructor and Destructor
//-----------------------------------------------------------------------------

      XrdSsiErrInfo() : errNum(0), errArg(0) {}

     ~XrdSsiErrInfo() {}

private:
const char* Errno2Text(int ecode);

std::string errText;
int         errNum;
int         errArg;
};
#endif
