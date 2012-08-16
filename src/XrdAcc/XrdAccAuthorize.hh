#ifndef __ACC_AUTHORIZE__
#define __ACC_AUTHORIZE__
/******************************************************************************/
/*                                                                            */
/*                    X r d A c c A u t h o r i z e . h h                     */
/*                                                                            */
/* (c) 2000 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include "XrdAcc/XrdAccPrivs.hh"

/******************************************************************************/
/*                      A c c e s s _ O p e r a t i o n                       */
/******************************************************************************/
  
//! The following are supported operations

enum Access_Operation  {AOP_Any      = 0,  //!< Special for getting privs
                        AOP_Chmod    = 1,  //!< chmod()
                        AOP_Chown    = 2,  //!< chown()
                        AOP_Create   = 3,  //!< open() with create
                        AOP_Delete   = 4,  //!< rm() or rmdir()
                        AOP_Insert   = 5,  //!< mv() for target
                        AOP_Lock     = 6,  //!< n/a
                        AOP_Mkdir    = 7,  //!< mkdir()
                        AOP_Read     = 8,  //!< open() r/o, prepare()
                        AOP_Readdir  = 9,  //!< opendir()
                        AOP_Rename   = 10, //!< mv() for source
                        AOP_Stat     = 11, //!< exists(), stat()
                        AOP_Update   = 12, //!< open() r/w or append
                        AOP_LastOp   = 12  //   For limits testing
                       };

/******************************************************************************/
/*                       X r d A c c A u t h o r i z e                        */
/******************************************************************************/
  
class XrdOucEnv;
class XrdSecEntity;

class XrdAccAuthorize
{
public:

//------------------------------------------------------------------------------
//! Check whether or not the client is permitted specified access to a path.
//!
//! @param     Entity    -> Authentication information
//! @param     path      -> The logical path which is the target of oper
//! @param     oper      -> The operation being attempted (see the enum above).
//!                         If the oper is AOP_Any, then the actual privileges
//!                         are returned and the caller may make subsequent
//!                         tests using Test().
//! @param     Env       -> Environmental information at the time of the
//!                         operation as supplied by the path CGI string.
//!                         This is optional and the pointer may be zero.
//!
//! @return    Permit: a non-zero value (access is permitted)
//!            Deny:   zero             (access is denied)
//------------------------------------------------------------------------------

virtual XrdAccPrivs Access(const XrdSecEntity    *Entity,
                           const char            *path,
                           const Access_Operation oper,
                                 XrdOucEnv       *Env=0) = 0;

//------------------------------------------------------------------------------
//! Route an audit message to the appropriate audit exit routine. See
//! XrdAccAudit.h for more information on how the default implementation works.
//! Currently, this method is not called by the ofs but should be used by the
//! implementation to record denials or grants, as warranted.
//!
//! @param     accok     -> True is access was grated; false otherwise.
//! @param     Entity    -> Authentication information
//! @param     path      -> The logical path which is the target of oper
//! @param     oper      -> The operation being attempted (see above)
//! @param     Env       -> Environmental information at the time of the
//!                         operation as supplied by the path CGI string.
//!                         This is optional and the pointer may be zero.
//!
//! @return    Success: !0 information recorded.
//!            Failure:  0 information could not be recorded.
//------------------------------------------------------------------------------

virtual int         Audit(const int              accok,
                          const XrdSecEntity    *Entity,
                          const char            *path,
                          const Access_Operation oper,
                                XrdOucEnv       *Env=0) = 0;

//------------------------------------------------------------------------------
//! Check whether the specified operation is permitted.
//!
//! @param     priv      -> the privileges as returned by Access().
//! @param     oper      -> The operation being attempted (see above)
//!
//! @return    Permit: a non-zero value (access is permitted)
//!            Deny:   zero             (access is denied)
//------------------------------------------------------------------------------

virtual int         Test(const XrdAccPrivs priv,
                         const Access_Operation oper) = 0;

//------------------------------------------------------------------------------
//! Constructor
//------------------------------------------------------------------------------

                          XrdAccAuthorize() {}

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual                  ~XrdAccAuthorize() {}
};
  
/******************************************************************************/
/*                 X r d A c c A u t h o r i z e O b j e c t                  */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Obtain an authorization object.
//!
//! XrdAccAuthorizeObject() is an extern "C" function that is called to obtain
//! an instance of the auth object that will be used for all subsequent
//! authorization decisions. It must be defined in the plug-in shared library.
//! All the following extern symbols must be defined at file level!
//!
//! @param lp   -> XrdSysLogger to be tied to an XrdSysError object for messages
//! @param cfn  -> The name of the configuration file
//! @param parm -> Parameters specified on the authlib directive. If none it 
//!                is zero.
//!
//! @return Success: A pointer to the authorization object.
//!         Failure: Null pointer which causes initialization to fail.
//------------------------------------------------------------------------------

/*! extern "C" XrdAccAuthorize *XrdAccAuthorizeObject(XrdSysLogger *lp,
                                                      const char   *cfn,
                                                      const char   *parm) {...}
*/
  
//------------------------------------------------------------------------------
//! Specify the compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. While not currently required, it is highly recommended to
//! avoid execution issues should the class definition change. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdAccAuthorizeObject,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.

    For the default statically linked authorization framework, the non-extern C
    XrdAccDefaultAuthorizeObject() is called instead so as to not conflict with
    that symbol in a shared library plug-in.
*/
#endif
