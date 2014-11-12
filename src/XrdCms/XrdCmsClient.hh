#ifndef __CMS_CLIENT__
#define __CMS_CLIENT__
/******************************************************************************/
/*                                                                            */
/*                       X r d C m s C l i e n t . h h                        */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class  XrdOucEnv;
class  XrdOucErrInfo;
class  XrdOucLogger;
class  XrdOucTList;
struct XrdSfsPrep;
class  XrdSysLogger;

/******************************************************************************/
/*                    R e t u r n   C o n v e n t i o n s                     */
/******************************************************************************/
  
/* The following return conventions are use by Forward(), Locate(), & Prepare()
   Return Val   Resp.errcode          Resp.errtext
   ---------    -------------------   --------
   SFS_DATA     Length of data.       Data to be returned to caller.
                Action: Caller is provided data as successful response.

   SFS_ERROR    errno                 Error message text.
                Action: Caller given error response.

   SFS_REDIRECT port (0 for default)  Host name
                Action: Caller is redirected to <host>:<port>

   SFS_STARTED  Expected seconds      n/a
                Action: Caller is told to wait for the "expected seconds" for a
                        callback with the result. A callback must follow.
                        See how to do callbacks below.

   > 0          Wait time (= retval)  Reason for wait
                Action: Caller told to wait retval seconds and retry request.

   < 0          Error number          Error message
                Action: Same as SFS_ERROR. You should *always* use SFS_ERROR.

   = 0          Not applicable        Not applicable (see below)
                Action: Forward() -> Return success; request forwarded.
                        Locate()  -> Redirection does not apply, operation
                                     should be done against local file system.
                        Prepare() -> Return success, request submitted.
*/

/******************************************************************************/
/*                  C a l l b a c k   C o n v e n t i o n s                   */
/******************************************************************************/
  
/* Most operations allow you to return SFS_STARTED to setup a callback.
   Callback information is contained in the XrdOucErrInfo object passed to
   Forward(), Locate() and Prepare(); the only methods that can apply callbacks.
   Use a callback when the operation will take at least several seconds so as
   to not occupy the calling thread for an excessive amount of time.

   The actual mechanics of a callback are rather complicated because callbacks
   are subject to non-causaility if not correctly handled. In order to avoid
   such issues, you should use the XrdOucCallBack object (see XrdOucCallBack.hh)
   to test for applicability, setup, and effect a callback.

   When calling back, you return the same information you would have returned
   had the execution path been synchronous. From that standpoint callbacks are
   relatively easy to understand. All you are doing is defering the return of
   information without occupying a thread while waiting to do so.

   A typical scenario, using Resp and the original ErrInfo object, would be....

   XrdOucCallBack cbObject;  // Must be persistent for the callback duration

   if (XrdOucCallBack::Allowed(Resp))
      {cbObject.Init(Resp);
       <hand off the cbObject to a thread that will perform the work>
       Resp.setErrCode(<seconds end-point should wait>);
       return SFS_STARTED; // Effect callback response!
      }

   Once the thread doing the work has a result, send it via a callback as if
   the work was done in a synchronous fashion.

   cbObject->Reply(retValue, ErrCodeValue, ErrTextValue);
*/

/******************************************************************************/
/*                    C l a s s   X r d C m s C l i e n t                     */
/******************************************************************************/
  
class XrdCmsClient
{
public:

//------------------------------------------------------------------------------
//! Notify the cms of a newly added file or a file whose state has changed on
//! a data server node.
//!
//! @param  path  The logical file name.
//! @param  Pend  When true, the file is scheduled to be present in the future
//!               (e.g. copied in).
//------------------------------------------------------------------------------

virtual void   Added(const char *path, int Pend=0) { (void)path; (void)Pend; }

//------------------------------------------------------------------------------
//! Configure the client object.
//!
//! @param  cfn     The configuration file name.
//! @param  Parms   Any parameters specified in the cmslib directive. If none,
//!                 the pointer may be null.
//! @param  EnvInfo Environmental information of the caller.
//!
//! @return Success !0
//!         Failure =0
//------------------------------------------------------------------------------

virtual int    Configure(const char *cfn, char *Parms, XrdOucEnv *EnvInfo) = 0;

//------------------------------------------------------------------------------
//! Relay a meta-operation to all nodes in the cluster.
//!
//! This method is only used on manager nodes and is enabled by the ofs.forward
//! directive.
//!
//! @param   Resp Object where messages are to be returned.
//! @param   cmd  The operation being performed (see table below).
//!               If it starts with a '+' then a response (2way) is needed.
//!               Otherwise, a best-effort is all that is all that is required
//!               and success can always be returned.
//! @param   arg1 1st argument to cmd.
//! @param   arg2 2nd argument to cmd, which may be null if none exists.
//! @param   Env1 Associated environmental information for arg1 (e.g., cgi info
//!               which can be retrieved by Env1->Env(<len>)).
//! @param   Env2 Associated environmental information for arg2 (e.g., cgi info
//!               which can be retrieved by Env1->Env(<len>)).
//!
//!          cmd       arg1    arg2           cmd       arg1    arg2
//!          --------  ------  ------         --------  ------  ------
//!          [+]chmod  <path>  <mode %o>      [+]rmdir  <path>  0
//!          [+]mkdir  <path>  <mode %o>      [+]mv     <oldp>  <newp>
//!          [+]mkpath <path>  <mode %o>      [+]trunc  <path>  <size %lld>
//!          [+]rm     <path>  0
//!
//! @Return:   As explained under "return conventions".
//------------------------------------------------------------------------------

virtual int    Forward(XrdOucErrInfo &Resp,   const char *cmd,
                       const char    *arg1=0, const char *arg2=0,
                       XrdOucEnv     *Env1=0, XrdOucEnv  *Env2=0)
{
  (void)Resp; (void)cmd; (void)arg1; (void)arg2; (void)Env1; (void)Env2;
  return 0;
}

//------------------------------------------------------------------------------
//! Check if this client is configured for a manager node.
//!
//! @return !0 Yes, configured as a manager.
//!         =0 No.
//------------------------------------------------------------------------------

virtual int    isRemote() {return myPersona == XrdCmsClient::amRemote;}

//------------------------------------------------------------------------------
//! Retrieve file location information.
//!
//! @param   Resp  Object where message or response is to be returned.
//! @param   path  The logical path whise location is wanted.
//! @param   flags One or more of the following:
//!
//!          SFS_O_LOCATE  - return the list of servers that have the file.
//!                          Otherwise, redirect to the best server for the file.
//!          SFS_O_NOWAIT  - w/ SFS_O_LOCATE return readily available info.
//!                          Otherwise, select online files only.
//!          SFS_O_CREAT   - file will be created.
//!          SFS_O_NOWAIT  - select server if file is online.
//!          SFS_O_REPLICA - a replica of the file will be made.
//!          SFS_O_STAT    - only stat() information wanted.
//!          SFS_O_TRUNC   - file will be truncated.
//!
//!          For any the the above, additional flags are passed:
//!          SFS_O_META    - data will not change (inode operation only)
//!          SFS_O_RESET   - reset cached info and recaculate the location(s).
//!          SFS_O_WRONLY  - file will be only written    (o/w RDWR   or RDONLY).
//!          SFS_O_RDWR    - file may be read and written (o/w WRONLY or RDONLY).
//!
//! @param   Info Associated environmental information for arg2 (e.g., cgi info
//!               which can be retrieved by Env1->Env(<len>)).
//!
//! @return  As explained under "return conventions".
//------------------------------------------------------------------------------

virtual int    Locate(XrdOucErrInfo &Resp, const char *path, int flags,
                      XrdOucEnv  *Info=0) = 0;

//------------------------------------------------------------------------------
//! Obtain the list of cmsd's being used by a manager node along with their
//! associated index numbers, origin 1.
//!
//! @return The list of cmsd's being used. The list is considered permanent
//!         and is not deleted.
// Return:    A list of managers or null if none exist.
//------------------------------------------------------------------------------

virtual
XrdOucTList   *Managers() {return 0;}

//------------------------------------------------------------------------------
//! Start the preparation of a file for future processing.
//!
//! @param   Resp  Object where message or response is to be returned.
//! @param   pargs Information on which and how to prepare the file.
//! @param   Info  Associated environmental information.
//!
//! @return  As explained under "return conventions".
//------------------------------------------------------------------------------

virtual int    Prepare(XrdOucErrInfo &Resp, XrdSfsPrep &pargs,
                       XrdOucEnv  *Info=0)
{
  (void)Resp; (void)pargs; (void)Info;
  return 0;
}

//------------------------------------------------------------------------------
//! Notify the cmsd that a file or directory has been deleted. It is only called
//! called on a data server node.
//!
//! @param  path The logical file name that was removed.
//------------------------------------------------------------------------------

virtual void   Removed(const char *path) { (void)path; }

//------------------------------------------------------------------------------
//! Resume service after a suspension.
//!
//! @param  Perm When true the resume persist across server restarts. Otherwise,
//!              it is treated as a temporary request.
//------------------------------------------------------------------------------

virtual void   Resume (int Perm=1) { (void)Perm; }

//------------------------------------------------------------------------------
//! Suspend service.
//!
//! @param  Perm When true the suspend persist across server restarts.
//!              Otherwise, it is treated as a temporary request.
//------------------------------------------------------------------------------

virtual void   Suspend(int Perm=1) { (void)Perm; }

// The following set of functions can be used to control whether or not clients
// are dispatched to this data server based on a virtual resource. The default
// implementations do nothing.
//
//------------------------------------------------------------------------------
//! Enables the Reserve() & Release() methods.
//!
//! @param  n  a positive integer that specifies the amount of resource units
//!            that are available. It may be reset at any time.
//!
//! @return The previous resource value. This first call returns 0.
//------------------------------------------------------------------------------

virtual int    Resource(int n)   { (void)n; return 0;}

//------------------------------------------------------------------------------
//! Decreases the amount of resources available. When the available resources
//! becomes non-positive, perform a temporary suspend to prevent additional
//! clients from being dispatched to this data server.
//!
//! @param  n  The value by which resources are decreased (default 1).
//!
//! @return The amount of resource left.
//------------------------------------------------------------------------------

virtual int    Reserve (int n=1) { (void)n; return 0;}

//------------------------------------------------------------------------------
//! Increases the amount of resource available. When transitioning from a
//! a non-positive to a positive resource amount, perform a resume so that
//! additional clients may be dispatched to this data server.
//!
//! @param  n  The value to add to the resources available (default 1). The
//!            total amount is capped by the amount specified by Resource().
//!
//! @return The amount of resource left.
//------------------------------------------------------------------------------

virtual int    Release (int n=1) { (void)n; return 0;}

//------------------------------------------------------------------------------
//! Obtain the overall space usage of a cluster. Called only on manager nodes.
//!
//! @param  Resp  Object to hold response or error message.
//! @param  path  Associated logical path for the space request.
//! @param  Info  Associated cgi information for path.
//!
//! @return Space information as defined by the response to kYR_statfs. For a
//!               typical implementation see XrdCmsNode::do_StatFS().
//------------------------------------------------------------------------------

virtual int    Space(XrdOucErrInfo &Resp, const char *path,
                     XrdOucEnv  *Info=0) = 0;

//------------------------------------------------------------------------------
//! Constructor
//!
//! @param  acting  The type of function this object is performing.
//------------------------------------------------------------------------------

        enum   Persona {amLocal,  //!< Not affiliated with a cluster
                        amRemote, //!< Am a manager an issue redirects
                        amTarget  //!< Am a server  an field redirects
                       };

               XrdCmsClient(Persona acting) : myPersona(acting) {}

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual       ~XrdCmsClient() {}

protected:

Persona        myPersona;
};

/******************************************************************************/
/*              I n s t a n t i a t i o n   M o d e   F l a g s               */
/******************************************************************************/
  
/*! The following instantiation mode flags are passed to the instantiator (see
    comments that follow). They may be or'd together, depending on which mode
    the cms client should operate. They are defined as follows:
*/
namespace XrdCms
{
enum  {IsProxy  = 1, //!< The role is proxy  {plus one or more of the below}
       IsRedir  = 2, //!< The role is manager and will redirect users
       IsTarget = 4, //!< The role is server  and will be a redirection target
       IsMeta   = 8  //!< The role is meta   {plus one or more of the above}
      };
}

/******************************************************************************/
/*               C M S   C l i e n t   I n s t a n t i a t o r                */
/******************************************************************************/

//------------------------------------------------------------------------------
//! Obtain an instance of a configured XrdCmsClient.
//!
//! The following extern "C" function is called to obtain an instance of the
//! XrdCmsClient object. This is only used if the client is an actual plug-in
//! as identified by the ofs.cmslib directive. Once the XrdCmsClient object
//! is obtained, its Configure() method is called to initialize the object.
//!
//! @param  logger -> XrdSysLogger to be tied to an XrdSysError object for
//!                   any messages.
//! @param  opMode -> The operational mode as defined by the enum above. There
//!                   are two general types of clients, IsRedir and IsTarget.
//!                   The IsProxy and IsMeta modes are specialization of these
//!                   two basic types. The plug-in must provide an instance of
//!                   the one asked for whether or not they actually do anything.
//!
//!                   IsRedir  clients are anything other than a data provider
//!                            (i.e., data servers). These clients are expected
//!                            to locate files and redirect a requestor to an
//!                            actual data server.
//!
//!                   IsTarget clients are typically data providers (i.e., data
//!                            servers) but may actually do other functions are
//!                            are allowed to redirect as well.
//!
//! @param  myPort -> The server's port number.
//! @param  theSS  -> The object that implements he underlying storage system.
//!                   This object may be passed for historic reasons.
//!
//! @return Success: a pointer to the appropriate object (IsRedir or IsTarget).
//!
//!         Failure: a null pointer which causes initialization to fail.
//------------------------------------------------------------------------------

class XrdOss;

typedef XrdCmsClient *(*XrdCmsClient_t)(XrdSysLogger *, int, int, XrdOss *);

/*! extern "C" XrdCmsClient *XrdCmsGetClient(XrdSysLogger *Logger,
                                             int           opMode,
                                             int           myPort
                                             XrdOss       *theSS);
*/

//------------------------------------------------------------------------------
//! Obtain an instance of a default unconfigured XrdCmsClient.
//!
//! The following function may be called to obtain an instance of the default
//! XrdCmsClient object. The Configure() method is *not* called before the
//! object is returned. The parameters are the same as those for the function
//! XrdCmsGetClient(), above. Note that you need not supply a pointer to the
//! underlying storage system, as this is historic in nature.
//!
//! @return Success: a pointer to the appropriate object (IsRedir or IsTarget).
//!
//!         Failure: a null pointer, neither ISRedir nor IsTarget has been
//!                  specified or there is insufficient memory.
//------------------------------------------------------------------------------

namespace XrdCms
{
          XrdCmsClient *GetDefaultClient(XrdSysLogger *Logger,
                                         int           opMode,
                                         int           myPort
                                        );
}

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in.
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdCmsGetClient,<name>);

*/
#endif
