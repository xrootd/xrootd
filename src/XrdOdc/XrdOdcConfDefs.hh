#ifndef _ODC_CONFDEFS_H
#define _ODC_CONFDEFS_H
/******************************************************************************/
/*                                                                            */
/*                     X r d O d c C o n f D e f s . h h                      */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//          $Id$

enum XrdOdcPselT { selByFD, selByLD, selByRR};

#define maxPORTS 16

enum   {Find_Create  = 0x0001, Find_LocInfo = 0x0002, Find_NoDelay = 0x0004,
        Find_RDWR    = 0x0008, Find_Read    = 0x0010, Find_Refresh = 0x0020,
        Find_Stat    = 0x0040, Find_Trunc   = 0x0080, Find_Write   = 0x0100
       };
#endif
