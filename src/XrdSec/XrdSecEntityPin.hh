#ifndef __SEC_ENTITYPIN_H__
#define __SEC_ENTITYPIN_H__
/******************************************************************************/
/*                                                                            */
/*                    X r d S e c E n t i t y P i n . h h                     */
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

/*! This class defines the XrdSecEntity object post processing plugin. When
    authentication succeeds, the plugin is called to inspect and possible
    decorate (e.g. add attributes) the entity object. The plugin also has the
    capability of returning failure due to some problem. When failure ooccurs,
    the security framework moves on to another authentication protocol, if one
    is avalable. Entity post processing plugins may be stacked. You always
    return the results of the stacked plugin whether or not you wish to handle
    the entity object, if a stacked plugin exists; unless you return false.
*/

class XrdOucErrInfo;
class XrdSecEntity;

class XrdSecEntityPin
{
public:

//------------------------------------------------------------------------------
//! Post process an authenticated entity object.
//!
//! @param  entity Reference to the entity object.
//! @param  einfo  Reference to errinfo object where a message that should be
//!                returned to the client on why post processing failed.
//!
//! @return true upon success and false upon failure with einfo containing
//!         the reason for the failure.
//------------------------------------------------------------------------------

virtual bool Process(XrdSecEntity &entity, XrdOucErrInfo &einfo) = 0;

             XrdSecEntityPin() {}
virtual     ~XrdSecEntityPin() {}
};

/*! An instance of the plugin is obtained by the plugin manager using the
    XrdOucPinObject class. The most straightforward way to implement this
    is to inherit the XrdOucPinObject class by a class of your choosing
    that defines a file level object named SecEntityPin, as follows:

    class myPinObject : public XrdOucPinObject<XrdSecEntityPin>
    {public:

     XrdSecEntityPin *getInstance(...) {provide concrete implementation}

    } SecEntityPin;

    see XrdOucPinObject.hh for additional details. There are many other
    ways to accomplish this including inheriting this class along with the
    XrdSecEntityPin class by the post processing implementation class.
  
    You should also specify the compilation version. That is, the XRootD
    version you used to compile your plug-in. Decalre it as:

    #include "XrdVersion.hh"
    XrdVERSIONINFO(SecEntityPin,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif
