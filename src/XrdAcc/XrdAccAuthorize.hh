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
  
class XrdSecEntity;

class XrdAccAuthorize
{
public:

// Access() indicates whether or not the user/host is permitted access to the
// path for the specified operation. Privileges are determined by combining
// user, host, user group, and user/host netgroup privileges. If the operation
// is AOP_Any, then the actual privileges are returned and the caller may make 
// subsequent tests using Test(). Otherwise, a non-zero value is returned if
// access is permitted or a zero value is returned is access is to be denied.
//
virtual XrdAccPrivs Access(const XrdSecEntity    *Entity,
                           const char            *path,
                           const Access_Operation oper) = 0;

// Audit() routes an audit message to the appropriate audit exit routine. See
// XrdAccAudit.h for more information.
//
virtual int         Audit(const int              accok,
                          const XrdSecEntity    *Entity,
                          const char            *path,
                          const Access_Operation oper) = 0;

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
