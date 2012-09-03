#ifndef __XRDCLIABSMONINTF_H__
#define __XRDCLIABSMONINTF_H__
/******************************************************************************/
/*                                                                            */
/*                X r d C l i e n t A b s M o n I n t f . h h                 */
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

// XrdClientAbsMonIntf
// Public interface to generic monitoring systems
//

class XrdClientAbsMonIntf {
public:


  // Initialization of the external library
  virtual int Init(const char *src, const char *dest, int debug=0, void *parm=0) = 0;
  virtual int DeInit() = 0;

  // To get the name of the library and other info
  virtual int GetMonLibInfo(char **name, char **version, char **remarks) = 0;


  // To submit a set of info about the progress of something
  // Set force to true to be sure that the info is sent and not eventually skipped
  virtual int PutProgressInfo(long long bytecount=0,
			      long long size=0,
			      float percentage=0.0,
			      bool force=false) = 0;


  XrdClientAbsMonIntf() {};
  virtual ~XrdClientAbsMonIntf() {};
};




/******************************************************************************/
/*                X r d C l i e n t A b s M o n I n t f . h  h               */
/******************************************************************************/
  
// The XrdClientMonIntf() function is called when the shared library containing
// implementation of this class is loaded. It must exist in the library as an
// 'extern "C"' defined function.


#define XrdClientMonIntfArgs const char *src, const char *dst

extern "C" {
  typedef XrdClientAbsMonIntf *(*XrdClientMonIntfHook)(XrdClientMonIntfArgs);
XrdClientAbsMonIntf *XrdClientgetMonIntf(XrdClientMonIntfArgs);
}
#endif
