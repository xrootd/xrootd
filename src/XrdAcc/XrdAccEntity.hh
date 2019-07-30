#ifndef __ACC_ENTITY_H__
#define __ACC_ENTITY_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d A c c E n t i t y . h h                        */
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

#include <stdlib.h>
#include <vector>

/******************************************************************************/
/*                      X r d A c c E n t i t y I n f o                       */
/******************************************************************************/

struct XrdAccEntityInfo
      {const char *name;      // Filled in by caller
       const char *host;      // Filled in by caller
       const char *vorg;
       const char *role;
       const char *grup;
                   XrdAccEntityInfo() {}
                  ~XrdAccEntityInfo() {}
      };

/******************************************************************************/
/*                          X r d A c c E n t i t y                           */
/******************************************************************************/

class XrdOucTokenizer;
class XrdSecEntity;
class XrdSysError;
  
class XrdAccEntity
{
public:

static
XrdAccEntity *GetEntity(const XrdSecEntity *secP);

bool          Next(int &seq, XrdAccEntityInfo &info)
                  {if (int(attrVec.size()) <= seq) return false;
                   EntityAttr *aP = &attrVec[seq++];
                   info.vorg = aP->vorg;
                   info.role = aP->role;
                   info.grup = aP->grup;
                   return true;
                  }

void          PutEntity();

static
void          setError(XrdSysError *errP);

private:

              XrdAccEntity(const XrdSecEntity *secP, bool &aOK);

             ~XrdAccEntity() {if (vorgInfo) free(vorgInfo);
                              if (roleInfo) free(roleInfo);
                              if (grpsInfo) free(grpsInfo);
                             }

bool OneOrZero(char *src, const char *&dest);
bool setAttr(XrdOucTokenizer &tkl, const char *&dest);

struct EntityAttr
      {const char *vorg;
       const char *role;
       const char *grup;
                   EntityAttr() {}
                  ~EntityAttr() {}
      };


std::vector<EntityAttr> attrVec;

char *vorgInfo;
char *roleInfo;
char *grpsInfo;
int   ueid;
};
  
/******************************************************************************/
/*                      X r d A c c E n t i t y I n i t                       */
/******************************************************************************/
  
class XrdAccEntityInit
{
public:

      XrdAccEntityInit(const XrdSecEntity *secP, XrdAccEntity *&aeR)
                      {aeR = aeP = XrdAccEntity::GetEntity(secP);}

     ~XrdAccEntityInit() {if (aeP) aeP->PutEntity();}

private:

XrdAccEntity *aeP;
};
#endif
