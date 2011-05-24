/******************************************************************************/
/*                                                                            */
/*                        X r d F r c T r a c e . c c                         */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include "XrdFrc/XrdFrcTrace.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
       XrdSysError        XrdFrc::Say(0, "frm_");

       XrdOucTrace        XrdFrc::Trace(&Say);
