#ifndef __OOUC_ERROR_H__
#define __OOUC_ERROR_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d O u c E r r o r . h h                         */
/*                                                                            */
/*(c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*Produced by Andrew Hanushevsky for Stanford University under contract       */
/*           DE-AC03-76-SFO0515 with the Deprtment of Energy                  */
/******************************************************************************/

//          $Id$
 
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

/******************************************************************************/
/*                      o o u c _ E r r o r _ T a b l e                       */
/******************************************************************************/

class XrdOucError_Table
{
public:
friend class XrdOucError;

char  *Lookup(int mnum)
             {return (char *)(mnum < base_msgnum || mnum > last_msgnum
                             ? 0 : msg_text[mnum - base_msgnum]);
             }
       XrdOucError_Table(int base, int last, const char **text)
                  {base_msgnum = base; last_msgnum = last; msg_text = text;
                   next = 0;
                  }
      ~XrdOucError_Table() {}

private:
XrdOucError_Table *next;            // -> Next table or 0;
int               base_msgnum;     // Starting message number
int               last_msgnum;     // Ending   message number
const char      **msg_text;        // Array of message text
};
  
/******************************************************************************/
/*                  L o g   M a s k   D e f i n i t i o n s                   */
/******************************************************************************/
  
const int OUC_LOG_01 =   1;
const int OUC_LOG_02 =   2;
const int OUC_LOG_03 =   4;
const int OUC_LOG_04 =   8;
const int OUC_LOG_05 =  16;
const int OUC_LOG_06 =  32;
const int OUC_LOG_07 =  64;
const int OUC_LOG_08 = 128;

/******************************************************************************/
/*                            o o u c _ E r r o r                             */
/******************************************************************************/

class XrdOucLogger;
  
class XrdOucError
{
public:
         XrdOucError(XrdOucLogger *lp, const char *ErrPrefix="oouc")
                   {SetPrefix(ErrPrefix); Logger=lp;
                   }

        ~XrdOucError() {}

// addTable allows you to add a new error table for errno handling. Any
// number of table may be added and must consist of statis message text
// since the table are deleted but the text is not freed. Error tables
// must be setup without multi-threading.  There is only one global table.
//
static void addTable(XrdOucError_Table *etp) {etp->next = etab; etab = etp;}

// baseFD() returns the original FD associated with this object.
//
int baseFD();

// ec2text tyranslates an error code to the correspodning error text or returns
// null if matching text cannot be found.
//
static char *ec2text(int ecode);

// Emsg() produces a message of various forms. The message is written to the 
// constructor specified file descriptor. See variations below.
//
// <datetime> <epfx><esfx>: error <ecode> (syser[<ecode>]); <text1> <text2>"
// (returns abs(ecode)).
//
int Emsg(const char *esfx, int ecode, const char *text1, char *text2=0);

// <datetime> <epfx><esfx>: <text1> <text2> <text3>
//
void Emsg(const char *esfx, const char *text1, char *text2=0, char *text3=0);

// <datetime> <epfx><esfx>: <text1> <text2> <text3>
//
void Log(const int mask, const char *esfx, const char *text1,
                                char *text2=0, char *text3=0);

// logger() sets/returns the logger object for this message message handler.
//
XrdOucLogger *logger(XrdOucLogger *lp=0)
                   {XrdOucLogger *oldp = Logger;
                    if (lp) Logger = lp;
                    return oldp;
                   }

// Say() route a line without timestamp or prefix
//
void Say(const char *text1, char *text2=0, char *txt3=0);

// SetPrefix() dynamically changes the error prefix
//
inline const char *SetPrefix(const char *prefix)
                        {char *oldpfx = (char *)epfx;
                         epfx = prefix; epfxlen = strlen(epfx);
                         return (const char *)oldpfx;
                        }

// TBeg() is used to start a trace on ostream cerr. The TEnd() ends the trace.
//
void TBeg(const char *txt1=0, const char *txt2=0, const char *txt3=0);
void TEnd();

private:

static XrdOucError_Table *etab;
const char               *epfx;
int                       epfxlen;
XrdOucLogger             *Logger;
};
#endif
