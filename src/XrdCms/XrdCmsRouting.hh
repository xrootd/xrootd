#ifndef __CMS_ROUTING_H__
#define __CMS_ROUTING_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d C m s R o u t i n g . h h                       */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$

#include <stdarg.h>

#include "XProtocol/YProtocol.hh"

class XrdCmsRouting
{
public:

enum {isInvalid = 0x00,
      isAsync   = 0x01,
      isSync    = 0x02,
      Forward   = 0x04,
      noArgs    = 0x08,
      Delayable = 0x10
     };

inline int  getRoute(int reqCode)
                      {return reqCode < XrdCms::kYR_MaxReq
                                      ? valVec[reqCode] : isInvalid;
                      }

            XrdCmsRouting(int mVal, ...)
                          {va_list ap;
                           int     vp = mVal;
                           memset(valVec, 0, sizeof(valVec));
                           va_start(ap, mVal);
                           do { valVec[vp] = va_arg(ap, int);
                              } while((vp  = va_arg(ap, int)));
                           va_end(ap);
                           }
            ~XrdCmsRouting() {}

private:
int          valVec[XrdCms::kYR_MaxReq];
};

/******************************************************************************/
/*                    X r d C m s R o u t e r   C l a s s                     */
/******************************************************************************/

class XrdCmsNode;
class XrdCmsRRData;
  
class XrdCmsRouter
{
public:

typedef const char *(XrdCmsNode::*NodeMethod_t)(XrdCmsRRData &);

inline  NodeMethod_t getMethod(int Code)
                           {return Code < XrdCms::kYR_MaxReq
                                        ? methVec[Code] : (NodeMethod_t)0;
                           }

inline  const char  *getName(int Code)
                            {return Code < XrdCms::kYR_MaxReq
                                         ? nameVec[Code] : "?";
                            }

              XrdCmsRouter(int mVal, ...)
                          {va_list ap;
                           int     vp = mVal;
                           NodeMethod_t *Method;
                           memset(methVec, 0, sizeof(methVec));
                           va_start(ap, mVal);
                           do { nameVec[vp] = (const char  *)va_arg(ap,const char *);
                                Method      = (NodeMethod_t*)va_arg(ap,NodeMethod_t*);
                                methVec[vp] = (Method ? *Method : (NodeMethod_t) 0);
                              } while((vp = va_arg(ap, int)));
                           va_end(ap);
                           }
             ~XrdCmsRouter() {}

private:

const  char         *nameVec [XrdCms::kYR_MaxReq];
       NodeMethod_t  methVec [XrdCms::kYR_MaxReq];
};

namespace XrdCms
{
extern XrdCmsRouter  Router;
extern XrdCmsRouting rdrVOps;
extern XrdCmsRouting rspVOps;
extern XrdCmsRouting srvVOps;
extern XrdCmsRouting supVOps;
}
#endif
