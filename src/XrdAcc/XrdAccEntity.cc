/******************************************************************************/
/*                                                                            */
/*                       X r d A c c E n t i t y . c c                        */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <cstdio>
#include <cstring>

#include "XrdAcc/XrdAccEntity.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

int XrdAccEntity::accSig = 0;
  
namespace
{
XrdSysError      *eDest = 0;
}
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdAccEntity::XrdAccEntity(const XrdSecEntity *secP, bool &aOK)
             : XrdSecAttr(&accSig)
{
   EntityAttr attrInfo;
   int have, want = 0;

// Assume all is going to be well and set our unique id.
//
   aOK = true;

// Copy out the various attributes we want to tokenize
//
   if (secP->vorg) {vorgInfo = strdup(secP->vorg); want++;}
      else vorgInfo = 0;
   if (secP->role) {roleInfo = strdup(secP->role); want++;}
      else roleInfo = 0;
   if (secP->grps) {grpsInfo = strdup(secP->grps); want++;}
      else grpsInfo = 0;

// If there are no attributes, then we are done.
//
   if (!want) return;

// If there is zero or one vorg and role then we can accept a short form
// attribute entry. This provides not only backward compatabilty but also 
// takes care of the common case.
//
   if (OneOrZero(vorgInfo, attrInfo.vorg) && OneOrZero(roleInfo, attrInfo.role))
      {if (grpsInfo)
          {XrdOucTokenizer grpsLine(grpsInfo);
           grpsLine.GetLine();
           while((attrInfo.grup = grpsLine.GetToken()))
                attrVec.push_back(attrInfo);
          }
       if (attrVec.size() == 0) attrVec.push_back(attrInfo);
       return;
      }

// Tokenize each of the lists
//
   XrdOucTokenizer vorgLine(vorgInfo);
   if (vorgInfo) vorgLine.GetLine();
   attrInfo.vorg = 0;

   XrdOucTokenizer roleLine(roleInfo);
   if (roleInfo) roleLine.GetLine();
   attrInfo.role = 0;

   XrdOucTokenizer grpsLine(grpsInfo);
   if (grpsInfo) grpsLine.GetLine();
   attrInfo.grup = 0;

   while(true)
        {have = 0;
         if (vorgInfo && setAttr(vorgLine, attrInfo.vorg)) have++;
         if (roleInfo && setAttr(roleLine, attrInfo.role)) have++;
         if (grpsInfo && setAttr(grpsLine, attrInfo.grup)) have++;
         if (want != have) break;
         attrVec.push_back(attrInfo);
        }

// Check if pairing was violated and indicate if so.
//
   if (have) aOK = false;
}

/******************************************************************************/
/*                             G e t E n t i t y                              */
/******************************************************************************/

XrdAccEntity *XrdAccEntity::GetEntity(const XrdSecEntity *secP, bool &isNew)
{
   XrdAccEntity *aeP;
   XrdSecAttr   *seP;
   bool aOK;

// If we already compiled the identity informaion, reuse it.
//
   if ((seP = secP->eaAPI->Get(&accSig)))
      {isNew = false;
       return static_cast<XrdAccEntity *>(seP);
      }

// At this point we muxt create a new entity for authorization purposes and
// return it if all went well. We do not attach it to its SecEntity object as
// this will be done by the AccEntityInit object upon deletion to avoid
// race conditions and memory leaks. This allows for parallel processing.
//
   isNew = true;
   aeP = new XrdAccEntity(secP, aOK);
   if (aOK) return aeP;

// Produce message indicating why we failed (there is only one possible reason)
//
   if (eDest)
      {char eBuff[128];
       snprintf(eBuff, sizeof(eBuff), "missing attrs in col %d for",
                static_cast<int>(aeP->attrVec.size()));
       eDest->Emsg("Entity", "Unable to validate entity;", eBuff,
                             (secP->tident ? secP->tident : "???"));
      }
   delete aeP;
   return 0;
}
  
/******************************************************************************/
/* Private:                    O n e O r Z e r o                              */
/******************************************************************************/
  
bool XrdAccEntity::OneOrZero(char *src, const char *&dest)
{

// If there is no source, then we are done
//
   if (!src)
      {dest = 0;
       return true;
      }

// Check if source has only one item;
//
   while(*src == ' ') src++;
   char *sP = src;
   while(*src && *src != ' ') src++;
   char *eP = src;
   while(*src == ' ') src++;
   if (*src) return false;
   if (*sP) {dest = sP; *eP = 0;}
      else dest = 0;
   return true;
}

/******************************************************************************/
/*                             P u t E n t i t y                              */
/******************************************************************************/
  
void XrdAccEntity::PutEntity(const XrdSecEntity *secP)
{

// Add this object to the indicated SecEntity object. There may be one there
// already if some other thread beat us to the punch (unlike). If there is
// we simply delete ourselves to avoid a memory leak.
//
   if (!secP->eaAPI->Add(*this)) delete this;
}

/******************************************************************************/
/* Private:                      s e t A t t r                                */
/******************************************************************************/
  
bool XrdAccEntity::setAttr(XrdOucTokenizer &tkl, const char *&dest)
{
   const char *attr = tkl.GetToken();
   if (!attr || !dest || strcmp(dest, attr)) dest = attr;
   return attr != 0;
}

/******************************************************************************/
/*                              s e t E r r o r                               */
/******************************************************************************/
  
void XrdAccEntity::setError(XrdSysError *erp) {eDest = erp;}
