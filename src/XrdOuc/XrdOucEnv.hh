#ifndef __OUC_ENV__
#define __OUC_ENV__
/******************************************************************************/
/*                                                                            */
/*                          X r d O u c E n v . h h                           */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdlib.h>
#ifndef WIN32
#include <strings.h>
#endif
#include "XrdOuc/XrdOucHash.hh"

class XrdSecEntity;

class XrdOucEnv
{
public:

// Env() returns the full environment string and length passed to the
//       constructor.
//
inline char *Env(int &envlen) {envlen = global_len; return global_env;}

// Export() sets an external environmental variable to the desired value
//          using dynamically allocated fixed storage.
//
static int   Export(const char *Var, const char *Val);
static int   Export(const char *Var, int         Val);

// Import() gets a variable from the extended environment and stores
//          it in this object
static bool  Import( const char *var, char *&val );
static bool  Import( const char *var, long  &val );

// Get() returns the address of the string associated with the variable
//       name. If no association exists, zero is returned.
//
       char *Get(const char *varname) {return env_Hash.Find(varname);}

// GetInt() returns a long integer value. If the variable varname is not found
//           in the hash table, return -999999999.       
//
       long  GetInt(const char *varname);

// GetPtr() returns a pointer as a (void *) value. If the varname is not found
//          a nil pointer is returned (i.e. 0).
//
       void *GetPtr(const char *varname);

// Put() associates a string value with the a variable name. If one already
//       exists, it is replaced. The passed value and variable strings are
//       duplicated (value here, variable by env_Hash).
//
       void  Put(const char *varname, const char *value)
                {env_Hash.Rep((char *)varname, strdup(value), 0, Hash_dofree);}

// PutInt() puts a long integer value into the hash. Internally, the value gets
//          converted into a char*
//
       void  PutInt(const char *varname, long value);

// PutPtr() puts a pointer value into the hash. The pointer is accepted as a
//          (void *) value. By convention, the variable name should end with
//          an asterisk and typically corresponds to it's class name.
//
       void PutPtr(const char *varname, void *value);

// Delimit() search for the first occurrence of comma (',') in value and
//           replaces it with a null byte. It then returns the address of the
//           remaining string. If no comma was found, it returns zero.
//
       char *Delimit(char *value);

// secEnv() returns the security environment; which may be a null pointer.
//
inline const XrdSecEntity *secEnv() const {return secEntity;}

// Use the constructor to define the initial variable settings. The passed
// string is duplicated and the copy can be retrieved using Env().
//
       XrdOucEnv(const char *vardata=0, int vardlen=0, 
                 const XrdSecEntity *secent=0);

      ~XrdOucEnv() {if (global_env) free((void *)global_env);}

private:

XrdOucHash<char> env_Hash;
const XrdSecEntity *secEntity;
char *global_env;
int   global_len;
};
#endif
