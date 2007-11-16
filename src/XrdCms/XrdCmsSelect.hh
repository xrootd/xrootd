#ifndef __CMS_SELECT_HH
#define __CMS_SELECT_HH
/******************************************************************************/
/*                                                                            */
/*                       X r d C m s S e l e c t . h h                        */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$

/******************************************************************************/
/*                    C l a s s   X r d C m s S e l e c t                     */
/******************************************************************************/
  
class XrdCmsSelect
{
public:
char          *Path;    //  In: Path to select
int            PLen;    //  In: Length of path
int            iovN;    //  In: Prepare notification I/O vector count
struct iovec  *iovP;    //  In: Prepare notification I/O vector
SMask_t        nmask;   //  In: Nodes to avoid
XrdCmsRRQInfo *InfoP;   //  In: Fast redirect routing
int            Opts;    //  In: One or more of the following enums

enum {Write   = 0x0001, // File will be open in write mode (select & cache)
      NewFile = 0x0802, // Gile will be created should not exist
      Online  = 0x0004, // Only consider online files
      Trunc   = 0x0808, // File will be truncated (w/ NewFile may exist)
      Peers   = 0x0020, // Peer clusters may be selected
      Refresh = 0x0040, // Cache should be refreshed
      Asap    = 0x0080, // Respond as soon as possible
      noStage = 0x0800, // Do not stage the file
      Pending = 0x8000  // File being staged (select & cache)
     };

struct {SMask_t wf;     // Out: Writable locations
        SMask_t hf;     // Out: Existing locations
        SMask_t pf;     // Out: Pending  locations
       }     Vec;

struct {int  Port;      // Out: Target node port number
        char Data[256]; // Out: Target node or error message
        int  DLen;      // Out: Length of Data including null byte
       }     Resp;

             XrdCmsSelect(int opts=0)
                         {Opts = opts; Resp.Port = 0;
                          *Resp.Data = '\0'; Resp.DLen = 0;
                              }
            ~XrdCmsSelect() {}
};

/******************************************************************************/
/*                  C l a s s   X r d C m s S e l e c t e d                   */
/******************************************************************************/
  
class XrdCmsSelected   // Argument to List() after select or locate
{
public:

XrdCmsSelected *next;
char           *Name;
SMask_t         Mask;
int             Id;
unsigned int    IPAddr;
int             Port;
int             IPV6Len;  // 12345678901234567890123456
char            IPV6[28]; // [::123.123.123.123]:123456
int             Load;
int             Util;
int             Free;
int             RefTotA;
int             RefTotR;
int             Status;      // One of the following

enum           {Disable = 0x0001,
                NoStage = 0x0002,
                Offline = 0x0004,
                Suspend = 0x0008,
                NoSpace = 0x0020,
                isRW    = 0x0040,
                Reservd = 0x0080,
                isMangr = 0x0100,
                isPeer  = 0x0200,
                isProxy = 0x0400,
                noServr = 0x0700
               };

               XrdCmsSelected(const char *sname, XrdCmsSelected *np=0)
                         {Name = (sname ? strdup(sname) : 0); next=np;}

              ~XrdCmsSelected() {if (Name) free(Name);}
};
#endif
