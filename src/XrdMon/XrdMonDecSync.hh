/*****************************************************************************/
/*                                                                           */
/*                             XrdMonDecSync.hh                              */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONDECSYNC_HH
#define XRDMONDECSYNC_HH

// Runs in a dedicated thread. Wakes up periodically and
// send signal to Sink to flush its contents
// Used only for real time decoding

extern "C" void* synchronizeDecBuffers(void*);

#endif /* XRDMONDECSYNC_HH */
