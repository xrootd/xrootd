/******************************************************************************/
/*                                                                            */
/*                     X r d S e c s s s A d m i n . c c                      */
/*                                                                            */
/* (c) 2008 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//       $Id$

const char *XrdSecsssAdminCVSID = "$Id$";
  
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysTimer.hh"

#include "XrdSecsss/XrdSecsssKT.hh"
  
/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/
  
#define eMsg(x) cerr <<XrdpgmName <<": " <<x << endl

struct XrdsecsssAdmin_Opts
      {XrdSecsssKT *kTab;
       const char  *Action;
       const char  *KeyName;
       const char  *KeyFile;
       time_t       Expdt;
       int          Debug;
       int          Keep;
       int          KeyLen;
       int          KeyNum;

       XrdsecsssAdmin_Opts() : kTab(0), Action(0), KeyName(0), KeyFile(0),
                               Expdt(0), Debug(0), Keep(3), KeyLen(32),
                               KeyNum(-1) {}
      ~XrdsecsssAdmin_Opts() {}
};

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
static const char *XrdpgmName;

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char **argv)
{
   extern char  *optarg;
   extern int    optind, opterr;
   extern int    XrdSecsssAdmin_addKey(XrdsecsssAdmin_Opts &Opt);
   extern int    XrdSecsssAdmin_delKey(XrdsecsssAdmin_Opts &Opt);
   extern int    XrdSecsssAdmin_insKey(XrdsecsssAdmin_Opts &Opt);
   extern int    XrdSecsssAdmin_lstKey(XrdsecsssAdmin_Opts &Opt);
   extern time_t getXDate(const char *cDate);
   extern void   Usage(int rc, const char *opn=0, const char *opv=0);

   XrdsecsssAdmin_Opts Opt;
   enum What2Do {doAdd, doInst, doDel, doList};
   char c, *sp;
   int rc;
   What2Do doIt;

// Get the name of our program
//
   XrdpgmName = ((sp = rindex(argv[0], '/')) ?  sp+1 : argv[0]);

// Process the options
//
   opterr = 0;
   if (argc > 1 && '-' == *argv[1]) 
      while ((c = getopt(argc,argv,"df:k:l:x:")) && ((unsigned char)c != 0xff))
     { switch(c)
       {
       case 'd': Opt.Debug = 1;
                 break;
       case 'f': Opt.KeyFile = optarg;
                 break;
       case 'k': if ((Opt.Keep = atoi(optarg)) <= 0) Usage(1, "-k", optarg);
                 break;
       case 'l': if ((Opt.KeyLen = atoi(optarg)) <= 0 
                 ||   Opt.KeyLen > XrdSecsssKT::ktEnt::maxKLen)
                    Usage(1, "-l", optarg);
                 break;
       case 'x': if ((Opt.Expdt = getXDate(optarg)) < 0
                 ||   Opt.Expdt < (time(0)+60)) Usage(1, "-x", optarg);
                 break;
       default:  eMsg("Invalid option '-" <<argv[optind-1] <<"'");
                 Usage(1);
       }
     }

// Make sure and opreration has been specified
//
   if (optind >= argc) {eMsg("Action not specified."); Usage(1);}

// Verify the action
//
        if (!strcmp(argv[optind], "add"))      doIt = doAdd;
   else if (!strcmp(argv[optind], "install"))  doIt = doInst;
   else if (!strcmp(argv[optind], "del"))      doIt = doDel;
   else if (!strcmp(argv[optind], "list"))     doIt = doList;
   else Usage(1, "parameter", argv[optind]);
   Opt.Action = argv[optind++];

// Grab any arguments
//
   if (optind < argc) Opt.KeyName = argv[optind++];
      else if (doIt == doDel)
              {eMsg(Opt.Action <<" keyname not specified."); Usage(1);}

   if (doIt == doDel)
      {if (optind < argc)
          {eMsg(Opt.Action <<" key number not specified."); Usage(1);}
       if (!strcmp("all", argv[optind])) Opt.KeyNum = -1;
          else if ((Opt.KeyNum = atoi(argv[optind])) <= 0)
                  {eMsg("Invalid 'del " <<Opt.KeyName <<"' key number - " <<argv[optind]);
                   Usage(1);
                  }
      }

// Make sure keyname is not too long
//
   if (Opt.KeyName && (int)strlen(Opt.KeyName) >= XrdSecsssKT::ktEnt::NameSZ)
      {eMsg("Key name must be less than " <<XrdSecsssKT::ktEnt::NameSZ
            << " characters.");
       exit(4);
      }

// Provide default keyfile if none specified
//
   if (!Opt.KeyFile) Opt.KeyFile = XrdSecsssKT::genFN();

// Perform the action
//
   switch(doIt)
         {case doAdd:  rc = XrdSecsssAdmin_addKey(Opt); break;
          case doDel:  rc = XrdSecsssAdmin_delKey(Opt); break;
          case doInst: rc = XrdSecsssAdmin_insKey(Opt); break;
          case doList: rc = XrdSecsssAdmin_lstKey(Opt); break;
          default:     rc = 16; eMsg("Internal processing error!");
         }

// All done
//
   if (Opt.kTab) delete Opt.kTab;
   exit(rc);
}

/******************************************************************************/
/*                              g e t X D a t e                               */
/******************************************************************************/

time_t getXDate(const char *cDate)
{
   struct tm myTM;
   char *eP;
   long  theVal;

// if no slashes then this is number of days
//
   if (!index(cDate, '/'))
      {theVal = strtol(cDate, &eP, 10);
       if (errno || *eP) return -1;
       if (theVal) theVal = XrdSysTimer::Midnight() + (86400*theVal);
       return static_cast<time_t>(theVal);
      }

// Do a date conversion
//
   eP = strptime(cDate, "%D", &myTM);
   if (*eP) return -1;
   return mktime(&myTM);
}
  
/******************************************************************************/
/*                                  i s N o                                   */
/******************************************************************************/
  
int isNo(int dflt, const char *Msg1, const char *Msg2, const char *Msg3)
{
   char Answer[8];

   cerr <<XrdpgmName <<": " <<Msg1 <<Msg2 <<Msg3;
   cin.getline(Answer, sizeof(Answer));
   if (!*Answer) return dflt;

   if (!strcmp("y",Answer) || !strcmp("ye",Answer) || !strcmp("yes",Answer))
      return 1;
   return 0;
}

/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/
  
void Usage(int rc, const char *opn, const char *opv)
{
// Check if we need to issue a message here
//
   if (opn)
      {if (opv) eMsg("Invalid " <<opn <<" argument - " <<opv);
          else  eMsg(opn <<" argument not specified.");
      }

cerr <<"\nUsage:   " <<XrdpgmName <<" [options] action\n";
cerr <<"\nOptions: [-d] [-f fn] [-k num] [-l len] [-x {days | mm/dd/yy}]" <<endl;
cerr <<"\nActions: add [name] | del name {num|all}] | install [name] | list [name]" <<endl;
exit(rc);
}

/******************************************************************************/
/*                 X r d S e c s s s A d m i n _ a d d K e y                  */
/******************************************************************************/
  
int  XrdSecsssAdmin_addKey(XrdsecsssAdmin_Opts &Opt)
{
   XrdOucErrInfo eInfo;
   XrdSecsssKT::ktEnt ktEnt;
   int retc, numKeys, numTot, numExp;

// Allocate the initial keytab
//
   Opt.kTab = new XrdSecsssKT(&eInfo, Opt.KeyFile, XrdSecsssKT::isAdmin);
   if ((retc = eInfo.getErrInfo()))
      {if (retc != ENOENT || isNo(0, "Keyfile '", Opt.KeyFile,
           "' does not exist. Create it? (y | n): ")) return 4;
      }

// Construct a new KeyTab entry
//
   strcpy(ktEnt.Name, (Opt.KeyName ? Opt.KeyName : "generic"));
        if (Opt.KeyLen > XrdSecsssKT::ktEnt::maxKLen)
           ktEnt.Len = XrdSecsssKT::ktEnt::maxKLen;
   else if (Opt.KeyLen < 4) ktEnt.Len = 4;
   else ktEnt.Len = Opt.KeyLen/4*4;
   ktEnt.Exp = Opt.Expdt;
   Opt.kTab->addKey(ktEnt);

// Now rewrite the file
//
   if ((retc = Opt.kTab->Rewrite(Opt.Keep, numKeys, numTot, numExp)))
      {eMsg("Unable to add key to '" <<Opt.KeyFile <<"'; " <<strerror(retc));
       retc = 8;
      } else {
       eMsg(numKeys <<(numKeys == 1 ? " key":" keys") <<" out of "
            <<numTot <<" kept (" <<numExp <<" expired).");
      }

// All done
//
   return retc;
}

/******************************************************************************/
/*                 X r d S e c s s s A d m i n _ d e l K e y                  */
/******************************************************************************/
  
int  XrdSecsssAdmin_delKey(XrdsecsssAdmin_Opts &Opt)
{
   XrdOucErrInfo eInfo;
   XrdSecsssKT::ktEnt ktEnt;
   int retc, numKeys, numTot, numExp;

// Allocate the initial keytab
//
   Opt.kTab = new XrdSecsssKT(&eInfo, Opt.KeyFile, XrdSecsssKT::isAdmin);
   if ((retc = eInfo.getErrInfo()))
      {if (retc == ENOENT)
          {eMsg("Keyfile '" <<Opt.KeyFile <<"' does not exist.");
           return 4;
          }
      }

// Construct a new KeyTab entry
//
   strcpy(ktEnt.Name, Opt.KeyName);
   ktEnt.ID = static_cast<long long>(Opt.KeyNum);
   if ((retc = Opt.kTab->delKey(ktEnt)))
      {if (Opt.KeyNum >= 0)
          {eMsg("Key " <<Opt.KeyName <<' ' <<Opt.KeyNum <<" not found.");}
          else {eMsg("No keys named " <<Opt.KeyName <<" found.");}
       return 4;
      }

// It's possible that all of the keys were deleted. Check for that
//
   if (Opt.kTab->keyList() == 0)
      {if (isNo(1, "No keys will remain is ", Opt.KeyFile,
                   ". Delete file? (n | y): "))
          {eMsg("No keys deleted!"); return 2;}
       unlink(Opt.KeyFile);
       return 0;
      }

// Now rewrite the file
//
   if ((retc = Opt.kTab->Rewrite(Opt.Keep, numKeys, numTot, numExp)))
      {eMsg("Unable to del key from '" <<Opt.KeyFile <<"'; " <<strerror(retc));
       retc = 8;
      } else {
       eMsg(numKeys <<(numKeys == 1 ? " key":" keys") <<" out of "
            <<numTot <<" kept (" <<numExp <<" expired).");
      }

// All done
//
   return retc;
}

/******************************************************************************/
/*                 X r d S e c s s s A d m i n _ i n s K e y                  */
/******************************************************************************/
  
int  XrdSecsssAdmin_insKey(XrdsecsssAdmin_Opts &Opt)
{
   XrdOucErrInfo eInfo;
   XrdSecsssKT::ktEnt *ktP;
   int retc, numKeys = 0, numTot, numExp;

// Allocate the initial keytab
//
   Opt.kTab = new XrdSecsssKT(&eInfo, 0, XrdSecsssKT::isAdmin);
   if ((retc = eInfo.getErrInfo())) return 4;

// Check if we need to trim the keytab to a particular key
//
   if (Opt.KeyName)
      {ktP = Opt.kTab->keyList();
       while(ktP)
            {if (strcmp(ktP->Name, Opt.KeyName)) ktP->Name[0] = '\0';
                else numKeys++;
             ktP = ktP->Next;
            }
       if (!numKeys)
          {eMsg("No keys named " <<Opt.KeyName <<" found to install.");
           return 8;
          }
      }

// Now rewrite the file
//
   Opt.kTab->setPath(Opt.KeyFile);
   if ((retc = Opt.kTab->Rewrite(Opt.Keep, numKeys, numTot, numExp)))
      {eMsg("Unable to install keytab '" <<Opt.KeyFile <<"'; " <<strerror(retc));
       retc = 8;
      } else {
       eMsg(numKeys <<(numKeys == 1 ? " key":" keys") <<" out of "
            <<numTot <<" installed (" <<numExp <<" expired).");
      }

// All done
//
   return retc;
}

/******************************************************************************/
/*                 X r d S e c s s s A d m i n _ l s t K e y                  */
/******************************************************************************/
  
int  XrdSecsssAdmin_lstKey(XrdsecsssAdmin_Opts &Opt)
{
   static const char Hdr1[] =
   "     Number Len Date/Time Created Expires  Keyname\n";
//  12345678901 123 mm/dd/yy hh:mm:ss mm/dd/yy
   static const char Hdr2[] =
   "     ------ --- --------- ------- -------- -------\n";

   XrdOucErrInfo eInfo;
   XrdSecsssKT::ktEnt *ktP;
   char crfmt[] = "%D %T", exfmt[] = "%D";
   char buff[128], crbuff[64], exbuff[16];
   int retc, pHdr = 1;

// Allocate the initial keytab
//
   Opt.kTab = new XrdSecsssKT(&eInfo, Opt.KeyFile, XrdSecsssKT::isAdmin);
   if ((retc = eInfo.getErrInfo()))
      {if (retc == ENOENT)
          {eMsg("Keyfile '" <<Opt.KeyFile <<"' does not exist.");
           return 4;
          }
      }

// Obtain the keytab list
//
   ktP = Opt.kTab->keyList();

// List the keys
//
   while(ktP)
        {if (Opt.KeyName && strcmp(Opt.KeyName, ktP->Name)) continue;
         if (pHdr) {cout <<Hdr1 <<Hdr2; pHdr = 0;}
         sprintf(buff, "%11lld %3d ", (ktP->ID & 0x7fffffff), ktP->Len);
         strftime(crbuff, sizeof(crbuff), crfmt, localtime(&ktP->Crt));
         if (!ktP->Exp) strcpy(exbuff, "--------");
            else strftime(exbuff, sizeof(exbuff), exfmt, localtime(&ktP->Exp));
         cout <<buff <<crbuff <<' ' <<exbuff <<' ' <<ktP->Name <<endl;
         ktP = ktP->Next;
        }

// Check if we printed anything
//
   if (pHdr)
      {if (Opt.KeyName) eMsg(Opt.KeyName <<" key not found in " <<Opt.KeyFile);
          else eMsg("No keys found in " <<Opt.KeyFile);
      }
   return 0;
}
