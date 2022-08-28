#ifndef __XRDOUCGATHERCONF_HH__
#define __XRDOUCGATHERCONF_HH__
/******************************************************************************/
/*                                                                            */
/*                   X r d O u c G a t h e r C o n f . h h                    */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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


#include "XrdOuc/XrdOucTokenizer.hh"

class XrdOucTList;
class XrdSysError;

class XrdOucGatherConf : public XrdOucTokenizer
{
public:

//------------------------------------------------------------------------------
//! Gather information from a config file.
//!
//! @note You must call this method or a successful useData() before calling
//!       any XrdOucTokenizer methods.
//!
//! @param  cfname Path to the configuration file.
//! @param  lvl    Indicates how the gathered directives are to be saved:
//!                full_lines - the full directive line, including newline.
//!                trim_lines - Like full_lines but the prefix (i.e.characters
//!                             the dot) are discarded. Useful only when
//!                             gathering a single prefix.
//!                only_body    Saves the body of each wanted directive as a
//!                             space separated string blob.
//!                trim_body    Like only_body but also includes the directive
//!                             characters after the dot. Useful only when
//!                             gathering a single prefix.
//! @param  parms  Optional pointer to initial configuration parameters.
//!                These may be present for plugins.
//!
//! @return > 0 Success, configuration data has been gathered.
//! @return = 0 Nothing was gathered.
//! @return < 0 Problem reading the config file, returned value is -errno.
//------------------------------------------------------------------------------

enum  Level {full_lines = 0,  //!< Complete lines
             trim_lines,      //!< Prefix trimmed lines
             only_body,       //!< Only directive bodies as a string blob.
             trim_body        //!< Prefix trimmed lines as a string blob.
            };

int   Gather(const char *cfname, Level lvl, const char *parms=0);

//------------------------------------------------------------------------------
//! Check if data is present.
//!
//! @return True if data is present and false, otherwise.
//------------------------------------------------------------------------------

bool  hasData();

//------------------------------------------------------------------------------
//! Attempt to use pre-existing data.
//!
//! @param  data  Pointer to null terminated pre-existing data.
//!
//! @return False if the pointer is nil or points to a null string; true o/w.
//------------------------------------------------------------------------------

bool  useData(const char *data);

//------------------------------------------------------------------------------
//! Constructor #1
//!
//! @note This object collects relevant configuration directives ready to be
//!       processed by the inherited XrdOucTokenizer methods. All if-fi, set,
//!       and variable substitutions are performed.
//!
//! @param  want   A space separated list of directive prefixes (i.e. end with a
//!                dot) and actual directives that should be gathered.
//! @param  errP   Optional pointer to an error object. When supplied, gathered
//!                lines are echoed. Additionally, error messages are issued.        supplied XrdSysError object or using std::cerr using a
//------------------------------------------------------------------------------

      XrdOucGatherConf(const char *want, XrdSysError *errP=0);

//------------------------------------------------------------------------------
//! Constructor #2
//!
//! @note This is the same as constructor #1 but uses vector to hold the
//!       wanted directives or directive prefixes.
//!
//! @param  want   A vector of strings of directive prefixes (i.e. end with a
//!                dot) and actual directives that should be gathered. The
//!                end of the vector is indicated by a nil pointer
//!                (e,g, const char *want[] = {"x.c", "y.", 0};
//! @param  errP   Optional pointer to an error object. When supplied, gathered
//!                lines are echoed. Additionally, error messages are issued.        supplied XrdSysError object or using std::cerr using a
//------------------------------------------------------------------------------

      XrdOucGatherConf(const char **&want, XrdSysError *errP=0);

     ~XrdOucGatherConf();

private:

XrdSysError *eDest;
XrdOucTList *Match;
char        *gBuff;
};
#endif
