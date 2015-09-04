#ifndef __XRDSSIPROVIDER_HH__
#define __XRDSSIPROVIDER_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d S s i P r o v i d e r . h h                      */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

//-----------------------------------------------------------------------------
//! The XrdSsiProvider object is used by the Scalable Service Interface
//! for two purposes:
//! 1) To ascertain the availability of a resource on a server node in an SSI
//!    cluster.
//! 2) To obtain a service object that can provision one or more resources.
//!
//! Client-side: A providor object is predefined in libXrdSsi.so and must be
//!              used by the client code to get service objects, as follows:
//!
//!              extern XrdSsiProvider *XrdSsiProviderClient;
//!              XrdSsiService *ClientService = XrdSsiProviderClient->
//!                                             GetService("hostname:port");
//!
//!
//! Server-side: the providor object is obtained from the plugin library which
//!              should have the XrdSsiProviderLookup and XrdSsiProviderServer
//!              pointer symbols defined at file level (i.e. static global).
//!
//!              The object pointed to by XrdSsiProviderLookup is used to obtain
//!              resource availability information via QueryResource() and a
//!              service object is never obtained (i.e. no call to GetService).
//!
//!              The object pointed to by XrdSsiProviderServer is used to effect
//!              service requests and thus *does* obtain a service object via
//!              a GetService() call.
//!
//!              These pointers are typically defined, as follows:
//!
//!              XrdSsiProvider *XrdSsiProviderLookup
//!                                    = new MyLookupProvider(....);
//!              XrdSsiProvider *XrdSsiProviderServer
//!                                    = new MyServerProvider(....);
//!
//!              where MyLookupProvider and MyServerProvider objects must
//!              inherit class XrdSsiProvider.
//!
//!              You use the following directives to configure the
//!              service providor for only the process that runs the cmsd:
//!
//!              all.role server
//!              all.manager <redirector-cmsd>:<port>
//!              oss.statlib  -2 <path>/libXrdSsi.so
//-----------------------------------------------------------------------------

class XrdSsiCluster;
class XrdSsiErrInfo;
class XrdSsiLogger;
class XrdSsiService;


class XrdSsiProvider
{
public:

//-----------------------------------------------------------------------------
//! Obtain a client-side service object.
//!
//! @param  eInfo    the object where error status is to be placed.
//! @param  contact  the point of first contact when provisioning the service.
//!                  The contact may be "host:port" where "host" is a DNS name,
//!                  an IPV4 address (i.e. d.d.d.d), or an IPV6 address
//!                  (i.e. [x:x:x:x:x:x]), and "port" is either a numeric port
//!                  number or the service name assigned to the port number.
//!                  This pointer is nil if the call is being made server-side.
//!                  Note that only one service object is obtained by a server.
//! @param  oHold    the maximum number of session objects that should be held
//!                  in reserve for future Provision() calls.
//!
//! @return =0       A service object could not be created, eInfo has the reason.
//! @return !0       Pointer to a service object.
//-----------------------------------------------------------------------------

virtual
XrdSsiService *GetService(XrdSsiErrInfo &eInfo,
                          const char    *contact,
                          int            oHold=256
                         ) {return 0;}

//-----------------------------------------------------------------------------
//! Initialize server-side processing. This method is invoked prior to any
//! other method in the XrdSsiProvider object.
//!
//! @param  logP   pointer to the logger object for message routing.
//! @param  clsP   pointer to the cluster management object. This pointer is nil
//!                when a service object is being obtained by an unclustered
//!                system (i.e. a stand-alone server).
//! @param  cfgFn  pointer to the conifiguration file name.
//! @param  parms  pointer to the conifiguration parameters or nil if none.
//! @param  argc   The count of command line arguments (always >= 1).
//! @param  argv   Pointer to a null terminated array of tokenized command line
//!                arguments. These arguments are taken from the command line
//!                after the "-+xrdssi" option (see invoking xrootd), if present.
//!                The first argument is always the same as argv[0] in main().
//!
//! @return true   Initialization succeeded.
//! @return =0     Initialization failed. The method should include an error
//!                message in the log indicating why initialization failed.
//-----------------------------------------------------------------------------

virtual bool   Init(XrdSsiLogger  *logP,
                    XrdSsiCluster *clsP,
                    const char    *cfgFn,
                    const char    *parms,
                    int            argc,
                    char         **argv
                   ) = 0;

//-----------------------------------------------------------------------------
//! Obtain the version of the abstract class used by underlying implementation.
//! The version returned must match the version compiled in the loading library.
//! If it does not, initialization fails.
//-----------------------------------------------------------------------------

static const int SsiVersion = 0x00010000;

       int     GetVersion() {return SsiVersion;}

//-----------------------------------------------------------------------------
//! Obtain the status of a resource.
//! Client-side:  This method can be called to obtain the availability of a
//!               resource relative to a particular endpoint.
//! Server-Side:  When configured via oss.statlib directive, this is called
//!               server-side by the XrdSsiCluster object to see if the resource
//!               can be provided by the providor via a service object.
//!
//! @param  rName    Pointer to the resource name.
//! @param  contact  the point of first contact that would be used to provision
//!                  the resource (see client-side GetService() for details).
//!                  A nil pointer indicates a query for availibity at the
//!                  local node (e.g. a cmsd query for resource availability).
//!
//! @return          One of the rStat enums, as follows:
//!                  notPresent - resource not present on this node.
//!                   isPresent - resource is  present and can be
//!                               immediately used, if necessary.
//!                   isPending - resource is  present but is not in an
//!                               immediately usable state, access may wait.
//-----------------------------------------------------------------------------

enum    rStat  {notPresent = 0, isPresent, isPending};

virtual rStat  QueryResource(const char *rName,
                             const char *contact=0
                            ) = 0;

//-----------------------------------------------------------------------------
//! Set the maximum number of threads for handling callbacks (client-side only).
//! When the maximum is reached, callbacks wait until an in-progress callback
//! completes. This method must be called prior to calling GetService().
//! This method has no meaning server-side and is ignored.
//!
//! @param  tNum     The maximum number of threads to be used (default is 300).
//!                  In practice twice this number of threads is allowed.
//-----------------------------------------------------------------------------

virtual void   SetCBThreads(int tNum) {(void)tNum;}

//-----------------------------------------------------------------------------
//! Set default global timeouts. By default, all timeouts are set to infinity.
//!
//! @param  what     One of the enums below specifying the timeout is to be set.
//! @param  tmoval   The timeout valid in seconds. A value of <= 0 is ignored.
//-----------------------------------------------------------------------------

enum    tmoType {idleClose=0, //!< Time before an idle socket is closed (client)
                 request_T,   //!< Time to wait for a request to finsish(client)
                 response_T,  //!< Time for client to wait for a resp   (Server)
                 stream_T     //!< Time to wait for socket activity     (Client)
                };

virtual void   SetTimeout(tmoType what, int tmoval) {(void)what; (void)tmoval;}

//-----------------------------------------------------------------------------
//! Constructor
//-----------------------------------------------------------------------------

               XrdSsiProvider() {}
protected:

//-----------------------------------------------------------------------------
//! Destructor. The providor object cannot be and never is explicitly deleted.
//-----------------------------------------------------------------------------

virtual       ~XrdSsiProvider() {}
};
#endif
