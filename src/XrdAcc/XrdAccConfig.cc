/******************************************************************************/
/*                                                                            */
/*                       X r d A c c C o n f i g . c c                        */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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
  
#include <unistd.h>
#include <cctype>
#include <fcntl.h>
#include <map>
#include <strings.h>
#include <cstdio>
#include <ctime>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdOuc/XrdOucLock.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucUri.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdAcc/XrdAccAccess.hh"
#include "XrdAcc/XrdAccAudit.hh"
#include "XrdAcc/XrdAccConfig.hh"
#include "XrdAcc/XrdAccGroups.hh"
#include "XrdAcc/XrdAccCapability.hh"

/******************************************************************************/
/*           G l o b a l   C o n f i g u r a t i o n   O b j e c t            */
/******************************************************************************/
  
// The following is the single configuration object. Other objects needing
// access to this object should simply declare an extern to it.
//
XrdAccConfig XrdAccConfiguration;

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_Xeq(x,m)   if (!strcmp(x,var)) return m(Config,Eroute);

#define TS_Str(x,m)   if (!strcmp(x,var)) {free(m); m = strdup(val); return 0;}

#define TS_Chr(x,m)   if (!strcmp(x,var)) {m = val[0]; return 0;}

#define TS_Bit(x,m,v) if (!strcmp(x,var)) {m |= v; return 0;}

#define ACC_PGO 0x0001

/******************************************************************************/
/*                    E x t e r n a l   F u n c t i o n s                     */
/******************************************************************************/
/******************************************************************************/
/*                  o o a c c _ C o n f i g _ R e f r e s h                   */
/******************************************************************************/

void *XrdAccConfig_Refresh( void *start_data )
{
   XrdSysError *Eroute = (XrdSysError *)start_data;

// Get the number of seconds between refreshes
//
   struct timespec naptime = {(time_t)XrdAccConfiguration.AuthRT, 0};

// Now loop until the bitter end
//
   while(1)
        {nanosleep(&naptime, 0); XrdAccConfiguration.ConfigDB(1, *Eroute);}
   return (void *)0;
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdAccConfig::XrdAccConfig()
{

// Initialize path value and databse pointer to nil
//
   dbpath        = strdup("/opt/xrd/etc/Authfile");
   Database      = 0;
   Authorization = 0;
   spChar        = 0;
   uriPath       = false;

// Establish other defaults
//
   ConfigDefaults();
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
int XrdAccConfig::Configure(XrdSysError &Eroute, const char *cfn) {
/*
  Function: Establish default values using a configuration file.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   char *var;
   int  retc, NoGo = 0, Cold = (Database == 0);
   pthread_t reftid;

// Print warm-up message
//
   Eroute.Say("++++++ Authorization system initialization started.");

// Process the configuration file and authorization database
//
   if (!(Authorization = new XrdAccAccess(&Eroute))
   ||   (NoGo = ConfigFile(Eroute, cfn))
   ||   (NoGo = ConfigDB(0, Eroute)))
       {if (Authorization) {delete Authorization, Authorization = 0;}
        NoGo = 1;
       }

// Start a refresh thread unless this was a refresh thread call
//
   if (Cold && !NoGo)
      {if ((retc=XrdSysThread::Run(&reftid,XrdAccConfig_Refresh,(void *)&Eroute)))
          Eroute.Emsg("ConfigDB",retc,"start refresh thread.");
      }

// All done
//
   var = (NoGo > 0 ? (char *)"failed." : (char *)"completed.");
   Eroute.Say("------ Authorization system initialization ", var);
   return (NoGo > 0);
}
  
/******************************************************************************/
/*                              C o n f i g D B                               */
/******************************************************************************/
  
int XrdAccConfig::ConfigDB(int Warm, XrdSysError &Eroute)
{
/*
  Function: Establish default values using a configuration file.

  Input:    None.

  Output:   0 upon success or !0 otherwise.
*/
   char buff[128];
   int  retc, anum = 0, NoGo = 0;
   struct XrdAccAccess_Tables tabs;
   XrdOucLock cdb_Lock(&Config_Context);

// Indicate type of start we are doing
//
   if (!Database) NoGo = !(Database = XrdAccAuthDBObject(&Eroute));
      else if (Warm && !Database->Changed(dbpath)) return 0;

// Try to open the authorization database
//
   if (!Database || !Database->Open(Eroute, dbpath)) return 1;

// Allocate new hash tables
//
   if (!(tabs.G_Hash = new XrdOucHash<XrdAccCapability>()) ||
       !(tabs.H_Hash = new XrdOucHash<XrdAccCapability>()) ||
       !(tabs.N_Hash = new XrdOucHash<XrdAccCapability>()) ||
       !(tabs.O_Hash = new XrdOucHash<XrdAccCapability>()) ||
       !(tabs.R_Hash = new XrdOucHash<XrdAccCapability>()) ||
       !(tabs.T_Hash = new XrdOucHash<XrdAccCapability>()) ||
       !(tabs.U_Hash = new XrdOucHash<XrdAccCapability>()) )
      {Eroute.Emsg("ConfigDB","Insufficient storage for id tables.");
       Database->Close(); return 1;
      }

// Now start processing records until eof.
//
   rulenum = 0;
   while((retc = ConfigDBrec(Eroute, tabs))) {NoGo |= retc < 0; anum++;}
   snprintf(buff, sizeof(buff), "%d auth entries processed in ", anum);
   Eroute.Say("Config ", buff, dbpath);

// All done, close the database and return if we failed
//
   if (!Database->Close() || NoGo) return 1;

// Do final setup for special identifiers (this will correctly order them)
//
   if (tabs.SYList) idChk(Eroute, tabs.SYList, tabs);

// Set the access control tables
//
   if (!tabs.G_Hash->Num()) {delete tabs.G_Hash; tabs.G_Hash=0;}
   if (!tabs.H_Hash->Num()) {delete tabs.H_Hash; tabs.H_Hash=0;}
   if (!tabs.N_Hash->Num()) {delete tabs.N_Hash; tabs.N_Hash=0;}
   if (!tabs.O_Hash->Num()) {delete tabs.O_Hash; tabs.O_Hash=0;}
   if (!tabs.R_Hash->Num()) {delete tabs.R_Hash; tabs.R_Hash=0;}
   if (!tabs.T_Hash->Num()) {delete tabs.T_Hash; tabs.T_Hash=0;}
   if (!tabs.U_Hash->Num()) {delete tabs.U_Hash; tabs.U_Hash=0;}
   Authorization->SwapTabs(tabs);

// All done
//
   return NoGo;
}

/******************************************************************************/
/*                     P r i v a t e   F u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/*        C o n f i g   F i l e   P r o c e s s i n g   M e t h o d s         */
/******************************************************************************/
  
int XrdAccConfig::ConfigFile(XrdSysError &Eroute, const char *ConfigFN) {
/*
  Function: Establish default values using a configuration file.

  Input:    None.

  Output:   1 - Processing failed.
            0 - Processing completed successfully.
           -1 = Security is to be disabled by request.
*/
   char *var;
   int  cfgFD, retc, NoGo = 0, recs = 0;
   XrdOucEnv myEnv;
   XrdOucStream Config(&Eroute, getenv("XRDINSTANCE"), &myEnv, "=====> ");

// If there is no config file, complain
//
   if( !ConfigFN || !*ConfigFN)
     {Eroute.Emsg("Config", "Authorization configuration file not specified.");
      return 1;
     } 

// Check if security is to be disabled
//
   if (!strcmp(ConfigFN, "none"))
      {Eroute.Emsg("Config", "Authorization system deactivated.");
       return -1;
      }

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {Eroute.Emsg("Config", errno, "open config file", ConfigFN);
       return 1;
      }
   Eroute.Emsg("Config","Authorization system using configuration in",ConfigFN);

// Now start reading records until eof.
//
   ConfigDefaults(); Config.Attach(cfgFD); Config.Tabs(0);
   static const char *cvec[] = { "*** acc plugin config:", 0 };
   Config.Capture(cvec);

   while((var = Config.GetMyFirstWord()))
        {if (!strncmp(var, "acc.", 2))
            {recs++;
             if (ConfigXeq(var+4, Config, Eroute)) {Config.Echo(); NoGo = 1;}
            }
        }

// Now check if any errors occurred during file i/o
//
   if ((retc = Config.LastError()))
      NoGo = Eroute.Emsg("Config",-retc,"read config file",ConfigFN);
      else {char buff[128];
            snprintf(buff, sizeof(buff), 
                     "%d authorization directives processed in ", recs);
            Eroute.Say("Config ", buff, ConfigFN);
           }
   Config.Close();

// Set external options, as needed
//
   if (options & ACC_PGO) GroupMaster.SetOptions(Primary_Only);

// All done
//
   return NoGo;
}

/******************************************************************************/
/*                        C o n f i g D e f a u l t s                         */
/******************************************************************************/

void XrdAccConfig::ConfigDefaults()
{
   AuthRT   = 60*60*12;
   options  = 0;
}
  
/******************************************************************************/
/*                             C o n f i g X e q                              */
/******************************************************************************/
  
int XrdAccConfig::ConfigXeq(char *var, XrdOucStream &Config, XrdSysError &Eroute)
{

// Fan out based on the variable
//
   TS_Xeq("audit",         xaud);
   TS_Xeq("authdb",        xdbp);
   TS_Xeq("authrefresh",   xart);
   TS_Xeq("encoding",      xenc);
   TS_Xeq("gidlifetime",   xglt);
   TS_Xeq("gidretran",     xgrt);
   TS_Xeq("nisdomain",     xnis);
   TS_Bit("pgo",           options, ACC_PGO);
   TS_Xeq("spacechar",     xspc);

// No match found, complain.
//
   Eroute.Emsg("Config", "unknown directive", var);
   Config.Echo();
   return 1;
}
  
/******************************************************************************/
/*                              s u b S p a c e                               */
/******************************************************************************/

void XrdAccConfig::subSpace(char *id)
{
   char *spc;

   while((spc = index(id, spChar)))
        {*spc = ' ';
         id = spc+1;
        }
}
  
/******************************************************************************/
/*                                  x a u d                                   */
/******************************************************************************/

/* Function: xaud

   Purpose:  To parse the directive: audit <options>

             options:

             deny     audit access denials.
             grant    audit access grants.
             none     audit is disabled.

   Output: 0 upon success or !0 upon failure.
*/

int XrdAccConfig::xaud(XrdOucStream &Config, XrdSysError &Eroute)
{
    static struct auditopts {const char *opname; int opval;} audopts[] =
       {
        {"deny",     (int)audit_deny},
        {"grant",    (int)audit_grant}
       };
    int i, audval = 0, numopts = sizeof(audopts)/sizeof(struct auditopts);
    char *val;

    val = Config.GetWord();
    if (!val || !val[0])
       {Eroute.Emsg("Config", "audit option not specified"); return 1;}
    while (val && val[0])
          {if (!strcmp(val, "none")) audval = (int)audit_none;
              else for (i = 0; i < numopts; i++)
                       {if (!strcmp(val, audopts[i].opname))
                           {audval |= audopts[i].opval; break;}
                        if (i >= numopts)
                           {Eroute.Emsg("Config","invalid audit option -",val);
                            return 1;
                           }
                       }
          val = Config.GetWord();
         }
    Authorization->Auditor->setAudit((XrdAccAudit_Options)audval);
    return 0;
}

/******************************************************************************/
/*                                  x a r t                                   */
/******************************************************************************/

/* Function: xart

   Purpose:  To parse the directive: authrefresh <seconds>

             <seconds> minimum number of seconds between aythdb refreshes.

   Output: 0 upon success or !0 upon failure.
*/

int XrdAccConfig::xart(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val;
    int reft;

      val = Config.GetWord();
      if (!val || !val[0])
         {Eroute.Emsg("Config","authrefresh value not specified");return 1;}
      if (XrdOuca2x::a2tm(Eroute,"authrefresh value",val,&reft,60))
         return 1;
      AuthRT = reft;
      return 0;
}

/******************************************************************************/
/*                                  x e n c                                   */
/******************************************************************************/

/* Function: xenc

   Purpose:  To parse the directive: encoding [space <char>] [pct path]

             <char>    the character that is to be considred as a space.
                       This only applies to identifiers.

   Output: 0 upon success or !0 upon failure.
*/

int XrdAccConfig::xenc(XrdOucStream &Config, XrdSysError &Eroute)
{
   char *val;

   if (!(val = Config.GetWord()) || *val == 0)
      {Eroute.Emsg("Config","encoding argument not specified"); return 1;}

do{     if (!strcmp(val, "pct"))
           {if (!(val = Config.GetWord()))
               {Eroute.Emsg("Config","pct argument not specified");
                return 1;
               }
            if (strcmp(val, "path"))
               {Eroute.Emsg("Config",val, "pct encoding not supported");
                return 1;
               }
            uriPath = true;
           }
   else if (!strcmp(val, "space"))
           {if (!(val = Config.GetWord()))
               {Eroute.Emsg("Config","space argument not specified");
                return 1;
               }
            if (strlen(val) != 1)
               {Eroute.Emsg("Config","invalid space argument -", val);
                return 1;
               }
            spChar = *val;
           }
  } while((val = Config.GetWord()) && *val);


   return 0;
}

/******************************************************************************/
/*                                  x d b p                                   */
/******************************************************************************/

/* Function: xdbp

   Purpose:  To parse the directive: authdb <path>

             <path>    is the path to the authorization database.

   Output: 0 upon success or !0 upon failure.
*/

int XrdAccConfig::xdbp(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val;

      val = Config.GetWord();
      if (!val || !val[0])
         {Eroute.Emsg("Config","authdb path not specified");return 1;}
      dbpath = strdup(val);
      return 0;
}
  
/******************************************************************************/
/*                                  x g l t                                   */
/******************************************************************************/

/* Function: xglt

   Purpose:  To parse the directive: gidlifetime <seconds>

             <seconds> maximum number of seconds to cache gid information.

   Output: 0 upon success or !0 upon failure.
*/

int XrdAccConfig::xglt(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val;
    int reft;

      val = Config.GetWord();
      if (!val || !val[0])
         {Eroute.Emsg("Config","gidlifetime value not specified");return 1;}
      if (XrdOuca2x::a2tm(Eroute,"gidlifetime value",val,&reft,60))
         return 1;
      GroupMaster.SetLifetime(reft);
      return 0;
}

/******************************************************************************/
/*                                  x g r t                                   */
/******************************************************************************/

/* Function: xgrt

   Purpose:  To parse the directive: gidretran <gidlist>

             <gidlist> is a list of blank separated gid's that must be
                       retranslated.

   Output: 0 upon success or !0 upon failure.
*/

int XrdAccConfig::xgrt(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val;
    int gid;

    val = Config.GetWord();
    if (!val || !val[0])
       {Eroute.Emsg("Config","gidretran value not specified"); return 1;}

    while (val && val[0])
      {if (XrdOuca2x::a2i(Eroute, "gid", val, &gid, 0)) return 1;
       if (GroupMaster.Retran((gid_t)gid) < 0)
          {Eroute.Emsg("Config", "to many gidretran gid's"); return 1;}
       val = Config.GetWord();
      }
    return 0;
}

/******************************************************************************/
/*                                  x n i s                                   */
/******************************************************************************/

/* Function: xnis

   Purpose:  To parse the directive: nisdomain <domain>

             <domain>  the NIS domain to be used for nis look-ups.

   Output: 0 upon success or !0 upon failure.
*/

int XrdAccConfig::xnis(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val;

      val = Config.GetWord();
      if (!val || !val[0])
         {Eroute.Emsg("Config","nisdomain value not specified");return 1;}
      GroupMaster.SetDomain(strdup(val));
      return 0;
}

/******************************************************************************/
/*                                  x s p c                                   */
/******************************************************************************/

/* Function: xspc (deprecated and undocumented, replaced by acc.encoding).

   Purpose:  To parse the directive: spacechar <char>

             <char>    the character that is to be considred as a space.

   Output: 0 upon success or !0 upon failure.
*/

int XrdAccConfig::xspc(XrdOucStream &Config, XrdSysError &Eroute)
{
    char *val;

      val = Config.GetWord();
      if (!val || !val[0])
         {Eroute.Emsg("Config","spacechar argument not specified");return 1;}
      if (strlen(val) != 1)
         {Eroute.Emsg("Config","invalid spacechar argument -", val);return 1;}
      spChar = *val;
      return 0;
}
  
/******************************************************************************/
/*                   D a t a b a s e   P r o c e s s i n g                    */
/******************************************************************************/
/******************************************************************************/
/*                           C o n f i g D B r e c                            */
/******************************************************************************/

int XrdAccConfig::ConfigDBrec(XrdSysError &Eroute,
                            struct XrdAccAccess_Tables &tabs)
{
// The following enum is here for convenience
//
    enum DB_RecType {  Group_ID = 'g',
                        Host_ID = 'h',
                      Netgrp_ID = 'n',
                         Org_ID = 'o',
                        Role_ID = 'r',
                         Set_ID = 's',
                    Template_ID = 't',
                        User_ID = 'u',
                         Xxx_ID = 'x',
                         Def_ID = '=',
                          No_ID = 0
                    };
    char *authid, rtype, *path, *privs;
    int alluser = 0, anyuser = 0, domname = 0, NoGo = 0;
    DB_RecType rectype;
    XrdAccAccess_ID *sp = 0;
    XrdOucHash<XrdAccCapability> *hp;
    XrdAccGroupType gtype = XrdAccNoGroup;
    XrdAccPrivCaps xprivs;
    XrdAccCapability mycap((char *)"", xprivs), *currcap, *lastcap = &mycap;
    XrdAccCapName *ncp;
    bool istmplt, isDup, xclsv = false;
  
   // Prepare the next record in the database
   //
   if (!(rtype = Database->getRec(&authid))) return 0;
   rectype = (DB_RecType)rtype;

   // Set up to handle the particular record
   //
   switch(rectype)
         {case    Group_ID: hp = tabs.G_Hash;
                            gtype=XrdAccUnixGroup;
                            if (spChar) subSpace(authid);
                            break;
          case     Host_ID: hp = tabs.H_Hash;
                            domname = (authid[0] == '.');
                            break;
          case      Set_ID: hp = 0;
                            break;
          case   Netgrp_ID: hp = tabs.N_Hash;
                            gtype=XrdAccNetGroup;
                            break;
          case      Org_ID: hp = tabs.O_Hash;
                            if (spChar) subSpace(authid);
                            break;
          case     Role_ID: hp = tabs.R_Hash;
                            if (spChar) subSpace(authid);
                            break;
          case Template_ID: hp = tabs.T_Hash;
                            break;
          case     User_ID: hp = tabs.U_Hash;
                            alluser = (authid[0] == '*' && !authid[1]);
                            anyuser = (authid[0] == '=' && !authid[1]);
                            if (!alluser && !anyuser && spChar) subSpace(authid);
                            break;
          case      Xxx_ID: hp = 0; xclsv = true;
                            break;
          case      Def_ID: return idDef(Eroute, tabs, authid);
                            break;
                   default: char badtype[2] = {rtype, '\0'};
                            Eroute.Emsg("ConfigXeq", "Invalid id type -",
                                        badtype);
                            return -1;
                            break;
         }

   // Check if this id is already defined in the table. For 's' rules the id
   // must have been previously defined.
   //
        if (domname)
           isDup = tabs.D_List && tabs.D_List->Find((const char *)authid);
   else if (alluser) isDup = tabs.Z_List != 0;
   else if (anyuser) isDup = tabs.X_List != 0;
   else if (hp)      isDup = hp->Find(authid) != 0;
   else    {if (!(sp = tabs.S_Hash->Find(authid)))
               {Eroute.Emsg("ConfigXeq", "Missing id definition -", authid);
                return -1;
               }
            isDup = sp->caps != 0;
            sp->rule = (xclsv ? rulenum++ : -1);
           }

   if (isDup)
      {Eroute.Emsg("ConfigXeq", "duplicate rule for id -", authid);
       return -1;
      }

   // Add this ID to the appropriate group object constants table
   //
   if (gtype) GroupMaster.AddName(gtype, (const char *)authid);

   // Now start getting <path> <priv> pairs until we hit the logical end
   //
   while(1) {NoGo = 0;
             if (!Database->getPP(&path, &privs, istmplt)) break;
             if (!path) continue;      // Skip pathless entries
             NoGo = 1;
             if (istmplt)
                {if ((currcap = tabs.T_Hash->Find(path)))
                    currcap = new XrdAccCapability(currcap);
                    else {Eroute.Emsg("ConfigXeq", "Missing template -", path);
                          break;
                         }
                } else {
                  if (!privs)
                     {Eroute.Emsg("ConfigXeq", "Missing privs for path", path);
                      break;
                     }
                  if (!PrivsConvert(privs, xprivs))
                     {Eroute.Emsg("ConfigXeq", "Invalid privs -", privs);
                      break;
                     }
                  if (uriPath)
                     {int plen = strlen(path);
                      char *decp = (char *)alloca(plen+1);
                      XrdOucUri::Decode(path, plen, decp);
                      currcap = new XrdAccCapability(decp, xprivs);
                     } else currcap = new XrdAccCapability(path, xprivs);
                }
             lastcap->Add(currcap);
             lastcap = currcap;
            }

   // Check if all went well
   //
   if (NoGo) return -1;

   // Check if any capabilities were specified
   //
   if (!mycap.Next())
      {Eroute.Emsg("ConfigXeq", "no capabilities specified for", authid);
       return -1;
      }

   // Insert the capability into the appropriate table/list
   //
        if (sp) sp->caps = mycap.Next();
   else if (domname)
           {if (!(ncp = new XrdAccCapName(authid, mycap.Next())))
               {Eroute.Emsg("ConfigXeq","unable to add id",authid); return -1;}
            if (tabs.E_List) tabs.E_List->Add(ncp);
               else tabs.D_List = ncp;
            tabs.E_List = ncp;
           }
   else if (anyuser) tabs.X_List = mycap.Next();
   else if (alluser) tabs.Z_List = mycap.Next();
   else    hp->Add(authid, mycap.Next());

   // All done
   //
   mycap.Add((XrdAccCapability *)0);
   return 1;
}
  
/******************************************************************************/
/* Private:                        i d C h k                                  */
/******************************************************************************/

void  XrdAccConfig::idChk(XrdSysError        &Eroute,
                          XrdAccAccess_ID    *idList,
                          XrdAccAccess_Tables &tabs)
{
   std::map<int, XrdAccAccess_ID *> idMap;
   XrdAccAccess_ID *idPN, *xList = 0, *yList = 0;

// Run through the list to make everything was used. We also, sort these items
// in the order the associated rule appeared.
//
   while(idList)
        {idPN = idList->next;
         if (idList->caps == 0)
            Eroute.Say("Config ","Warning, unused identifier definition '",
                                 idList->name, "'.");
            else if (idList->rule >= 0) idMap[idList->rule] = idList;
                    else {idList->next = yList; yList = idList;}
         idList = idPN;
        }

// Place 'x' rules in the order they were used. The ;s; rules are in the
// order the id's were defined which is OK because the are inclusive.
//
   std::map<int,XrdAccAccess_ID *>::reverse_iterator rit;
   for (rit = idMap.rbegin(); rit != idMap.rend(); ++rit)
       {rit->second->next = xList;
        xList = rit->second;
       }

// Set the new lists in the supplied tabs structure
//
   tabs.SXList = xList;
   tabs.SYList = yList;
}
  
/******************************************************************************/
/* Private:                        i d D e f                                  */
/******************************************************************************/

int XrdAccConfig::idDef(XrdSysError &Eroute,
                        struct XrdAccAccess_Tables &tabs,
                        const char *idName)
{
   XrdAccAccess_ID *xID, theID(idName);
   char *idname, buff[80], idType;
   bool haveID = false, idDup = false;

// Now start getting <idtype> <idname> pairs until we hit the logical end
//
   while(!idDup)
        {if (!(idType = Database->getID(&idname))) break;
         haveID = true;
         switch(idType)
               {case 'g': if (spChar) subSpace(idname);
                          if (theID.grp)  idDup = true;
                             else{theID.grp   = strdup(idname);
                                  theID.glen  = strlen(idname);
                                 }
                          break;
                case 'h': if (theID.host) idDup = true;
                             else{theID.host = strdup(idname);
                                  theID.hlen = strlen(idname);
                                 }
                          break;
                case 'o': if (theID.org)  idDup = true;
                             else {if (spChar) subSpace(idname);
                                   theID.org   = strdup(idname);
                                  }
                          break;
                case 'r': if (theID.role) idDup = true;
                             else {if (spChar) subSpace(idname);
                                   theID.role  = strdup(idname);
                                  }
                          break;
                case 'u': if (theID.user) idDup = true;
                             else {if (spChar) subSpace(idname);
                                   theID.user  = strdup(idname);
                                  }
                          break;
                default:  snprintf(buff, sizeof(buff), "'%c: %s' for",
                                                       idType, idname);
                          Eroute.Emsg("ConfigXeq", "Invalid id selector -",
                                                   buff, theID.name);
                          return -1;
                          break;
               }
         if (idDup)
            {snprintf(buff, sizeof(buff),
                      "id selector '%c' specified twice for", idType);
             Eroute.Emsg("ConfigXeq", buff, theID.name);
             return -1;
            }
        }

// Make sure some kind of id was specified
//
   if (!haveID)
      {Eroute.Emsg("ConfigXeq", "No id selectors specified for", theID.name);
       return -1;
      }

// Make sure this name has not been specified before
//
   if (!tabs.S_Hash) tabs.S_Hash = new XrdOucHash<XrdAccAccess_ID>;
      else if (tabs.S_Hash->Find(theID.name))
              {Eroute.Emsg("ConfigXeq","duplicate id definition -",theID.name);
               return -1;
              }

// Export the id definition and add it to the S_Hash
//
   xID = theID.Export();
   tabs.S_Hash->Add(xID->name, xID);

// Place this FIFO in SYList (they reordered later based on rule usage)
//
   xID->next = tabs.SYList;
   tabs.SYList = xID;

// All done
//
   return 1;
}
  
/******************************************************************************/
/*                          P r i v s C o n v e r t                           */
/******************************************************************************/
  
int XrdAccConfig::PrivsConvert(char *privs, XrdAccPrivCaps &ctab)
{
    int i = 0;
    XrdAccPrivs ptab[] = {XrdAccPriv_None, XrdAccPriv_None}; // Speed conversion here

    // Convert the privs
    //
    while(*privs)
         {switch((XrdAccPrivSpec)(*privs))
                {case    All_Priv:
                            ptab[i] = (XrdAccPrivs)(ptab[i]|XrdAccPriv_All);
                            break;
                 case Delete_Priv:
                            ptab[i] = (XrdAccPrivs)(ptab[i]|XrdAccPriv_Delete);
                            break;
                 case Insert_Priv: 
                            ptab[i] = (XrdAccPrivs)(ptab[i]|XrdAccPriv_Insert);
                            break;
                 case   Lock_Priv: 
                            ptab[i] = (XrdAccPrivs)(ptab[i]|XrdAccPriv_Lock);
                            break;
                 case Lookup_Priv: 
                            ptab[i] = (XrdAccPrivs)(ptab[i]|XrdAccPriv_Lookup);
                            break;
                 case Rename_Priv: 
                            ptab[i] = (XrdAccPrivs)(ptab[i]|XrdAccPriv_Rename);
                            break;
                 case   Read_Priv: 
                            ptab[i] = (XrdAccPrivs)(ptab[i]|XrdAccPriv_Read);
                            break;
                 case  Write_Priv: 
                            ptab[i] = (XrdAccPrivs)(ptab[i]|XrdAccPriv_Write);
                            break;
                 case    Neg_Priv: if (i) return 0; i++;   break;
                 default:                 return 0;
                }
           privs++;
          }
     ctab.pprivs = ptab[0]; ctab.nprivs = ptab[1];
     return 1;
}
