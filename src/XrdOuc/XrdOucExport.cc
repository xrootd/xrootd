/******************************************************************************/
/*                                                                            */
/*                       X r d O u c E x p o r t . c c                        */
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

#include "XrdOuc/XrdOucExport.hh"
#include "XrdOuc/XrdOucPList.hh"
#include "XrdSys/XrdSysPlatform.hh"
  
/******************************************************************************/
/*                             P a r s e D e f s                              */
/******************************************************************************/
  
/* Function: ParseDefs

   Purpose:  Parse: defaults [[no]cache] [[no]check] [[no]dread]

                             [[no]filter] [forcero]

                             [inplace] [local] [global] [globalro]
                              
                             [[no]mig] [[not]migratable] [[no]mkeep]

                             [[no]mlock] [[no]mmap] [outplace] [readonly]

                             [[no]stage] [stage+] [[no]rcreate]

                             [[not]writable] [[no]xattrs]

   Notes: See the oss configuration manual for the meaning of each option.

   Output: Returns updated flags passed as input
*/

unsigned long long XrdOucExport::ParseDefs(XrdOucStream      &Config,
                                           XrdSysError       &Eroute,
                                           unsigned long long Flags)
{
    static struct rpathopts 
           {const    char     *opname;
            unsigned long long oprem; 
            unsigned long long opadd; 
            unsigned long long opset;} rpopts[] =
       {
        {"r/o",           0,              XRDEXP_READONLY,XRDEXP_ROW_X},
        {"readonly",      0,              XRDEXP_READONLY,XRDEXP_ROW_X},
        {"forcero",       0,              XRDEXP_FORCERO, XRDEXP_ROW_X},
        {"notwritable",   0,              XRDEXP_READONLY,XRDEXP_ROW_X},
        {"writable",      XRDEXP_NOTRW,   0,              XRDEXP_ROW_X},
        {"r/w",           XRDEXP_NOTRW,   0,              XRDEXP_ROW_X},
        {"inplace",       0,              XRDEXP_INPLACE, XRDEXP_INPLACE_X},
        {"outplace",      XRDEXP_INPLACE, 0,              XRDEXP_INPLACE_X},
//      {"nocache",       XRDEXP_PFCACHE, 0,              XRDEXP_PFCACHE_X},
        {"cache",         0,              XRDEXP_PFCACHE, XRDEXP_PFCACHE_X},
        {"nomig",         XRDEXP_MIG,     0,              XRDEXP_MIG_X},
        {"mig",           0,              XRDEXP_MIG,     XRDEXP_MIG_X},
        {"notmigratable", XRDEXP_MIG,     0,              XRDEXP_MIG_X},
        {"migratable",    0,              XRDEXP_MIG,     XRDEXP_MIG_X},
        {"nomkeep",       XRDEXP_MKEEP,   0,              XRDEXP_MKEEP_X},
        {"mkeep",         0,              XRDEXP_MKEEP,   XRDEXP_MKEEP_X},
        {"nomlock",       XRDEXP_MLOK,    0,              XRDEXP_MLOK_X},
        {"mlock",         0,              XRDEXP_MLOK,    XRDEXP_MLOK_X},
        {"nommap",        XRDEXP_MMAP,    0,              XRDEXP_MMAP_X},
        {"mmap",          0,              XRDEXP_MMAP,    XRDEXP_MMAP_X},
        {"mwfiles",       0,              XRDEXP_MWMODE,  XRDEXP_MWMODE_X},
        {"nopurge",       XRDEXP_PURGE,   0,              XRDEXP_PURGE_X},
        {"purge",         0,              XRDEXP_PURGE,   XRDEXP_PURGE_X},
        {"nostage",       XRDEXP_STAGE,   0,              XRDEXP_STAGE_X},
        {"stage",         0,              XRDEXP_STAGE,   XRDEXP_STAGE_X},
        {"stage+",        0,              XRDEXP_STAGEMM, XRDEXP_STAGE_X},
        {"dread",         XRDEXP_NODREAD, 0,              XRDEXP_DREAD_X},
        {"nodread",       0,              XRDEXP_NODREAD, XRDEXP_DREAD_X},
        {"check",         XRDEXP_NOCHECK, 0,              XRDEXP_CHECK_X},
        {"nocheck",       0,              XRDEXP_NOCHECK, XRDEXP_CHECK_X},
        {"rcreate",       0,              XRDEXP_RCREATE, XRDEXP_RCREATE_X},
        {"norcreate",     XRDEXP_RCREATE, 0,              XRDEXP_RCREATE_X},
        {"local",         XRDEXP_GLBLRO,  XRDEXP_LOCAL,   XRDEXP_LOCAL_X},
        {"global",        XRDEXP_LOCAL,   0,              XRDEXP_LOCAL_X},
        {"globalro",      XRDEXP_LOCAL,   XRDEXP_GLBLRO,  XRDEXP_GLBLRO_X},
        {"lock",          XRDEXP_NOLK,    0,              XRDEXP_NOLK_X},
        {"nolock",        0,              XRDEXP_NOLK,    XRDEXP_NOLK_X},
        {"xattrs",        XRDEXP_NOXATTR, 0,              XRDEXP_NOXATTR_X},
        {"noxattrs",      0,              XRDEXP_NOXATTR, XRDEXP_NOXATTR_X},
        {"noficl",        0,              XRDEXP_NOFICL,  XRDEXP_NOFICL_X},
        {"ficl",          XRDEXP_NOFICL,  0,              XRDEXP_NOFICL_X}
       };
    int i, numopts = sizeof(rpopts)/sizeof(struct rpathopts);
    char *val;

// Process options
//
   val = Config.GetWord();
   while (val)
         {for (i = 0; i < numopts; i++)
              {if (!strcmp(val, rpopts[i].opname))
                  {Flags = (Flags & ~rpopts[i].oprem)
                                  |  rpopts[i].opadd
                                  |  rpopts[i].opset;
                   break;
                  }
              }
         if (i >= numopts) 
            Eroute.Emsg("Export", "warning, invalid path option", val);
         val = Config.GetWord();
         }

// All done
//
   return Flags;
}

/******************************************************************************/
/*                             P a r s e P a t h                              */
/******************************************************************************/

/* Function: ParsePath

   Purpose:  To parse the directive args: <path> [<options>]

             <path>    the path prefix that applies
             <options> a blank separated list of options:
                       [no]cache    - is [not] file caching
                       [no]check    - [don't] check if new file exists in MSS
                       [no]dread    - [don't] read actual directory contents
                           forcero  - force r/w opens to r/o opens
                           inplace  - do not use extended cache for creation
                          outplace  -        use extended cache for creation
                           local    - do not export via olbd
                           global   - do     export via olbd
                           globalro - do     export via olbd as r/o path
                       [no]mig      - this is [not] a migratable name space
                       [no]mkeep    - this is [not] a memory keepable name space
                       [no]mlock    - this is [not] a memory lockable name space
                       [no]mmap     - this is [not] a memory mappable name space
                       [no]rcreate  - [don't] create file in MSS as well
                           r/o      - do not allow modifications (read/only)
                           r/w      - path is writable/modifiable
                       [no]stage    - [don't] stage in files.

   Output: XrdOucPList object upon success or 0 upon failure.
*/
  
XrdOucPList *XrdOucExport::ParsePath(XrdOucStream &Config, XrdSysError &Eroute,
                                     XrdOucPListAnchor &Export,
                                     unsigned long long Defopts)
{
    XrdOucPList *plp;
    char *path, pbuff[1024];
    unsigned long long rpval;

// Get the path
//
   path = Config.GetWord();
   if (!path || !path[0])
      {Eroute.Emsg("Export", "path not specified"); return 0;}
   strlcpy(pbuff, path, sizeof(pbuff));

// Handle object ID specification
//
   if (*pbuff == '*') pbuff[1] = 0;

// Process path options and apply defaults to any unspecified otions
//
   rpval = ParseDefs(Config, Eroute, 0);
   rpval = rpval | (Defopts & (~(rpval >> XRDEXP_MASKSHIFT)))
                 | (Defopts & (~(rpval & ~XRDEXP_SETTINGS)));

// Make sure that we have no conflicting options
//
   if ((rpval & XRDEXP_MEMAP) && !(rpval & XRDEXP_NOTRW))
      {Eroute.Emsg("config", "warning, file memory mapping forced path", path,
                             "to be readonly");
       rpval |= XRDEXP_FORCERO;
      }

// noxattr conflicts with mig or purge
//
   if ((rpval & XRDEXP_NOXATTR) && (rpval & XRDEXP_MIGPRG))
      {Eroute.Emsg("config", "noxattrs attribute is incompatible with "
                   "mig and purge attributes.");
       return 0;
      }


// Update the export list. If this path is being modified, turn off all bits
// in the old path specified in the new path and then set the new bits.
//
   if ((plp = Export.Match(pbuff)))
      {unsigned long long Opts = rpval >> XRDEXP_MASKSHIFT;
       Opts = (plp->Flag() & ~Opts) | rpval;
       plp->Set(Opts);
      } else {
       plp = new XrdOucPList(pbuff,rpval);
       Export.Insert(plp);
      }

// Return the path specification
//
   return plp;
}
