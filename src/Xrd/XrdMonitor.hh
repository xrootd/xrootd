#ifndef __XRDMONITOR__
#define __XRDMONITOR__
/******************************************************************************/
/*                                                                            */
/*                         X r d M o n i t o r . h h                          */
/*                                                                            */
/* (c) 2024 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <map>
#include <cstring>
#include <vector>

#include "Xrd/XrdMonRoll.hh"

class XrdMonitor
{
public:

// Format Options:
//
enum Mopts {F_JSON = 0x10000000, X_PLUG = 0x00000001, X_ADON = 0x00000002};

int      Format(char* buff, int bsize, int& item, int opts=0);

int      Format(char* buff, int bsize, const char* setName, int opts=0);

bool     Register(XrdMonRoll::rollType  setType, const char* setName,
                  XrdMonRoll::setMember setVec[]);

bool     Registered() {return !regVec.empty();}

         XrdMonitor() {}
        ~XrdMonitor() {}

private:
enum sType {isAdon=0x00000001, isPlug=0x00000002};

struct RegInfo
      {char* setName;
       int   setType;
       struct {char* hdr;
               std::vector<char*> key;
              } Json;
       struct {char* hdr;
               std::vector<char*> keyBeg;
               std::vector<char*> keyEnd;
              } Xml;
       RAtomic_uint**     keyVal;

       RegInfo(const char* sName, int sType)
              : setName(strdup(sName)), setType(sType) {}
      ~RegInfo() {}  // Never gets deleted
      };

RegInfo* FindSet(const char* setName, int sType);
int      FormJSON(RegInfo& regInfo, char* buff, int bsize);
int      FormXML( RegInfo& regInfo, char* buff, int bsize);

std::vector<RegInfo*> regVec;
};
#endif
