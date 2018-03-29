#ifndef __XRDCKSCALC_HH__
#define __XRDCKSCALC_HH__
/******************************************************************************/
/*                                                                            */
/*                         X r d C k s C a l c . h h                          */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/*! This class defines the interface to a checksum computation. When this class
    is used to define a plugin computation, the initial XrdCksCalc computation
    object is created by the XrdCksCalcInit() function defined at the end of
    this file.
*/
  
class XrdCksCalc
{
public:

//------------------------------------------------------------------------------
//! Calculate a one-time checksum. The obvious default implementation is
//! provided and assumes that Init() may be called more than once.
//!
//! @param    Buff   -> Data to be checksummed.
//! @param    BLen   -> Length of the data in Buff.
//!
//! @return   the checksum value in binary format. The pointer to the value
//!           becomes invalid once the associated object is deleted.
//------------------------------------------------------------------------------

virtual char *Calc(const char *Buff, int BLen)
                  {Init(); Update(Buff, BLen); return Final();}

//------------------------------------------------------------------------------
//! Get the current binary checksum value (defaults to final). However, the
//! final checksum result is not affected.
//!
//! @return   the checksum value in binary format. The pointer to the value
//!           becomes invalid once the associated object is deleted.
//------------------------------------------------------------------------------

virtual char *Current() {return Final();}

//------------------------------------------------------------------------------
//! Get the actual checksum in binary format.
//!
//! @return   the checksum value in binary format. The pointer to the value
//!           becomes invalid once the associated object is deleted.
//------------------------------------------------------------------------------

virtual char *Final() = 0;

//------------------------------------------------------------------------------
//! Initializes data structures (must be called by constructor). This is always
//! called to reuse the object for a new checksum.
//------------------------------------------------------------------------------

virtual void  Init() = 0;

//------------------------------------------------------------------------------
//! Get a new instance of the underlying checksum calculation object.
//!
//! @return   the checksum calculation object.
//------------------------------------------------------------------------------
virtual
XrdCksCalc   *New() = 0;

//------------------------------------------------------------------------------
//! Recycle the checksum object as it is no longer needed. A default is given.
//------------------------------------------------------------------------------

virtual void  Recycle() {delete this;}

//------------------------------------------------------------------------------
//! Get the checksum object algorithm name and the number bytes (i.e. size)
//! required for the checksum value.
//!
//! @param    csSize -> Parameter to hold the size of the checksum value.
//!
//! @return   the checksum algorithm's name. The name persists event after the
//!           checksum object is deleted.
//------------------------------------------------------------------------------

virtual const char *Type(int &csSize) = 0;

//------------------------------------------------------------------------------
//! Compute a running checksum. This method may be called repeatedly for data
//! segments; with Final() returning the full checksum.
//!
//! @param    Buff   -> Data to be checksummed.
//! @param    BLen   -> Length of the data in Buff.
//------------------------------------------------------------------------------

virtual void  Update(const char *Buff, int BLen) = 0;

//------------------------------------------------------------------------------
//! Constructor
//------------------------------------------------------------------------------

              XrdCksCalc() {}

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual      ~XrdCksCalc() {}
};

/******************************************************************************/
/*               C h e c k s u m   O b j e c t   C r e a t o r                */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Obtain an instance of the checksum calculation object.
//!
//! XrdCksCalcInit() is an extern "C" function that is called to obtain an
//! initial instance of a checksum calculation object. You may create custom
//! checksum calculation and use them as plug-ins to the  checksum manager
//! (see XrdCks.hh). The function must be defined in the plug-in shared library.
//! All the following extern symbols must be defined at file level!
//!
//! @param eDest  -> The XrdSysError object for messages.
//! @param csName -> The name of the checksum algorithm.
//! @param cFN    -> The name of the configuration file
//! @param Parms  -> Parameters specified on the ckslib directive. If none it is
//!                  zero.
//------------------------------------------------------------------------------

/*! extern "C" XrdCksCalc *XrdCksCalcInit(XrdSysError *eDest,
                                          const char  *csName,
                                          const char  *cFN,
                                          const char  *Parms);
*/

//------------------------------------------------------------------------------
//! Declare the compilation version number.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. While not currently required, it is highly recommended to
//! avoid execution issues should the class definition change. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdCksCalcInit,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif
