#ifndef __XRDSSISERVICE_HH__
#define __XRDSSISERVICE_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d S s i S e r v i c e . h h                       */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSsi/XrdSsiErrInfo.hh"

//-----------------------------------------------------------------------------
//! The XrdSsiService object is used by the Scalable Service Interface to
//! process client requests.
//!
//! There may be many client-side service objects, as needed. However, only
//! one such object is obtained server-side. The object is used to effect all
//! service requests and handle responses.
//!
//! Client-side: the service object is obtained via XrdSsiGetClientService().
//!
//! Server-side: the service object is obtained via an extern "C" plugin,
//!              XrdSsiGetServerService() defined in a shared library. You use
//!              the following directives to configure the service:
//!
//!              all.role server        <--- Only for clustered configurations
//!              oss.statlib  <path>/libXrdSsi.so [Optional, see QueryResource]
//!              xrootd.fslib <path>/libXrdSsi.so
//!              ssi.svclib   <path>/<your shared library>
//-----------------------------------------------------------------------------

class XrdSsiEntity;
class XrdSsiSession;

class XrdSsiService
{
public:

//-----------------------------------------------------------------------------
//! Obtain the version of the abstract class used by underlying implementation.
//! The version returned must match the version compiled in the loading library.
//! If it does not, initialization fails.
//-----------------------------------------------------------------------------

static const int SsiVersion = 0x00010000;

       int     GetVersion() {return SsiVersion;}

//-----------------------------------------------------------------------------
//! The Resource class describes the session resource to be provisioned and is
//! and is used to communicate the results of the provisioning request.
//-----------------------------------------------------------------------------

class          Resource
{
public:
const char    *rName;  //!< -> Name of the resource to be provisioned
const char    *hAvoid; //!< -> Comma separated list of hosts to avoid
XrdSsiEntity  *client; //!< -> Pointer to client identification (server-side)
XrdSsiErrInfo  eInfo;  //!<    Holds error information upon failure

//-----------------------------------------------------------------------------
//! Handle the ending results of a Provision() call. It is called by the
//! Service object when provisioning that has actually started completes.
//!
//! @param  sessP    !0 -> to the successfully provisioned session object.
//!                  =0 Provisioning failed, the eInfo object holds the reason.
//-----------------------------------------------------------------------------

virtual void   ProvisionDone(XrdSsiSession *sessP) = 0; //!< Callback

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  rname    points to the name of the resource and must remain valid
//!                  until provisioning ends. The resource name must start a
//!                  slash. Duplicate slashes amd dot-shashes are compressed.
//!
//! @param  havoid   if not null then points to a comma separated list of
//!                  hostnames to avoid using to provision the resource and
//!                  must remain valid until provisioning ends. This argument
//!                  is only meaningfull client-side.
//-----------------------------------------------------------------------------

               Resource(const char *rname, const char *havoid=0)
                       : rName(rname), hAvoid(havoid) {}

//-----------------------------------------------------------------------------
//! Destructor
//-----------------------------------------------------------------------------

virtual       ~Resource() {}
};

//-----------------------------------------------------------------------------
//! Provision of service session; client-side or server-side.
//!
//! @param  resP     Pointer to the Resource object (see above) that describes
//!                  the resource to be provisioned.
//!
//! @param  timeOut  the maximum number seconds the operation may last before
//!                  it is considered to fail. A zero value uses the default.
//!
//! @return true     Provisioning has started; resP->ProvisionDone() will be
//!                  called on a different thread with the final result.
//!
//! @return false    Provisoning could not be started. resP->eInfo holds the
//!                  reason for the failure.
//!
//! Special notes for server-side processing:
//!
//! 1) Two special errors are recognized that allow for a client retry:
//!
//!    resP->eInfo.eNum = EAGAIN (client should retry elsewhere)
//!    resP->eInfo.eMsg = the host name where the client is redirected
//!    resP->eInfo.eArg = the port number to be used by the client
//!
//!    resP->eInfo.eNum = EBUSY  (client should wait and then retry).
//!    resP->eInfo.eMsg = an optional reason for the wait.
//!    resP->eInfo.eArg = the number of seconds the client should wait.
//-----------------------------------------------------------------------------

virtual bool   Provision(Resource       *resP,
                         unsigned short  timeOut=0
                        ) = 0;

//-----------------------------------------------------------------------------
//! Obtain the status of a resource. When configured via oss.statlib directive,
//! this is called server-side by the XrdSsiCluster object to see if a
//! resource is available.
//!
//! @param  rName    Pointer to the resource name.
//!
//! @return          One of the rStat enums, as follows:
//!                  notAvailable - resource not available.
//!                   isAvailable - resource is  available and can be
//!                                 immediately used, if necessary.
//!                   isPending   - resource is  available but is not in an
//!                                 immediately usable state, access may wait.
//-----------------------------------------------------------------------------

enum    rStat  {notAvailable = 0, isAvailable, isPending};

virtual rStat  QueryResource(const char *rName)
                            {(void)rName; return notAvailable;}

//-----------------------------------------------------------------------------
//! Stop the client-side service. This is never called server-side.
//!
//! @return true     Service has been stopped and this object has been deleted.
//! @return false    Service cannot be stopped because there are still active
//!                  sessions. Unprovision all the sessions then call Stop().
//-----------------------------------------------------------------------------

virtual bool   Stop() {return false;}

//-----------------------------------------------------------------------------
//! Constructor
//-----------------------------------------------------------------------------

               XrdSsiService() {}
protected:

//-----------------------------------------------------------------------------
//! Destructor. The service object canot be explicitly deleted. Use Stop().
//-----------------------------------------------------------------------------

virtual       ~XrdSsiService() {}
};
  
/******************************************************************************/
/*            X r d S s i S e r v i c e   I n s t a n t i a t o r             */
/******************************************************************************/
/******************************************************************************/
/*                           C l i e n t - S i d e                            */
/******************************************************************************/
//-----------------------------------------------------------------------------
//! Obtain a client-side service object.
//!
//! @param  eInfo    the object where error status is to be placed.
//! @param  contact  the point of first contact when provisioning the service.
//!                  The contact may be "host:port" where "host" is a DNS name,
//!                  an IPV4 address (i.e. d.d.d.d), or an IPV6 address
//!                  (i.e. [x:x:x:x:x:x]), and "port" is either a numeric port
//!                  number or the service name assigned to the port number.
//! @param  oHold    the maximum number of session objects that should be held
//!                  in reserve for future Provision() calls.
//!
//! @return =0       A manager could not be created, eInfo has the reason.
//! @return !0       Pointer to a manager object.
//-----------------------------------------------------------------------------

    extern XrdSsiService *XrdSsiGetClientService(XrdSsiErrInfo &eInfo,
                                                 const char    *contact,
                                                 int            oHold=256
                                                );

/******************************************************************************/
/*                           S e r v e r - S i d e                            */
/******************************************************************************/

class XrdSsiCluster;
class XrdSsiLogger;

typedef XrdSsiService *(*XrdSsiServService_t)(XrdSsiLogger  *logP,
                                              XrdSsiCluster *clsP,
                                              const char    *cfgFn,
                                              const char    *parms,
                                                    int      argc,
                                                    char   **argv);
  
/*! Obtain a server-side service object (only one is ever obtained).
    When building a server-side shared library plugin, the following "C" entry
    point must exist in the library:

    extern "C"
          {XrdSsiService *XrdSsiGetServerService(XrdSsiLogger  *logP,
                                                 XrdSsiCluster *clsP,
                                                 const char    *cfgFn,
                                                 const char    *parms,
                                                       int      argc,
                                                       char   **argv);
          }

   @param  logP   pointer to the logger object for message routing.
   @param  clsP   pointer to the cluster management object. This pointer is null
                  when a service object is being obtained by the clustering
                  system itself for QueryResource() invocations.
   @param  cfgFn  pointer to the conifiguration file name.
   @param  parms  pointer to the conifiguration parameters or null if none.
                  This pointer may be null.
   @param  argc   The count of command line arguments (always >= 1).
   @param  argv   Pointer to a null terminated array of tokenized command line
                  arguments. These arguments are taken from the command line
                  after the "-+xrdssi" option (see invoking xrootd), if present.
                  The first argument is always the same as argv[0] in main().

   @return =0     the server object could not be created
   @return !0     pointer to the service object
*/
#endif
