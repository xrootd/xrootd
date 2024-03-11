#ifndef _XRDOSSARCDATASET_H
#define _XRDOSSARCDATASET_H
/******************************************************************************/
/*                                                                            */
/*                   X r d O s s A r c D a t a s e t . h h                    */
/*                                                                            */
/* (c) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

#include <map>
#include <string>

#include <stdlib.h>
#include <time.h>

#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysRAtomic.hh"

struct cmp_str;
class  XrdOssArcRecompose;

class XrdOssArcDataset
{
public:
// AddFile() is called either when a new file is created which triggers a
// dataset creation or when we are adding files from a manifest. These run
// asynchronously to prevent one locking out the other as datasets can be
// very large.
//
       bool             AddFile(const char* tid, const char* dsn, 
                                const char* fsn, bool isCreate);

// Archive is called when the full dataset has been successfully copied to the
// tape RSE (i.e. us). It the creates a zip file, moves it to the tape buffer
// for transfer to tape, and registers the zip archive. It runs async mode.
// All of these action are performed by a script that it invokes.
//
       bool              Archive();

// Complete marks the path as a fully formed file that can be archived. It 
// returns true of the path is archivable and exists in the dataset ad false
// otherwise. Typcally used by query prepare.
//
static int               Complete(const char* path, time_t& reqTime);

// When placing a file in the archive, Create() must be the first call. We
// use this to create a respresentation of the dataset as well as verifying 
// against a true second party manifest.
//
static int               Create(const char* tid, XrdOssArcRecompose& dsInfo);

// Delete() must be called to delete the dataset object and its extensions.
// This also removes the dataset from the dataset map.
//
       void              Delete(const char* why);

// This is called asynchronously to fill out the fileset for a newly created
// dataset. During this time we allow files to be added to the dataset.
//
       bool              GetManifest();

       void              Ref(int);

       int               Ref() {return refCnt;}

       bool              RevertFile(const char* tid, const char* path);

// Unlink() is called when a file is unlinked in a dataset so that we can mark
// it as yet to be created and certainly not complete.
//
static void              Unlink(const char* tid, XrdOssArcRecompose& dsInfo);


       XrdOssArcDataset(XrdOssArcRecompose& dsInfo);

private:
      ~XrdOssArcDataset(); // Use Delete() method!

bool AddFile(const char* path, const char* dsn, const char* tid=0);

time_t      crTime;       // Time this dataset was established
char*       dsName;       // External dataset name
char*       dsDir;        // Directory path to local dataset
int         didCnt;       // Number if did's in this dataset using Manifest
RAtomic_int Ready{0};     // Number of files ready to be archived
RAtomic_int refCnt{0};  
bool        isDead  = false;
bool        zipping = false;

static XrdSysMutex dsMapMutex;
static std::map<const char*, XrdOssArcDataset*, cmp_str> dsMap;

struct dsFile
      {bool  created;
       bool  complete = false;

             dsFile(bool isCreate) : created(isCreate) {}
            ~dsFile() {}
};
       XrdSysMutex fsMapMutex;
       std::map<std::string, dsFile> fsMap;
};
#endif
