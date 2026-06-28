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
                  XrdMonRoll::Item itemVec[], int itemCnt);

bool     Registered() {return !regVec.empty();}

         XrdMonitor() {}
        ~XrdMonitor() {}

private:

using MRIFam = XrdMonRoll::Item::Family;
using MRISch = XrdMonRoll::Item::Schema;
using MRITrt = XrdMonRoll::Item::Trait;

enum   sType {isAdon=0x00000001, isPlug=0x00000002};

struct RegInfo
      {char*             typName;
       char*             setName;
       char*             Json_hdr = 0;
       char*             Xml_hdr  = 0;
       char*             eTmplt;
       sType             setType;
       int               iCount   = 0;
       XrdMonRoll::Item* iVec     = 0;

       RegInfo(const char* sName, const char* tName, sType sTVal);

      ~RegInfo();
      };

std::vector<RegInfo*> regVec;

RegInfo* FindSet(const char* setName, int sType);
int      FormJSON(RegInfo& regInfo, char* buff, int bsize);
int      FormXML( RegInfo& regInfo, char* buff, int bsize);
bool     RegFail(const char* TName, const char* SName, const char* why);
int      V2S(RegInfo& rI, XrdMonRoll::Item& iR, char* buff, int blen);
int      V2T(RegInfo& rI, XrdMonRoll::Item& iR, char* buff, int blen, bool ij);
void     ValEnd(bool& isBad, XrdMonitor::RegInfo& rI, const char* aoT,
                const char* begKey, XrdMonRoll::Item* endP);
bool     ValErr(RegInfo& regInfo, int iNum, const char* etxt);
void     ValKey(bool& isBad, RegInfo& rI, XrdMonRoll::Item* itemP);
bool     Validate(RegInfo& regInfo);
};
#endif
