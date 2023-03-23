#ifndef __SYS_ERROR_H__
#define __SYS_ERROR_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d S y s E r r o r . h h                         */
/*                                                                            */
/*(c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*Produced by Andrew Hanushevsky for Stanford University under contract       */
/*           DE-AC02-76-SFO0515 with the Deprtment of Energy                  */
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
 
#include <cstdlib>
#ifndef WIN32
#include <unistd.h>
#include <cstring>
#include <strings.h>
#else
#include <cstring>
#endif

/******************************************************************************/
/*                      o o u c _ E r r o r _ T a b l e                       */
/******************************************************************************/

class XrdSysError_Table
{
public:
friend class XrdSysError;

char  *Lookup(int mnum)
             {return (char *)(mnum < base_msgnum || mnum > last_msgnum
                             ? 0 : msg_text[mnum - base_msgnum]);
             }
       XrdSysError_Table(int base, int last, const char **text)
                        : next(0),
                          base_msgnum(base),
                          last_msgnum(last),
                          msg_text(text) {}
      ~XrdSysError_Table() {}

private:
XrdSysError_Table *next;            // -> Next table or 0;
int               base_msgnum;     // Starting message number
int               last_msgnum;     // Ending   message number
const char      **msg_text;        // Array of message text
};
  
/******************************************************************************/
/*                  L o g   M a s k   D e f i n i t i o n s                   */
/******************************************************************************/
  
const int SYS_LOG_01 =   1;
const int SYS_LOG_02 =   2;
const int SYS_LOG_03 =   4;
const int SYS_LOG_04 =   8;
const int SYS_LOG_05 =  16;
const int SYS_LOG_06 =  32;
const int SYS_LOG_07 =  64;
const int SYS_LOG_08 = 128;
//              0x00000100 to 0x0000ffff reseved for     XRootD use
//              0x00010000 to 0xffff0000 reseved for non-XRootD use

/******************************************************************************/
/*                            o o u c _ E r r o r                             */
/******************************************************************************/

class XrdSysLogger;
  
class XrdSysError
{
public:
         XrdSysError(XrdSysLogger *lp, const char *ErrPrefix="sys")
                    : epfx(0),
                      epfxlen(0),
                      msgMask(-1),
                      Logger(lp)
                    { SetPrefix(ErrPrefix); }

        ~XrdSysError() {}

// addTable allows you to add a new error table for errno handling. Any
// number of table may be added and must consist of statis message text
// since the table are deleted but the text is not freed. Error tables
// must be setup without multi-threading.  There is only one global table.
//
static void addTable(XrdSysError_Table *etp) {etp->next = etab; etab = etp;}

// baseFD() returns the original FD associated with this object.
//
int baseFD();

// ec2text tyranslates an error code to the correspodning error text or returns
// null if matching text cannot be found.
//
static const char *ec2text(int ecode);

// Emsg() produces a message of various forms. The message is written to the 
// constructor specified file descriptor. See variations below.
//
// <datetime> <epfx><esfx>: error <ecode> (syser[<ecode>]); <text1> <text2>"
// (returns abs(ecode)).
//
int Emsg(const char *esfx, int ecode, const char *text1, const char *text2=0);

// <datetime> <epfx><esfx>: <text1> <text2> <text3>
//
void Emsg(const char *esfx, const char *text1, 
                            const char *text2=0,
                            const char *text3=0);

// <datetime> <epfx><esfx>: <text1> <text2> <text3>
//
inline void Log(int mask, const char *esfx, 
                          const char *text1,
                          const char *text2=0,
                          const char *text3=0)
               {if (mask & msgMask) Emsg(esfx, text1, text2, text3);}

// logger() sets/returns the logger object for this message message handler.
//
XrdSysLogger *logger(XrdSysLogger *lp=0)
                   {XrdSysLogger *oldp = Logger;
                    if (lp) Logger = lp;
                    return oldp;
                   }

// Say() route a line without timestamp or prefix
//
void Say(const char *text1,   const char *text2=0, const char *txt3=0,
         const char *text4=0, const char *text5=0, const char *txt6=0);

// Set/Get the loging mask (only used by clients of this object)
//
void setMsgMask(int mask) {msgMask = mask;}

int  getMsgMask()         {return msgMask;}

// SetPrefix() dynamically changes the error prefix
//
inline const char *SetPrefix(const char *prefix)
                        {const char *oldpfx = epfx;
                         epfx = prefix; epfxlen = strlen(epfx);
                         return oldpfx;
                        }

// TBeg() is used to start a trace on std::ostream std::cerr. The TEnd() ends the trace.
//
void TBeg(const char *txt1=0, const char *txt2=0, const char *txt3=0);
void TEnd();

private:

static XrdSysError_Table *etab;
const char               *epfx;
int                       epfxlen;
int                       msgMask;
XrdSysLogger             *Logger;
};
#endif
