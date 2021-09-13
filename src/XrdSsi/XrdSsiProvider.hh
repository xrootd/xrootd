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
//! 2) To obtain a service object that can process one or more requests.
//!
//! Client-side: A provider object is predefined in libXrdSsi.so and must be
//!              used by the client code to get service objects, as follows:
//!
//!              extern XrdSsiProvider *XrdSsiProviderClient;
//!              XrdSsiService *ClientService = XrdSsiProviderClient->
//!                                             GetService("hostname:port");
//!
//!
//! Server-side: the provider object is obtained from the plugin library which
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
//!              service provider for only the process that runs the cmsd:
//!
//!              all.role server
//!              all.manager <redirector-cmsd>:<port>
//!              oss.statlib  -2 <path>/libXrdSsi.so
//!
//! Warning! All methods (except Init()) in this class must be thread-safe.
//-----------------------------------------------------------------------------

#include <cerrno>

#include "XrdSsi/XrdSsiErrInfo.hh"
#include "XrdSsi/XrdSsiResource.hh"

class XrdSsiCluster;
class XrdSsiLogger;
class XrdSsiService;

class XrdSsiProvider
{
public:

//-----------------------------------------------------------------------------
//! Issue a control operation (future).
//!
//! @param  cmd      The control command.
//! @param  argP     The operational argument cast to a void pointer.
//! @param  resP     A reference to the pointer to hold the operational
//!                  result object if any.
//!
//! @return Upon success 0 is returned and if a resutt is returned is must
//!         cast to te correct pointer type and deleted,when no longer needed.
//!         Upon failure, -errno is returned.
//-----------------------------------------------------------------------------

enum CTL_Cmd {CTL_None = 0};

virtual int    Control(CTL_Cmd cmd, const void *argP, void *&resP)
                      {(void)cmd; (void)argP; (void)resP;
                       return (cmd == CTL_None ? 0 : -ENOTSUP);
                      }

//-----------------------------------------------------------------------------
//! Obtain a service object (client-side or server-side).
//!
//! @param  eInfo    the object where error status is to be placed.
//! @param  contact  the point of first contact when processing a request.
//!                  The contact may be "host:port" where "host" is a DNS name,
//!                  an IPV4 address (i.e. d.d.d.d), or an IPV6 address
//!                  (i.e. [x:x:x:x:x:x]), and "port" is either a numeric port
//!                  number or the service name assigned to the port number.
//!                  You may specify more than one contact by separating each
//!                  with a comma (e.g. host1:port,host2:port,...). Each host
//!                  must be equivalent with respect to request processing.
//!                  This is a null string if the call is being made server-side.
//!                  Note that only one service object is obtained by a server.
//! @param  oHold    the maximum number of request objects that should be held
//!                  in reserve for future calls.
//!
//! @return =0       A service object could not be created, eInfo has the reason.
//! @return !0       Pointer to a service object.
//-----------------------------------------------------------------------------

virtual
XrdSsiService *GetService(XrdSsiErrInfo     &eInfo,
                          const std::string &contact,
                          int                oHold=256
                         ) {eInfo.Set("Service not implemented!", ENOTSUP);
                            return 0;
                           }

//-----------------------------------------------------------------------------
//! Obtain the version of the abstract class used by underlying implementation.
//! The version returned must match the version compiled in the loading library.
//! If it does not, initialization fails.
//-----------------------------------------------------------------------------

static const int SsiVersion = 0x00010000;

       int     GetVersion() {return SsiVersion;}

//-----------------------------------------------------------------------------
//! Initialize server-side processing. This method is invoked prior to any
//! other method in the XrdSsiProvider object.
//!
//! @param  logP   pointer to the logger object for message routing.
//! @param  clsP   pointer to the cluster management object. This pointer is nil
//!                when a service object is being obtained by an unclustered
//!                system (i.e. a stand-alone server).
//! @param  cfgFn  file path to the the conifiguration file.
//! @param  parms  conifiguration parameters, if any.
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
                    std::string    cfgFn,
                    std::string    parms,
                    int            argc,
                    char         **argv
                   ) = 0;

//-----------------------------------------------------------------------------
//! Obtain the status of a resource.
//! Client-side:  This method can be called to obtain the availability of a
//!               resource relative to a particular endpoint.
//! Server-Side:  When configured via oss.statlib directive, this is called
//!               server-side by the XrdSsiCluster object to see if the resource
//!               can be provided by the providor via a service object. This
//!               method is also used server-side to determine resource status.
//!
//! @param  rName    Pointer to the resource name.
//! @param  contact  the point of first contact that would be used to process
//!                  the request relative to the resource (see ProcessRequest()).
//!                  A nil pointer indicates a query for availibity at the
//!                  local node (e.g. a query for local resource availability).
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
//! Notify provider that a resource was added to this node. This method is
//! called by the cmsd process in response to calling XrdSsiCluster::Added()
//! in the xrootd process. This method only is invoked on resource storage
//! nodes (i.e. all.role server).
//!
//! @param  rName    Pointer to the resource name that was added.
//-----------------------------------------------------------------------------

virtual void   ResourceAdded(const char *rName) {}

//-----------------------------------------------------------------------------
//! Notify provider that a resource was removed from this node. This method is
//! called by the cmsd process in response to calling XrdSsiCluster::Removed()
//! in the xrootd process. This method only is invoked on resource storage
//! nodes (i.e. all.role server).
//!
//! @param  rName    Pointer to the resource name that was removed.
//-----------------------------------------------------------------------------

virtual void   ResourceRemoved(const char *rName) {}

//-----------------------------------------------------------------------------
//! Set thread values. This call is deprecated. You should use SetConfig().
//! This method has no meaning server-side and is ignored.
//!
//! @param  cbNum    Equivalent to SetConfig("cbThreads",  cbNum).
//! @param  ntNum    Equivalent to SetConfig("netThreads", ntNum).
//-----------------------------------------------------------------------------

virtual void   SetCBThreads(int cbNum, int ntNum=0) {(void)cbNum; (void)ntNum;}

//-----------------------------------------------------------------------------
//! Set a configuration option for execution behaviour.
//!
//! @param  eInfo    The object where error status is to be placed.
//! @param  optname  The name of the option (see table below).
//! @param  optvalue The value to be set for the option.
//!
//! @return true     Option set.
//! @return false    Option not set, eInfo holds the reason.
//!
//! @note All calls to SetConfig() must occur before GetService() is called.
//-----------------------------------------------------------------------------

/*! The following table list the currently supported keynames and what the
    value actually does. These options only apply to the client. The options
    must be set before calling GetService().

    cbThreads        The maximum number of threads to be used for callbacks and
                     sets the maximum number of active callbacks (default 300).
                     set a value between 1 and 32767. Note: the nproc ulimit
                     is final arbiter of the actual number of threads to use.
    hiResTime        enables the use of a high resolution timer in log message
                     timestamps. The optvalue is immaterial as this simply sets
                     feature on and once set on cannot be set off.
    netThreads       The maximum number of threads to be used to handle network
                     traffic. The minimum is 3, the default is 10% of cbThreads
                     but no more than 100.
    pollers          The number network interrupt pollers to run. Larger values
                     allow the initial fielding of more interrupts. Care must
                     be taken to not overrun netThreads. The default is 3. The
                     suggested maximum is the number of cores.
    reqDispatch      Request dispatch algorithm to use when contact has multiple
                     endpoints. Choose one of:
                     < 0: Random choice each time.
                     = 0: Use DNS order.
                     > 0: Round robbin (the default).
*/

virtual bool SetConfig(XrdSsiErrInfo &eInfo,
                       std::string   &optname,
                       int            optvalue)
                      {(void)optname; (void)optvalue; return 0;}

//-----------------------------------------------------------------------------
//! Set the client-size request spread size.
//! @param  ssz      The spread value which may be <0, -0, or >0:
//!                  >0 The maximum number of connections to use to to handle
//!                     requests. No more or less are used until reset.
//!                  =0 Turns on auto-tuning using curent setting. The initial
//!                     default is 4. The size is automatically increased to
//!                     accomodate the number of simultaeous requests.
//!                  <0 Turns on auto-tuning using curent setting using the
//!                     abs(sval) as the new spread value. The size is
//!                     The size is automatically increased to accomodate the
//!                     number of simultaeous requests.
//!
//! @note   This method may be called at any time. An abs(ssz) value > 1024
//!         is set to 1024. Auto-tuning must be requested; it's not the default.
//-----------------------------------------------------------------------------

virtual void   SetSpread(short ssz) {(void)ssz;}

//-----------------------------------------------------------------------------
//! Set default global timeouts. By default, all timeouts are set to infinity.
//!
//! @param  what     One of the enums below specifying the timeout is to be set.
//! @param  tmoval   The timeout valid in seconds. A value of <= 0 is ignored.
//-----------------------------------------------------------------------------

enum    tmoType {connect_N=0, //!< Number of times to try connection    (client)
                 connect_T,   //!< Time to wait for a connection        (client)
                 idleClose,   //!< Time before an idle socket is closed (client)
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
