#ifndef __FRCREQAGENT_H__
#define __FRCREQAGENT_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d F r c R e q A g e n t . h h                      */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdFrc/XrdFrcReqFile.hh"
#include "XrdFrc/XrdFrcRequest.hh"

class XrdFrcReqAgent
{
public:

void Add(XrdFrcRequest &Request);

void Del(XrdFrcRequest &Request);

int  List(XrdFrcRequest::Item *Items, int Num);
int  List(XrdFrcRequest::Item *Items, int Num, int Prty);

int  NextLFN(char *Buff, int Bsz, int Prty, int &Offs);

void Ping(const char *Msg=0);

int  Start(char *aPath, int aMode);

     XrdFrcReqAgent(const char *Me, int qVal);
    ~XrdFrcReqAgent() {}

private:

static char     *c2sFN;

XrdFrcReqFile   *rQueue[XrdFrcRequest::maxPQE];
const char      *Persona;
const char      *pingMsg;
const char      *myName;
int              theQ;
};
#endif
