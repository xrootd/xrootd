/*****************************************************************************/
/*                                                                           */
/*                              XrdMonErrors.hh                              */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONERRORS_HH
#define XRDMONERRORS_HH

typedef int err_t;

// common. Range: 0001-0999
const err_t ERR_INVPACKETLEN    = 0001;
const err_t ERR_INVPACKETTYPE   = 0002;
const err_t ERR_INVALIDARG      = 0003;
const err_t ERR_INVALIDADDR     = 0004;
const err_t ERR_NODIR           = 0005;

// Collector. Range: 1000-1999
const err_t ERR_NOAVAILLOG      = 1001;
const err_t ERR_RECEIVE         = 1002;
const err_t ERR_RENAME          = 1003;
const err_t ERR_NOMEM           = 1004;
const err_t ERR_UNKNOWN         = 1005;
const err_t ERR_SENDERNOTREG    = 1006;

const err_t SIG_SHUTDOWNNOW     = 1999;


#endif /* XRDMONERRORS_HH */
