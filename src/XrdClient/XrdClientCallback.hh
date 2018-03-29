#ifndef XRD_CLIENTCALLBACK_H
#define XRD_CLIENTCALLBACK_H
/******************************************************************************/
/*                                                                            */
/*                X r d C l i e n t C a l l b a c k . h h                     */
/*                                                                            */
/* Author: Fabrizio Furano (CERN IT-DSS, 2009)                                */
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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Base class for objects receiving events from XrdClient               //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

class XrdClientAbs;

class XrdClientCallback
{

public:

   // Invoked when an Open request completes with some result.
   virtual void OpenComplete(XrdClientAbs *clientP, void *cbArg, bool res) = 0;

   XrdClientCallback() {}
   virtual ~XrdClientCallback() {}
};
#endif
