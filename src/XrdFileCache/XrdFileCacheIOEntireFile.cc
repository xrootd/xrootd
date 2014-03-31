//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University
// Author: Alja Mrak-Tadel, Matevz Tadel, Brian Bockelman
//----------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------------------

#include <stdio.h>

#include "XrdClient/XrdClientConst.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysPthread.hh"

#include "XrdFileCacheIOEntireFile.hh"
#include "XrdFileCacheStats.hh"
#include "XrdFileCacheFactory.hh"

using namespace XrdFileCache;

void *PrefetchRunner(void * prefetch_void)
{
   XrdFileCache::Prefetch *prefetch = static_cast<XrdFileCache::Prefetch *>(prefetch_void);
   if (prefetch)
      prefetch->Run();
   return NULL;
}
//______________________________________________________________________________


IOEntireFile::IOEntireFile(XrdOucCacheIO &io, XrdOucCacheStats &stats, Cache & cache)
   : IO(io, stats, cache),
     m_prefetch(0)
{
   clLog()->Info(XrdCl::AppMsg, "IO::IO() [%p] %s", this, m_io.Path());

   std::string fname;
   m_cache.getFilePathFromURL(io.Path(), fname);

   m_prefetch = new Prefetch(io, fname, 0, io.FSize());

}

IOEntireFile::~IOEntireFile()
{}

bool IOEntireFile::ioActive()
{
   return m_prefetch->InitiateClose();
}

void IOEntireFile::StartPrefetch()
{
   pthread_t tid;
   XrdSysThread::Run(&tid, PrefetchRunner, (void *)(m_prefetch), 0, "XrdFileCache Prefetcher");

}


XrdOucCacheIO *IOEntireFile::Detach()
{
   m_statsGlobal.Add(m_prefetch->GetStats());

   XrdOucCacheIO * io = &m_io;

   delete m_prefetch;
   m_prefetch = 0;

   // This will delete us!
   m_cache.Detach(this);
   return io;
}

int IOEntireFile::Read (char *buff, long long off, int size)
{
   clLog()->Debug(XrdCl::AppMsg, "IO::Read() [%p]  %lld@%d %s", this, off, size, m_io.Path());

   ssize_t bytes_read = 0;
   ssize_t retval = 0;

   retval = m_prefetch->Read(buff, off, size);
   clLog()->Debug(XrdCl::AppMsg, "IO::Read() read from prefetch retval =  %d %s", retval, m_io.Path());
   if (retval > 0)
   {

      bytes_read += retval;
      buff += retval;
      size -= retval;
   }


   if ((size > 0))
   {
      clLog()->Debug(XrdCl::AppMsg, "IO::Read() missed %d bytes %s", size, m_io.Path());
      if (retval > 0) bytes_read += retval;
   }

   if (retval < 0)
   {
      clLog()->Error(XrdCl::AppMsg, "IO::Read(), origin bytes read %d %s", retval, m_io.Path());
   }

   return (retval < 0) ? retval : bytes_read;
}



/*
 * Perform a readv from the cache
 */
int IOEntireFile::ReadV (const XrdOucIOVec *readV, int n)
{
   clLog()->Warning(XrdCl::AppMsg, "IO::ReadV(), get %d requests %s", n, m_io.Path());


   return m_prefetch->ReadV(readV, n);
}
