#ifndef __SecsssKT__
#define __SecsssKT__
/******************************************************************************/
/*                                                                            */
/*                        X r d S e c s s s K T . h h                         */
/*                                                                            */
/* (c) 2008 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//       $Id$

#include <string.h>
#include <time.h>
#include "XrdSys/XrdSysPthread.hh"

class XrdOucErrInfo;

class XrdSecsssKT
{
public:

class ktEnt
      {public:
       long long ID;
       time_t    Crt;
       time_t    Exp;
       ktEnt    *Next;
       int       Opts;
       static const int useLID = 1;
       static const int anyUSR = 2;
       static const int anyGRP = 4;
       int       Len;
       static const int maxKLen = 128;
       char      Val[maxKLen];// Key strings are 1024 bits or less
       static const int NameSZ  = 256;
       char      Name[NameSZ];// Key names are null terminated 255 chars or less
       static const int UserSZ  = 128;
       char      User[UserSZ];// Usr names are null terminated 127 chars or less
       static const int GrupSZ  =  64;
       char      Grup[GrupSZ];// Grp names are null terminated  63 chars or less

       void      NUG(ktEnt *ktP) {strcpy(Name, ktP->Name);
                                  strcpy(User, ktP->User);
                                  strcpy(Grup, ktP->Grup);
                                 }
       void      Set(ktEnt &rhs) {ID=rhs.ID; Crt=rhs.Crt; Exp=rhs.Exp;
                                  Len = rhs.Len; memcpy(Val, rhs.Val, Len);}

       ktEnt() : ID(-1), Next(0), Opts(0) {*Val = '\0'; *Name = '\0';
                                           *User= '\0'; *Grup = '\0';}
      ~ktEnt() {}
      };

void   addKey(ktEnt &ktNew);

int    delKey(ktEnt &ktDel);

static
char  *genFN();

static
void   genKey(char *Buff, int blen);

int    getKey(ktEnt &ktEql);

ktEnt *keyList() {return ktList;}

void   Refresh();

time_t RefrTime() {return ktRefT;}

int    Rewrite(int Keep, int &numKeys, int &numTot, int &numExp);

int    Same(const char *path) {return (ktPath && !strcmp(ktPath, path));}

void   setPath(const char *Path) 
              {if (ktPath) free(ktPath); ktPath = strdup(Path);}

enum   xMode {isAdmin = 0, isClient, isServer};

       XrdSecsssKT(XrdOucErrInfo *, const char *, xMode, int refr=60*60);
      ~XrdSecsssKT();

private:
int    eMsg(const char *epn, int rc, const char *txt1,
            const char *txt2=0, const char *txt3=0, const char *txt4=0);
ktEnt *getKeyTab(XrdOucErrInfo *eInfo, time_t Mtime, mode_t Amode);
mode_t fileMode(const char *Path);
int    isKey(ktEnt &ktRef, ktEnt *ktP, int Full=1);
void   keyB2X(ktEnt *theKT, char *buff);
void   keyX2B(ktEnt *theKT, char *xKey);

XrdSysMutex myMutex;
char       *ktPath;
ktEnt      *ktList;
time_t      ktMtime;
xMode       ktMode;
time_t      ktRefT;
int         kthiID;
static int  randFD;
};
#endif
