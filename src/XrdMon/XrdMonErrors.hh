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

// Decoder. Range: 2000-2999
const err_t ERR_INVDICTSTRING   = 2001;
const err_t ERR_INVALIDINFOTYPE = 2003;
const err_t ERR_NEGATIVEOFFSET  = 2004;
const err_t ERR_DICTIDINCACHE   = 2005;
const err_t ERR_NODICTIDINCACHE = 2006;
const err_t ERR_OUTOFMEMORY     = 2007;
const err_t ERR_FILENOTCLOSED   = 2009;
const err_t ERR_FILENOTOPEN     = 2010;
const err_t ERR_FILEOPEN        = 2011;
const err_t ERR_PUBLISHFAILED   = 2012;
const err_t ERR_INVALIDTIME     = 2013;
const err_t ERR_NOTATIMEWINDOW  = 2014;
const err_t ERR_INVALIDFNAME    = 2015;
const err_t ERR_CANNOTOPENFILE  = 2016;
const err_t ERR_INTERNALERR     = 2017;
const err_t ERR_INVALIDSEQNO    = 2018;
const err_t ERR_TOOMANYLOST     = 2019;


#endif /* XRDMONERRORS_HH */
