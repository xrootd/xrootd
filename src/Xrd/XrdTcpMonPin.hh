#ifndef __XRDTCPMONPIN_H__
#define __XRDTCPMONPIN_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d T c p M o n P i n . h h                        */
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

/*! This class defines the XrdLink TCP post monitoring plugin. This plugin is
    used to generate additional moinitoring information, as needed, when the
    connection is about to be closed. The plugin should use the g-stream object
    recorded in the passed environment. If not found, the getInstance() method
    should return failure as this plugin should not be loaded unless such an
    object exists in the environment. You get the g-stream object specific
    to this plugin by executing the following (assume envR is the environment):

    @code {.cpp}
    XrdXrootdGStream *gS = (XrdXrootdGStream *)envR.GetPtr("TcpMon.gStream*");
    @endcode
*/

class XrdNetAddrInfo;

class XrdTcpMonPin
{
public:

//------------------------------------------------------------------------------
//! Produce monitoring information upon connection termination.
//!
//! @param  netInfo Reference to the network object associated with link.
//! @param  lnkInfo Reference to link-specific information.
//! @param  liLen   Byte length of lnkInfo being passed.
//------------------------------------------------------------------------------

struct LinkInfo
      {const char *tident;   //!< Pointer to the client's trace identifier
       int         fd;       //!< Socket file descriptor
       int         consec;   //!< Seconds connected
       long long   bytesIn;  //!< Bytes read  from the socket
       long long   bytesOut; //!< Bytes written to the socket
      };

virtual void Monitor(XrdNetAddrInfo &netInfo, LinkInfo &lnkInfo, int liLen) = 0;

             XrdTcpMonPin() {}
virtual     ~XrdTcpMonPin() {}
};

/*! An instance of the plugin is obtained by the plugin manager using the
    XrdOucPinObject class. The most straightforward way to implement this
    is to inherit the XrdOucPinObject class by a class of your choosing
    that defines a file level object named TcpMonPin, as follows:

    class myPinObject : public XrdOucPinObject<XrdTcpMonPin>
    {public:

     XrdTcpMonPin *getInstance(...) {provide concrete implementation}

    } TcpMonPin;

    see XrdOucPinObject.hh for additional details and the definition of the
    getInstance() method. There are many other ways to accomplish this
    including inheriting this class along with the XrdTcpMonPin class by
    the implementation class.
  
    You should also specify the compilation version. That is, the XRootD
    version you used to compile your plug-in. Declare it as:

    #include "XrdVersion.hh"
    XrdVERSIONINFO(TcpMonPin,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif
