/*****************************************************************************/
/*                                                                           */
/*                             XrdMonDecSync.cc                              */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMonDecSync.hh"

extern "C" void* synchronizeDecBuffers(void* arg)
{
    int* noSecToSleep = dynamic_cast<int*>(arg);
    
}


