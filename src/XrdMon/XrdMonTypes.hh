/*****************************************************************************/
/*                                                                           */
/*                              XrdMonTypes.hh                               */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$
                                                                                                          
#if defined(sun)
#include <sys/types.h>
#endif
                                                                                                          
#if defined (__linux__)
#include <stdint.h>
#endif

typedef int64_t  offset_t;
typedef int32_t  length_t;
typedef int32_t  dictid_t;
typedef uint8_t  packet_t;
typedef uint8_t  sequen_t;
typedef uint16_t packetlen_t;


