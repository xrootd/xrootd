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

struct XrdOucGatherConfData;
class  XrdSysError;

class XrdOucGatherConf
{
public:

//------------------------------------------------------------------------------
//! Echo the last line retrieved using GetLine() using proper framing.
//!
//! @notes  1) An exception is thrown if a XrdSysError object was not supplied.
//------------------------------------------------------------------------------

void EchoLine();

//------------------------------------------------------------------------------
//! Specift the order in which the last line is displayed vs a message.
//!
//! @paramn doBefore - When true, the line is displayed before the message.
//!                    When false, it is displayed after the message (default).
//!
//! @notes  1) This call is only relevant to calls to MsgE(), MsgW(), MsgfE(),
//!            and MsgfW.
//------------------------------------------------------------------------------

void EchoOrder(bool doBefore);

//------------------------------------------------------------------------------
//! Gather information from a config file.
//!
//! @note You must call this method or a successful useData() before calling
//!       any Get/Ret methods.
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
//! Sequence to the next line in the configuration file.
//!
//! @return Pointer to the next line that will be tokenized or NIL if there
//!         are no more lines left.
//!
//! @notes  1) You must call GetLine() before calling GetToken().
//------------------------------------------------------------------------------

char* GetLine();

//------------------------------------------------------------------------------
//! Get the next blank-delimited token in the record returned by Getline().
//!
//! @param rest     - Address of a char pointer. When specified, a pointer to
//!                   the first non-blank character after the returned token.
//! @param lowcasee - When 1, all characters are converted to lower case.
//!                   When 0, the default, the characters are not changed. 
//!
//! @return A pointer to the next token. If the end of the line has been
//!         reached, a NIL pointer is returned.
//------------------------------------------------------------------------------

char* GetToken(char **rest=0, int lowcase=0);

//------------------------------------------------------------------------------
//! Get the last line.
//!
//! @return pointer to the last line. If no last line exists a null string is
//!         returned. The pointer is valid until GetLine() is called.
//------------------------------------------------------------------------------

const char* LastLine();

//------------------------------------------------------------------------------
//! Check if data is present.
//!
//! @return True if data is present and false, otherwise.
//------------------------------------------------------------------------------

bool  hasData();

//-----------------------------------------------------------------------------
//! Display a space delimited error/warning message.
//!
//! @param  txt1,txt2,txt3,txt4,txt5,txt6  the message components formatted as
//!         "txt1 [txt2] [txt3] [txt4] [txt5] [txt6]"
//!
//! @notes  1) This methods throws an exception if a XrdSysError object was not
//!            passed to the constructor.
//!         2) The last line returned by this object will be displayed either
//!            before or after the message (see EchoOrder()).
//!         3) Messages are truncated at 2048 bytes.
//!         4} Use MsgE for errors.   The text is prefixed by "Config mistake:"
//!            Use MsgW for warnings. The text is prefixed by "Config warning:"
//-----------------------------------------------------------------------------

void  MsgE(const char* txt1,   const char* txt2=0, const char* txt3=0,
           const char* txt4=0, const char* txt5=0, const char* txt6=0);

void  MsgW(const char* txt1,   const char* txt2=0, const char* txt3=0,
           const char* txt4=0, const char* txt5=0, const char* txt6=0);

//-----------------------------------------------------------------------------
//! Display a formated error/warning message using variable args (i.e. vprintf).
//!
//! @param  fmt  the message formatting template (i.e. printf format).
//! @param  ...  the arguments that should be used with the template. The
//!              formatted message is truncated at 2048 bytes.
//!
//! @notes  1) This methods throws an exception if a XrdSysError object was not
//!            passed to the constructor.
//!         2) The last line returned by this object will be displayed either
//!            before or after the message (see EchoOrder()).
//!         3) Messages are truncated at 2048 bytes.
//!         4} Use MsgfE for errors.   The text is prefixed by "Config mistake:"
//!            Use MsgfW for warnings. The text is prefixed by "Config warning:"
//-----------------------------------------------------------------------------

void  MsgfE(const char *fmt, ...);

void  MsgfW(const char *fmt, ...);

//------------------------------------------------------------------------------
//! Backups the token scanner just prior to the last returned token.
//!
//! @notes 1) Only one backup is allowed. Calling RetToken() more than once
//!           without an intervening  GetToken() call results in undefined
//!           behaviour.
//!        2) This call is useful for backing up due to an overscan.
//------------------------------------------------------------------------------

void  RetToken();

//------------------------------------------------------------------------------
//! Specify how tag characters should be handled.
//!
//! @param x       - When 0, tabs are converted to spaces.
//!                  When 1, tabs are untouched (the default).
//------------------------------------------------------------------------------

void  Tabs(int x=1);

//------------------------------------------------------------------------------
//! Attempt to use pre-existing data.
//!
//! @param  data  - Pointer to null terminated pre-existing data.
//!
//! @return False if the pointer is nil or points to a null string; true o/w.
//------------------------------------------------------------------------------

bool  useData(const char *data);

//------------------------------------------------------------------------------
//! Constructor #1
//!
//! @note This object collects relevant configuration directives ready to be
//!       processed by the Get/Ret methods. All if-fi, set, and variable
//!       substitutions are performed.
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

void MsgX(const char** mVec, int n);
void MsgfX(const char* txt1, const char* txt2);

XrdOucGatherConfData *gcP;
};
#endif
