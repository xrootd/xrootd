/******************************************************************************/
/*                                                                            */
/*                   X r d O u c G a t h e r C o n f . c c                    */
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

#include <stdexcept>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucGatherConf.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                         L o c a l   O b j e c t s                          */
/******************************************************************************/
  
struct XrdOucGatherConfData
{
XrdOucTokenizer  Tokenizer;
XrdSysError     *eDest   = 0;
XrdOucString     lline;
XrdOucTList     *Match   = 0;
char            *gBuff   = 0;
bool             echobfr = false;

                 XrdOucGatherConfData(XrdSysError *eP)
                                     : Tokenizer(0), eDest(eP) {}
                ~XrdOucGatherConfData() {}
};

/******************************************************************************/
/*                        C o n s t r u c t o r   # 1                         */
/******************************************************************************/

XrdOucGatherConf::XrdOucGatherConf(const char *want, XrdSysError *errP)
                 : gcP(new XrdOucGatherConfData(errP))
{
   XrdOucString wlist(want), wtoken;
   int wlen, wPos = 0;
  
   while((wPos = wlist.tokenize(wtoken, wPos, ' ')) != -1)
        {wlen = (wtoken.endswith('.') ? wtoken.length() : 0);
         gcP->Match = new XrdOucTList(wtoken.c_str(), wlen, gcP->Match);
        }
}

/******************************************************************************/
/*                        C o n s t r u c t o r   # 2                         */
/******************************************************************************/

XrdOucGatherConf::XrdOucGatherConf(const char **&want, XrdSysError *errP)
                 : gcP(new XrdOucGatherConfData(errP))
{
   int n, i = 0;

   while(want[i])
       {if ((n = strlen(want[i])))
           {if (*(want[i]+(n-1)) != '.') n = 0;
            gcP->Match = new XrdOucTList(want[i], n, gcP->Match);
           }
       }
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOucGatherConf::~XrdOucGatherConf()
{
   XrdOucTList *tP;

   while((tP = gcP->Match))
        {gcP->Match = tP->next;
         delete tP;
        }

   if (gcP->gBuff)     free(gcP->gBuff);
}

/******************************************************************************/
/*                              E c h o L i n e                               */
/******************************************************************************/

void XrdOucGatherConf::EchoLine()
{

// Make sure we can actually display anything
//
   if (!(gcP->eDest))
      throw std::invalid_argument("XrdSysError object not supplied!");

// Echo only when we have something to echo
//
   if (gcP->lline.length()) gcP->eDest->Say("=====> ", gcP->lline.c_str());
}
  
/******************************************************************************/
/*                             E c h o O r d e r                              */
/******************************************************************************/

void XrdOucGatherConf::EchoOrder(bool doBefore)
{
   gcP->echobfr = doBefore;
}
  
/******************************************************************************/
/*                                G a t h e r                                 */
/******************************************************************************/
  
int XrdOucGatherConf::Gather(const char *cfname, Level lvl, const char *parms)
{
   XrdOucEnv myEnv;
   XrdOucStream Config(gcP->eDest, getenv("XRDINSTANCE"), &myEnv, "=====> ");
   XrdOucTList *tP;
   XrdOucString theGrab;
   char *var, drctv[64], body[4096];
   int cfgFD, n, rc;
   bool trim = false, addKey = true;

// Make sure we have something to compare
//
   if (!(gcP->Match)) return 0;

// Reset the buffer if it has been set
//
   if (gcP->gBuff)
      {free(gcP->gBuff);
       gcP->gBuff = 0;
       gcP->Tokenizer.Attach(0);
      }

// Open the config file
//
   if ( (cfgFD = open(cfname, O_RDONLY, 0)) < 0)
      {rc = errno;
       if (gcP->eDest) gcP->eDest->Emsg("Gcf", rc, "open config file", cfname);
       return -rc;
      }

// Attach the file to our stream object and size the grab buffer
//
   Config.Attach(cfgFD, 4096);
   theGrab.resize(4096);
   if (parms && *parms) theGrab = parms;

// Setup for processing
//
   switch(lvl)
         {case full_lines: *drctv = '\n'; trim = false; addKey = true;  break;
          case trim_lines: *drctv = '\n'; trim = true;  addKey = true;  break;
          case only_body:  *drctv = ' ';  trim = false; addKey = false; break;
          case trim_body:  *drctv = ' ';  trim = true;  addKey = true;  break;
          default: break;  return 0;    // Should never happen
                           break;
         }

// Process the config file
//
   while((var = Config.GetMyFirstWord()))
        {tP = gcP->Match;
         while(tP && ((tP->val && strncmp(var, tP->text, tP->val)) ||
                     (!tP->val && strcmp( var, tP->text)))) tP = tP->next;

         if (tP)
            {if (addKey)
                {if (trim)
                    {char *dot = index(var, '.');
                     if (dot && *(dot+1)) var = dot+1;
                    }
                 int n =  snprintf(drctv+1, sizeof(drctv)-1, "%s ", var);
                 if (n >= (int)sizeof(drctv)-1)
                    {if (gcP->eDest) gcP->eDest->Emsg("Gcf", E2BIG, "handle", var);
                     return -E2BIG;
                    }
                } else drctv[1] = 0;

              if (!Config.GetRest(body, sizeof(body)))
                 {if (gcP->eDest) gcP->eDest->Emsg("Gcf", E2BIG, "handle arguments");
                  return -E2BIG;
                 }

              if (*body || addKey)
                 {theGrab += drctv;
                  theGrab += body;
                 }
            }
        }

// Now check if any errors occurred during file i/o
//
   if ((rc = Config.LastError()))
      {if (gcP->eDest) gcP->eDest->Emsg("Gcf", rc, "read config file", cfname);
       return (rc < 0 ? rc : -rc);
      }


// Copy the grab to a modifiable buffer.
//
   if ((n = theGrab.length()) <= 1) n = 0;
      else {gcP->gBuff = (char *)malloc(n);
            strcpy(gcP->gBuff, theGrab.c_str()+1); // skip 1st byte but add null
            gcP->Tokenizer.Attach(gcP->gBuff);
            n--;
           }
   return n;
}

/******************************************************************************/
/*                               G e t L i n e                                */
/******************************************************************************/

char* XrdOucGatherConf::GetLine()
{
   char* theLine = gcP->Tokenizer.GetLine();

   while(theLine && *theLine == 0) theLine = gcP->Tokenizer.GetLine();

   if (!theLine) gcP->lline = "";
      else       gcP->lline = theLine;

   return theLine;
}

/******************************************************************************/
/*                              G e t T o k e n                               */
/******************************************************************************/

char* XrdOucGatherConf::GetToken(char **rest, int lowcase)
{
   return gcP->Tokenizer.GetToken(rest, lowcase);
}
  
/******************************************************************************/
/*                               h a s D a t a                                */
/******************************************************************************/
  
bool XrdOucGatherConf::hasData()
{
   return gcP->gBuff != 0 && *(gcP->gBuff) != 0;
}

/******************************************************************************/
/*                              L a s t L i n e                               */
/******************************************************************************/

const char* XrdOucGatherConf::LastLine()
{
   if (gcP->lline.capacity() == 0) return "";
   return gcP->lline.c_str();  
}
  
/******************************************************************************/
/*                                  M s g E                                   */
/******************************************************************************/

void XrdOucGatherConf::MsgE(const char* txt1,const char* txt2,const char* txt3,
                            const char* txt4,const char* txt5,const char* txt6)
{
  const char* mVec[7];
  int n = 0;

            mVec[n++] = "Config mistake:";
  if (txt1) mVec[n++] = txt1;
  if (txt2) mVec[n++] = txt2;
  if (txt3) mVec[n++] = txt3;
  if (txt4) mVec[n++] = txt4;
  if (txt5) mVec[n++] = txt5;
  if (txt6) mVec[n++] = txt6;

  MsgX(mVec, n+1);
}
  
/******************************************************************************/
/*                                  M s g W                                   */
/******************************************************************************/

void XrdOucGatherConf::MsgW(const char* txt1,const char* txt2,const char* txt3,
                            const char* txt4,const char* txt5,const char* txt6)
{
  const char* mVec[7];
  int n = 0;

            mVec[n++] = "Config warning:";
  if (txt1) mVec[n++] = txt1;
  if (txt2) mVec[n++] = txt2;
  if (txt3) mVec[n++] = txt3;
  if (txt4) mVec[n++] = txt4;
  if (txt5) mVec[n++] = txt5;
  if (txt6) mVec[n++] = txt6;

  MsgX(mVec, n+1);
}

/******************************************************************************/
/*                                  M s g X                                   */
/******************************************************************************/

void XrdOucGatherConf::MsgX(const char** mVec, int n)
{
   XrdOucString theMsg(2048);

// Make sure we can actually display anything
//
   if (!(gcP->eDest))
      throw std::invalid_argument("XrdSysError object not supplied!");

// Construct the message in a string
//
   for (int i = 0; i < n; i++)
       {theMsg += mVec[i];
        if (i+1 < n) theMsg += ' ';
       }

// Dislay the last line and the message in the proper order
//
   if (gcP->echobfr)  EchoLine();
   gcP->eDest->Say(theMsg.c_str());
   if (!(gcP->echobfr)) EchoLine();
}
  
/******************************************************************************/
/*                                 M s g f E                                  */
/******************************************************************************/

void XrdOucGatherConf::MsgfE(const char *fmt, ...)
{
   char buffer[2048];
   va_list  args;
   va_start (args, fmt);

// Format the message
//
   vsnprintf(buffer, sizeof(buffer), fmt, args);

// Go print the message
//
   MsgfX("Config mistake: ", buffer);
}
  
/******************************************************************************/
/*                                 M s g f W                                  */
/******************************************************************************/

void XrdOucGatherConf::MsgfW(const char *fmt, ...)
{
   char buffer[2048];
   va_list  args;
   va_start (args, fmt);

// Format the message
//
   vsnprintf(buffer, sizeof(buffer), fmt, args);

// Go print the message
//
   MsgfX("Config warning: ", buffer);
}
  
/******************************************************************************/
/*                                 M s g f X                                  */
/******************************************************************************/

void XrdOucGatherConf::MsgfX(const char* txt1, const char* txt2)
{  

// Make sure we can actually display anything
//
   if (!(gcP->eDest))
      throw std::invalid_argument("XrdSysError object not supplied!");

// Dislay the last line and the message in the proper order
//
   if (gcP->echobfr)  EchoLine();
   gcP->eDest->Say(txt1, txt2);
   if (!(gcP->echobfr)) EchoLine();
}

/******************************************************************************/
/*                              R e t T o k e n                               */
/******************************************************************************/

void XrdOucGatherConf::RetToken()
{
   return gcP->Tokenizer.RetToken();
}

/******************************************************************************/
/*                                  T a b s                                   */
/******************************************************************************/

void XrdOucGatherConf::Tabs(int x)
{
   gcP->Tokenizer.Tabs(x);
}
  
/******************************************************************************/
/*                               u s e D a t a                                */
/******************************************************************************/
  
bool XrdOucGatherConf::useData(const char *data)
{
   if (!data || *data == 0) return false;

   if (gcP->gBuff) free(gcP->gBuff);
   gcP->gBuff = strdup(data);
   gcP->Tokenizer.Attach(gcP->gBuff);
   return true;
}
