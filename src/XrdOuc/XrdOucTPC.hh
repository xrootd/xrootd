#ifndef __XRDOUCTPC_HH__
#define __XRDOUCTPC_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d O u c T P C . h h                           */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include <stdlib.h>

class XrdOucTPC
{
public:

static
const char *cgiC2Dst(const char *cKey, const char *xSrc, const char *xLfn,
                     const char *xCks,       char *Buff, int Blen);

static
const char *cgiC2Src(const char *cKey, const char *xDst, int xTTL,
                           char *Buff, int Blen);

static
const char *cgiD2Src(const char *cKey, const char *cOrg,
                           char *Buff, int Blen);

static
const char *tpcCks;
static
const char *tpcDst;
static
const char *tpcKey;
static
const char *tpcLfn;
static
const char *tpcOrg;
static
const char *tpcSrc;
static
const char *tpcTtl;

            XrdOucTPC() {}
           ~XrdOucTPC() {}
private:

struct tpcInfo
      {char *Data;
             tpcInfo() : Data(0) {}
            ~tpcInfo() {if (Data) free(Data);}
      };
};
#endif
