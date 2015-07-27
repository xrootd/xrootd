#ifndef __XRDXMLRDRTINY_HH__
#define __XRDXMLRDRTINY_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d X m l R d r T i n y . h h                       */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdXml/XrdXmlReader.hh"
  
//-----------------------------------------------------------------------------
//! The XrdXmlRdrTiny object provides a xml parser based on libxml2.
//-----------------------------------------------------------------------------

class  TiXmlDocument;
class  TiXmlElement;
class  TiXmlNode;

class XrdXmlRdrTiny : public XrdXmlReader
{
public:

virtual bool    GetAttributes(const char **aname, char **aval);

virtual int     GetElement(const char **ename, bool reqd=false);

virtual
const char     *GetError(int &ecode) {return ((ecode = eCode) ? eText : 0);}

virtual char   *GetText(const char *ename, bool reqd=false);

static  bool    Init();

//-----------------------------------------------------------------------------
//! Constructor & Destructor
//-----------------------------------------------------------------------------

                XrdXmlRdrTiny(bool &aOK, const char *fname, const char *enc=0);
virtual        ~XrdXmlRdrTiny();

private:
void            Debug(const char *,const char *,const char *,const char *,int);

TiXmlDocument  *reader;
TiXmlNode      *curNode;
TiXmlElement   *curElem;
TiXmlNode      *elmNode;
int             eCode;
bool            debug;
char            eText[251];
};
#endif
