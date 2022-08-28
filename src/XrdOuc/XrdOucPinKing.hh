#ifndef __XRDOUCPINKING_HH__
#define __XRDOUCPINKING_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d O u c P i n K i n g . h h                       */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <vector>

#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucPinObject.hh"
#include "XrdSys/XrdSysError.hh"

//------------------------------------------------------------------------------
//! This include file defines a class that loads an object oriented versioned
//! plugin. This is new for R5 and establishes a standard  frameworkwork for
//! all new plugins going forward (should have been done long time ago).
//------------------------------------------------------------------------------

class  XrdOucEnv;
struct XrdVersionInfo;

template<class T>
class XrdOucPinKing
{
public:

//------------------------------------------------------------------------------
//! Add an Pin object to the load list.
//!
//! @param path   Pointer to the pin's path.
//! @param parms  Pointer to the pin's parameters.
//! @param push   When true pushes the pin onto the load stack. Otherwise,
//!               replaces or defines the base plugin.
//------------------------------------------------------------------------------

void   Add(const char *path,
           const char *parms,
           bool        push=false)
          {if (push) pinVec.push_back(pinInfo(path, parms));
              else pinVec[0] = pinInfo(path, parms);
          }

//------------------------------------------------------------------------------
//! Load all necessary plugins.
//!
//! @param Symbol Pointer to the external symobol of the plugin.
//!
//! @return Pointer to the plugin upon success and nil upon failure.
//------------------------------------------------------------------------------

T     *Load(const char *Symbol);

//------------------------------------------------------------------------------
//! Constructor
//!
//! @param drctv  Ref to the directive that initiated the load. The text is
//!               used in error messages to relate the directive to the error.
//!               E.g. "sec.entlib" -> "Unable to load sec.entlib plugin...."
//! @param envR   Ref to environment.
//! @param errR   Ref to the message routing object.
//! @param vinfo  Pointer to the version information of the caller. If the
//!               pointer is nil, no version checking occurs.
//------------------------------------------------------------------------------

       XrdOucPinKing(const char     *drctv,
                     XrdOucEnv      &envR,
                     XrdSysError    &errR,
                     XrdVersionInfo *vinfo=0)
                    : Drctv(drctv), eInfo(envR), eMsg(errR), vInfo(vinfo)
                     {pinVec.push_back(pinInfo(0,0));}

//------------------------------------------------------------------------------
//! Destructor
//!
//! Upon deletion, if the plugin was successfully loaded, it is persisted.
//------------------------------------------------------------------------------

      ~XrdOucPinKing() {}

private:

const char     *Drctv;
XrdOucEnv      &eInfo;
XrdSysError    &eMsg;
XrdVersionInfo *vInfo;

struct pinInfo
{
std::string      path;
std::string      parm;
XrdOucPinLoader *pinP;

               pinInfo(const char *pth, const char *prm)
                      : path(pth ? pth : ""), parm(prm ? prm : ""), pinP(0) {}

              ~pinInfo() {delete pinP;}
};

std::vector<pinInfo> pinVec;
};


template<class T>
T *XrdOucPinKing<T>::Load(const char *Symbol)
{
   XrdOucPinObject<T> *objPIN;
   T *lastPIN = 0;
   typename std::vector<pinInfo>::iterator it;

   for (it = pinVec.begin(); it != pinVec.end(); it++)
       {if (it->path.size() == 0) continue;
        it->pinP = new XrdOucPinLoader(&eMsg, vInfo, Drctv, it->path.c_str());
        objPIN = (XrdOucPinObject<T> *)(it->pinP->Resolve(Symbol));
        if (!objPIN
        || !(lastPIN = objPIN->getInstance(it->parm.c_str(), eInfo,
                                           *(eMsg.logger()), lastPIN)))
           return 0;
       }
   return lastPIN;
}
#endif
