#ifndef __OLB_MANLIST__H
#define __OLB_MANLIST__H
/******************************************************************************/
/*                                                                            */
/*                      X r d O l b M a n L i s t . h h                       */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include "XrdOuc/XrdOucPthread.hh"

//         $Id$

class XrdOlbManRef;

class XrdOlbManList
{
public:

// Add() adds an alternate manager to the list of managers (duplicates not added)
//
void     Add(unsigned int refp, char *manp, int manport);

// Del() removes all entries added under refp
//
void     Del(unsigned int refp);

// haveAlts() returns true if alternates exist, false otherwise
//
int      haveAlts() {return allMans != 0;}

// Next() returns the next manager in the list or 0 if none are left.
//        The next call to Next() will return the first manager in the list.
//
char    *Next(int &port, char *buff, int bsz);

         XrdOlbManList() {allMans = nextMan = 0;}
        ~XrdOlbManList();

private:

XrdOucMutex   mlMutex;
XrdOlbManRef *nextMan;
XrdOlbManRef *allMans;
};
#endif
