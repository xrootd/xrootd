#ifndef ___XRDVOMS_TRACE_H___
#define ___XRDVOMS_TRACE_H___
/******************************************************************************/
/*                                                                            */
/*                       X r d V o m s T r a c e . h h                        */
/*                                                                            */
/* (C) 2005  G. Ganis, CERN                                                   */
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
/*                                                                            */
/******************************************************************************/

#include "XrdSys/XrdSysLogger.hh"

#ifndef NODEBUG

#define PRINT(y)    if (gDebug) {std::cerr <<gLogger->traceBeg() <<" XrdVoms"\
                                      <<epname <<": " <<y <<'\n' << std::flush;\
                                 gLogger->traceEnd();}
#define DEBUG(y)    if (gDebug > 1) {PRINT(y)}
#define EPNAME(x)   static const char *epname = x;

#else

#define  PRINT(x)
#define  DEBUG(x)
#define EPNAME(x)

#endif

#endif
