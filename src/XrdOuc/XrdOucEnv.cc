/******************************************************************************/
/*                                                                            */
/*                          X r d O u c E n v . c c                           */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//         $Id$

const char *XrdOucEnvCVSID = "$Id$";

#include "string.h"

#include "Experiment/Experiment.hh"
#include "XrdOuc/XrdOucEnv.hh"
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOucEnv::XrdOucEnv(const char *vardata, int varlen)
{
   char *vdp, varsave, *varname, *varvalu;

   if (!vardata) {global_env = 0; return;}

// Copy the the global information (don't rely on its being correct)
//
   if (!varlen) varlen = strlen(vardata);
   global_env = (char *)malloc(varlen+1);
   memcpy((void *)global_env, (const void *)vardata, (size_t)varlen);
   global_env[varlen+1] = '\0';
   vdp = global_env;

// scan through the string looking for '&'
//
   if (vdp) while(*vdp)
        {if (*vdp != '&') {vdp++; continue;}    // &....
         varname = ++vdp;

         while(*vdp && *vdp != '=') vdp++;  // &....=
         if (!*vdp) break;
         *vdp = '\0';
         varvalu = ++vdp;

         while(*vdp && *vdp != '&') vdp++;  // &....=....&
         varsave = *vdp; *vdp = '\0';

         if (*varname && *varvalu)
            env_Hash.Rep(strdup(varname), strdup(varvalu), 0, Hash_dofree);

         *vdp = varsave; *(--varvalu) = '=';
        }
   return;
}

/******************************************************************************/
/*                               D e l i m i t                                */
/******************************************************************************/

char *XrdOucEnv::Delimit(char *value)
{
     while(*value) if (*value == ',') {*value = '\0'; return ++value;}
                      else value++;
     return (char *)0;
}
