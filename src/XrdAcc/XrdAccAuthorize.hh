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

//         $Id$

#include "XrdAcc/XrdAccPrivs.hh"

/******************************************************************************/
/*                        A c c e s s _ I D _ T y p e                         */
/******************************************************************************/
  
// The following are supported id types for access() checking
//
enum Access_ID_Type   {AID_Group,
                       AID_Host,
                       AID_Netgroup,
                       AID_Set,
                       AID_Template,
                       AID_User
                      };

/******************************************************************************/
/*                      A c c e s s _ O p e r a t i o n                       */
/******************************************************************************/
  
// The following are supported operations
//
enum Access_Operation  {AOP_Any      = 0,
                        AOP_Chmod    = 1,
                        AOP_Chown    = 2,
                        AOP_Create   = 3,
                        AOP_Delete   = 4,
                        AOP_Insert   = 5,
                        AOP_Lock     = 6,
                        AOP_Mkdir    = 7,
                        AOP_Read     = 8,
                        AOP_Readdir  = 9,
                        AOP_Rename   = 10,
                        AOP_Stat     = 11,
                        AOP_Update   = 12,
                        AOP_LastOp   = 12   // For limits testing
                       };

/******************************************************************************/
/*                 o o a c c _ A u t h o r i z e   C l a s s                  */
/******************************************************************************/
  
class XrdAccAuthorize
{
public:

// Access() indicates whether or not the user/host is permitted access to the
// path for the specified operation. Privileges are determined by combining
// user, host, user group, and user/host netgroup privileges. If the operation
// is AOP_Any, then the actual privileges are returned and the caller may make 
// subsequent tests using Test(). Otherwise, a non-zero value is returned is
// access is permitted or a zero value is returned is access is to be denied.
//
virtual XrdAccPrivs Access(const char *atype,
                           const char *id, const char *host,
                           const char *path,
                           const Access_Operation oper) = 0;

// Access() indicates whether or not access allowed for the given operation for
// a particular id type and path. If AOP_And is specified as the operation,
// then the actual privileges are returned and the caller may use Test() to
// test for permitted operations in the future. Otherwise, is access is allowed
// a non-zero value is returned. If access is denied, XrdAccPriv_None (actual 0)
// is returned. For login acccess, use the previous form of Access().
//
virtual XrdAccPrivs Access(const char *id,
                           const Access_ID_Type idtype,
                           const char *path,
                           const Access_Operation oper) = 0;

// Audit() routes an audit message to the appropriate audit exit routine. See
// XrdAccAudit.h for more information.
//
virtual int         Audit(const int accok,
                          const char *atype,
                          const char *id,
                          const char *host,
                          const char *path,
                          const Access_Operation oper) = 0;

// Enable() records the fact that a user/host pair is enabled for an object ID.
//
virtual void        Enable(const    char *user,
                           const    char *host,
                           unsigned long  oid) = 0;

// Disable() undoes the Enable()for a user/host/object ID triple.
//
virtual void       Disable(const    char *user,
                           const    char *host,
                           unsigned long  oid) = 0;

// isEnabled() check whether a user/host/object ID triple is enabled.
//
virtual int      isEnabled(const    char *user,
                           const    char *host,
                           unsigned long  oid) = 0;

// Test() check whether the specified operation is permitted. If permitted it
// returns a non-zero. Otherwise, zero is returned.
//
virtual int         Test(const XrdAccPrivs priv,
                         const Access_Operation oper) = 0;

                          XrdAccAuthorize() {}

virtual                  ~XrdAccAuthorize() {}
};
  
/******************************************************************************/
/*                   o o a c c _ A c c e s s _ O b j e c t                    */
/******************************************************************************/

class XrdOucLogger;
  
extern XrdAccAuthorize *XrdAccAuthorizeObject(XrdOucLogger *lp,
                                              const char   *cfn);

#endif
