#ifndef __OUC_ENV__
#define __OUC_ENV__
/******************************************************************************/
/*                                                                            */
/*                          X r d O u c E n v . h h                           */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//         $Id$

#include <stdlib.h>
#include "XrdOuc/XrdOucHash.hh"

class XrdOucEnv
{
public:
inline char *Env(int &envlen) {envlen = global_len; return global_env;}

       char *Get(char *varname) {return env_Hash.Find(varname);}

       void  Put(char *varname, char *value) 
                {env_Hash.Rep(varname, value, 0, Hash_dofree);}

       char *Delimit(char *value);

       XrdOucEnv(const char *vardata=0, int vardlen=0);
      ~XrdOucEnv() {if (global_env) free((void *)global_env);}

private:

XrdOucHash<char> env_Hash;
char *global_env;
int   global_len;
};
#endif
