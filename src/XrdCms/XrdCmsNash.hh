#ifndef __XRDCMSNASH_HH__
#define __XRDCMSNASH_HH__
/******************************************************************************/
/*                                                                            */
/*                         X r d C m s N a s h . h h                          */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include "XrdCms/XrdCmsCache.hh"
  
class XrdCmsNashKey
{
public:

         char         *Key;
unsigned long long     Hash;
XrdCmsNashItem        *Item;
union   {unsigned int  streamid;
         struct {short KLen;
                 short LATRef;
                }      Info;

         XrdCmsNashKey(char *key, int klen=0)
                      : Key(key), Hash(0), Info.KLen(klen), Info.HLBRef(0);
        ~XrdCmsNashKey() {};
};

class XrdCmsNash
{
public:

enum {Add2HLB = 1};

XrdCmsCInfo *Add(XrdCmsKey &KeyInfo, XrdCmsCInfo &KeyData);

XrdCmsCInfo *Find(XrdCmsKey &KeyInfo);

void         Trim(unsigned int Tick);

// When allocateing a new nash, specify the required starting size. Make
// sure that the previous number is the correct Fibonocci antecedent. The
// series is simply n[j] = n[j-1] + n[j-2].
//
    XrdCmsNash(int psize = 17711, int size = 28657);
   ~XrdCmsNash() {} // Never gets deleted

private:

static const int LATMax  = 1024;
static const int LoadMax = 80;

void               Expand();
void               Remove(int kent, XrdCmsNashItem *nip, XrdCmsNashItem *pnip);
XrdCmsNashItem    *Search(XrdCmsNashItem *nip,  unsigned long long khash,
                          const char     *kval);
unsigned long long XHash(const char *KeyVal, int KeyLen);

XrdCmsNashItem  *LATable[LATMax];
XrdCmsNashItem **nashtable;
int              prevtablesize;
int              nashtablesize;
int              nashnum;
int              Threshold;
};
#endif
