#ifndef __XRDMPXXML_HH__
#define __XRDMPXXML_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d M p x X m l . h h                           */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdOucTokenizer;
  
class XrdMpxXml
{
public:

enum fmtType {fmtCGI, fmtFlat, fmtText, fmtXML};

int Format(const char *Host, char *ibuff, char *obuff);

    XrdMpxXml(fmtType ft, bool nz=false, bool dbg=false)
                          : fType(ft), Debug(dbg), noZed(nz)
                          {if (ft == fmtCGI) {vSep = '='; vSfx = '&';}
                              else           {vSep = ' '; vSfx = '\n';}
                           doV2T = ft == fmtText;
                          }
   ~XrdMpxXml() {}

private:

struct VarInfo
      {const char *Name;
             char *Data;
      };

char *Add(char *Buff, const char *Var, const char *Val);
void  getVars(XrdOucTokenizer &Data, VarInfo Var[]);
int   xmlErr(const char *t1, const char *t2=0, const char *t3=0);

fmtType fType;
char    vSep;
char    vSfx;
bool    Debug;
bool    noZed;
bool    doV2T;
};
#endif
