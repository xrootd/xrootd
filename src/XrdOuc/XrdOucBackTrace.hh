#ifndef _XRDOUCBACKTRACE_HH_
#define _XRDOUCBACKTRACE_HH_
/******************************************************************************/
/*                                                                            */
/*                    X r d O u c B a c k T r a c e . h h                     */
/*                                                                            */
/*(c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University   */
/*Produced by Andrew Hanushevsky for Stanford University under contract       */
/*           DE-AC02-76-SFO0515 with the Deprtment of Energy                  */
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

//-----------------------------------------------------------------------------
//! XrdOucBackTrace provides a way to perform a back trace to assist in
//! debugging esoteric problems. It is controlled by various envars or options
//! can be set by methods in this class. It is fully MT_safe. However, a full
//! back trace is only available for programs compiled using a GNU compiler.
//!
//! Back tracing can be selectively controlled by filters. As filters interact,
//! one must understand how they are applied. There are four filters:
//! 1) this     pointer matching
//! 2) object   pointer matching
//! 3) request  code    matching (XrdBT only)
//! 4) response code    matching (XrdBT only, also known as the status code)
//!
//! Both DoBT() and XrdBT() apply this and pointer filters while XrdBT() also
//! applies request and response filters while DoBT() does not apply them.
//! When DoBT() or XrdBT() is called, the following sequence occurs:
//! 1) If the this   filter is set and the this   pointer matches a BT occurs.
//! 2) if the object filter is set and the object pointer matches a BT occurs.
//! 3) If both pointer filters are set then no BT occurs, period.
//! 4) If only one of the pointer filters is set then DoBT() does not do a BT.
//! 5) XrdBT() also does not do a BT if the neither code matching filters are
//!    set. If one of the code filters is set then a BT occurs if both filters
//!    succeed (note that an unset code filter always succeeds).
//!
//! Two handy commands in gdb to convert a back trace to source file line:
//! info line *addr
//! list *addr
//!      The addr is the address that appears in the back trace in brackets.
//!      The 'info line' gives you a close approximation while list provides
//!      more context with a suggested line (optimization makes this obtuse).
//!      For example, given [0x7ffff695db36] say 'info line *0x7ffff695db36'.
//-----------------------------------------------------------------------------

class XrdOucBackTrace
{
public:

//-----------------------------------------------------------------------------
//! Produce a back trace. The message header and corresponding back trace have
//! the format below. The traceback lines may differ if an error occurs. The
//! maximum levels displayed is controlled by the XRDBT_DEPTH envar. If not set,
//! or is invalid, Only the last 15 levels are displayed (maximum depth is 30).
//! This version is geared for general applications (see XrdBT() alternative).
//!
//! TBT <thread_id> <thisP> [<head>] obj <objP> [<tail>]
//! TBT <thread_id> [<addr_of_ret>] <func>(<args>)+offs
//!
//! @param  head    Points to text to be included in back trace header. A nil
//!                 pointer indicates there is no information.
//! @param  thisP   Is the this pointer of the caller. If there is no this
//!                 pointer, pass nil. The address is included in the header.
//!                 Use Filter() to filter the address.
//! @param  objP    Pointer to an object of interest. It's address is included
//!                 in the header. Use Filter() to filter the address.
//! @param  tail    Pointer to text to be included at the end of the header.
//!                 A nil pointer indicates there is none.
//! @param  force   When true, all filters are ignored.
//-----------------------------------------------------------------------------

static void DoBT(const char *head=0, void *thisP=0, void *objP=0,
                 const char *tail=0, bool  force=false);

//-----------------------------------------------------------------------------
//! Do optional one time static intialization. Invoke this method at file level
//! (e.g. bool aOK = XrdOucBackTrace::Init()) if you wish to set xrootd
//! specific filters when calling XrdBT(). Otherwise, don't use this method.
//!
//! @param  reqs    The kXR_ request code name(s). If the pointer is nil, the
//!                 back trace filter is set using envar XRDBT_REQFILTER as the
//!                 argument. If both are nil, no filter is established.
//!                 Specify, one or more names, each separated by a space.
//!                 Invalid names are ignored. Choose from this list:
//!
//!                 admin auth bind chmod close dirlist endsess getfile locate
//!                 login mkdir mv open ping prepare protocol putfile query read
//!                 readv rm rmdir set stat statx sync truncate verifyw write
//!
//! @param  rsps    The kXR_ response code name(s). If the pointer is nil, the
//!                 back trace filter is set using envar XRDBT_RSPFILTER as the
//!                 argument. If both are nil, no filter is established.
//!                 Specify, one or more names, each separated by a space.
//!                 Invalid names are ignored. Choose from this list:
//!
//!                 attn authmore error ok oksofar redirect wait waitresp
//!
//! @return true    Initialization succeeded.
//! @return false   Initialization completed but one or more reqs or rsps were
//!                 ignored because they were invalid.
//-----------------------------------------------------------------------------

static bool Init(const char *reqs=0, const char *rsps=0);

//-----------------------------------------------------------------------------
//! Define filter types and actions.
//-----------------------------------------------------------------------------

enum PtrType {isThis,  //!< Pointer is a  this   pointer
              isObject //!< Pointer is an object pointer
             };

enum Action  {addIt=0, //!< Add item to the list of PtrTypes being filtered.
              clrIt,   //!< Delete all PtrType filtered items (1st arg ignored).
              delIt,   //!< Delete this item from the list of PtrTypes filtered.
              repIt    //!< Replace all PtrTypes items filtered with this item.
             };

//-----------------------------------------------------------------------------
//! Set a pointer filter. Back traces only occur when the corresponding pointer
//! is passed to DoBT() or XrdBT(). See filtering explanation above.
//!
//! @param  ptr     The pointer.
//! @param  pType   The pointer's logical type (see PtrType defined above).
//! @param  how     One of the action enums in Action (defined above).
//-----------------------------------------------------------------------------

static void Filter(void *ptr, PtrType pType, Action how=addIt);

//-----------------------------------------------------------------------------
//! Produce an XrrotD  specific back trace. The message header and corresponding
//! back trace have the format below. The back trace lines may differ if an
//! error occurs. This version is for for XRootD applications. See the DoBT().
//!
//! TBT <thread_id> <thisP> <head> obj <objP> rsp <statN> req <reqN> <tail>
//! TBT <thread_id> [<addr_of_ret>] <func>(<args>)+offs
//!
//! @param  head    Points to text to be included in back trace header
//! @param  thisP   Is the this pointer of the caller. If there is no this
//!                 pointer, pass nil. The address is included in the header.
//!                 User Filter() to filter the pointer.
//! @param  objP    Pointer to an object of interest. It's address is included
//!                 in the header. Use Filter() to filter the pointer.
//! @param  rspN    The kXR_ status code reflected in an XRootD response. It's
//!                 corresponding name, if any, is included in the header.
//!                 Use Init() to filter the code value.
//! @param  reqN    The kXR_ request code of an XRootD request. It's
//!                 corresponding name, if any, is included in the header.
//!                 Use Init() to filter the code value.
//! @param  tail    Pointer to text to be included at the end of the header.
//!                 A nil pointer does not add any additional text.
//! @param  force   When true, all filters are ignored.
//-----------------------------------------------------------------------------

static void XrdBT(const char *head=0,  void *thisP=0,       void *objP=0,
                        int   rspN=0,  int   reqN=0,  const char *tail=0,
                        bool  force=false);
};
#endif
