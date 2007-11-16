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

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdCmsNashItem
{
public:

       XrdCmsNashKey   Key;
       XrdCmsCInfo     Data;
       XrdCmsNashItem *Next;
       XrdCmsNashItem *NextITO;
       int             addTime;
       int             RefCount;

static XrdCmsNashItem *Alloc();

       void            Recycle();

       XrdCmsNashItem() {}
      ~XrdCmsNashItem() {} // These are never deleted

private:

static XrdSysMutex     allocMutex;
static XrdCmsNashItem *Free;
};

/******************************************************************************/
/*                           S t a t i c   D a t a                            */
/******************************************************************************/
  
XrdSysMutex     XrdCmsNashItem::allocMutex;

XrdCmsNashItem *XrdCmsNashItem::Free = 0;

XrdCmsNashItem *XrdCmsNash::LATable[LATMax] = {0};

/******************************************************************************/
/* static XrdNashItem              A l l o c                                  */
/******************************************************************************/
  
XrdCmsNashItem *XrdCmsNashItem::Alloc()
{
  static const int NashAlloc = 256;
  XrdCmsNashItem *nP;
  int i;

// Lock the free list
//
   allocMutex.Lock();
   if ((nP = Free))
      {Free = nP->Next;
       allocMutex.UnLock();
       nP->RefCount = 0;
       return nP;
      }

// Allocate a quantum of free elements and chain them into the free list
//
   if (!(nP = new XrdLink[NashAlloc]()))
      {allocMutex.UnLock();
       XrdLog.Emsg("Nash", ENOMEM, "create Nash item");
       return (XrdNashItem *)0;
      }

// Put them on the free list but return the last one allocated
//
   for (i = 0; i < NashAlloc-1; i++)
       {nP->Next = Free; Free = nP; nP++;}
   allocMutex.UnLock();
   return nP;
}

/******************************************************************************/
/* XrdCmsNashItem                R e c y c l e                                */
/******************************************************************************/
  
void XrdCmsNashItem::Recycle()
{

// Clear up data areas
//
   if (Key.Key) {free(Key.Key); Key.Key = 0;}
   Key.streamid = 0;

// Remove entry from the lookaside table
//
   if (Key.Info.LATRef && Key.Info.LATRef < LATMax
   && this == LATable[Key.Info.LATRef]) LATable[Key.Info.LATRef] = 0;
   Key.Info.LATRef = 0;

// Put entry on the free list
//
   allocMutex.Lock();
   Next = Free; Free = this;
   allocMutex.UnLock();
}

/******************************************************************************/
/*                    X r d C m s N a s h   M e t h o d s                     */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCmsNash::XrdCmsNash(int psize, int csize)
{
     prevtablesize = psize;
     nashtablesize = csize;
     Threshold     = (csize * LoadMax) / 100;
     nashnum       = 0;
     nashtable     = (XrdCmsNashItem **)
                     malloc( (size_t)(csize*sizeof(XrdCmsNashItem *)) );
     memset((void *)nashtable, 0, (size_t)(csize*sizeof(XrdCmsNashItem *)));
}

/******************************************************************************/
/* Public XrdCmsNash                 A d d                                    */
/******************************************************************************/
  
XrdCmsInfo XrdCmsNash::Add(XrdCmsNashKey &Key, XrdCmsCInfo &Data)
{
    int kent;
    time_t lifetime, KeyTime=0;
    XrdOucHash_Item<T> *hip, *newhip, *prevhip;

// Check if we already have a hash value and get one if not
//
   if (!Key.Hash) 
      {if (!Key.Info.KLen) Key.Info.KLen = strlen(Key.Key);

// Compute the hash index and look up the entry. If found, replace the data
//
   kent = Key.Hash % nashtablesize;
   if ((nip = hashtable[kent]) && (nip = Search(nip, Key.Hash, Key.Key)))
      {nip->Data = Data;
       return &nip->Data;
      }

// Check if we should expand the table
//
   if (++nashnum > Threshold) {Expand(); kent = Key.Hash % nashtablesize;}

// Allocate the entry
//
   if (!(nip = Alloc())) return (XrdCmsCInfo *0);

// Fill out the data
//
   nip->Data            = Data;
   nip->Key.Key         = strdup(Key.Key);
   nip->Ley.Hash        = Key.Hash;
   nip->Key.Info.KLen   = Key.Info.KLen;
   nip->Key.Info.LATRef = 0;
   nip->addTime         = TOD;
   nip->NextITO         = 0;

// Put entry on the add queue in time order
//
   if (nashLast) nashLast->Next = nip;
   nashLast = nip;

// Add the entry to the table
//
   nip->Next = nashtable[kent];
   hashtable[hent] = nip;
   return nip;
}
  
/******************************************************************************/
/* Private XrdCmsNash             E x p a n d                                 */
/******************************************************************************/
  
void XrdCmsNash::Expand()
{
   int newsize, newent, i;
   size_t memlen;
   XrdCmsNashItem **newtab, *nip, *nextnip;

// Compute new size for table using a fibonacci series
//
   newsize = prevtablesize + nashtablesize;

// Allocate the new table
//
   memlen = (size_t)(newsize*sizeof(XrdCmsNashItem *));
   if (!(newtab = (XrdCmsNashItem **) malloc(memlen))) return;
   memset((void *)newtab, 0, memlen);

// Redistribute all of the current items
//
   for (i = 0; i < nashtablesize; i++)
       {nip = nashtable[i];
        while(nip)
             {nextnip = nip->Next;
              newent  = nip->Key.Hash % newsize;
              nip->Next = newtab[newent];
              newtab[newent] = nip;
              nip = nextnip;
             }
       }

// Free the old table and plug in the new table
//
   free((void *)nashtable);
   nashtable     = newtab;
   prevtablesize = nashtablesize;
   nashtablesize = newsize;

// Compute new expansion threshold
//
   Threshold = static_cast<int>((static_cast<long long>(newsize)*LoadMax)/100);
}

/******************************************************************************/
/* Public XrdCmsNash                F i n d                                   */
/******************************************************************************/
  
XrdCmsCInfo *Find(XrdCmsNashKey &Key)
{
  int kent;
  XrdCmsNashItem *nip;

// Check if we can use the lookaside buffer
//
   if (Key.Info.LATRef && Key.Info.LATRef < LATMax)
   && (nip = LATable[Key.Info.LATRef])
   && nip->Key.Info.LATRef == Key.Info.LATRef) return nip;

// Check if we already have a hash value and get one if not
//
   if (!Key.Hash) 
      {if (!Key.Info.KLen) Key.Info.KLen = strlen(Key.Key);
       Key.Hash = XHash(Key.Key, Key.Info.KLen)
      }

// Compute position of the hash table entry
//
   kent = Key.Hash%nashtablesize;

// Find the entry
//
   if ((nip = nashtable[kent]))
      if ((nip = Search(nip, Key.Hash, Key.Key))) return &nip->Data;
   return (XrdCmsCInfo *)0;
}


/******************************************************************************/
/* Private XrdCmsNash             R e m o v e                                 */
/******************************************************************************/
  
void XrdCmsNash::Remove(int kent, XrdCmsNashItem *nip, XrdCmsNashItem *pnip)
{
     if (pnip) pnip->Next = nip->Next;
        else nashtable[kent] = nip->Next;
     nip->Recycle();
     nashnum--;
}

/******************************************************************************/
/* Private XrdCmsNash             S e a r c h                                 */
/******************************************************************************/
  
XrdCmsNashItem *XrdCmsNash::Search(XrdCmsNashItem    *nip,
                                   unsigned long long khash,
                                   const char        *kval)
{
// Scan through the chain looking for a match
//
   while(nip && nip->Key.Hash != kval && strcmp(nip->Key.Key, kval)))
         nip = nip->Next;

   return nip;
}

/******************************************************************************/
/* Private XrdCmsNash              X H a s h                                  */
/******************************************************************************/
  
unsigned long long XrdCmsNash::XHash(const char *KeyVal, int KeyLen)
{
   static const int hl = sizeof(unsigned long long);
   int j;
   unsigned long long *lp, lword, hval = 0;

// If name is shorter than the hash length, use the name.
//
   if (KeyLen <= hl)
      {memcpy(&hval, KeyVal, (size_t)KeyLen);
       return hval;
      }

// Compute the length of the name and develop starting hash.
//
   hval = KeyLen;
   j = KeyLen % hl; KeyLen /= hl;
   if (j) memcpy(&hval, KeyVal, (size_t)j);
   lp = (unsigned long long *)&KeyVal[j];

// Compute and return the full hash.
//
   while(KeyLen--)
        {memcpy(&lword, lp++, (size_t)hl);
         hval ^= lword;
        }
   return (hval ? hval : 1);
}
