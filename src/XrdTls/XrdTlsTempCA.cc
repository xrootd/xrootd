/******************************************************************************/
/*                                                                            */
/*               X r d T l s T e m p C A . c c                                */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Brian Bockelman                                              */
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


#include <cstdlib>
#include <fcntl.h>
#include <dirent.h>
#include <sys/poll.h>

#include <unordered_set>
#include <memory>

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdCrypto/XrdCryptoX509Chain.hh"
#include "XrdCrypto/XrdCryptosslAux.hh"
#include "XrdCrypto/XrdCryptosslX509Crl.hh"
#include "XrdVersion.hh"

#include "XrdTlsTempCA.hh"

#include <sstream>
#include <vector>

namespace {
    
typedef std::unique_ptr<FILE, decltype(&fclose)> file_smart_ptr;


static uint64_t monotonic_time_s() {
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  return tp.tv_sec + (tp.tv_nsec >= 500000000);
}


class CASet {
public:
    CASet(int output_fd, XrdSysError &err)
    : m_log(err),
      m_output_fd(output_fd)
    {}

    /**
     * Given an open file descriptor pointing to
     * a file potentially containing a CA, process it
     * for PEM-formatted entries.  If a new, unique CA
     * is found, then it is written into the current
     * tempfile.
     *
     * The fname argument is used solely for debugging.
     * 
     * Returns true on success.
     */
    bool processFile(file_smart_ptr &fd, const std::string &fname);

private:
    XrdSysError &m_log;

        // Grid CA directories tend to keep everything in triplicate;
        // we keep a unique hash of all known CAs so we write out each
        // one only once.
    std::unordered_set<std::string> m_known_cas;
    const int m_output_fd;
};


bool
CASet::processFile(file_smart_ptr &fp, const std::string &fname)
{
    XrdCryptoX509Chain chain;
    // Not checking return value here; function returns `0` on error and
    // if no certificate is found.
    XrdCryptosslX509ParseFile(fp.get(), &chain, fname.c_str());

    auto ca = chain.Begin();
    file_smart_ptr outputfp(fdopen(XrdSysFD_Dup(m_output_fd), "w"), &fclose);
    if (!outputfp.get()) {
        m_log.Emsg("CAset", "Failed to reopen file for output", fname.c_str());
        chain.Cleanup();
        return false;
    }
    while (ca) {
        auto hash_ptr = ca->SubjectHash();
        if (!hash_ptr) {
            continue;
        }
        auto iter = m_known_cas.find(hash_ptr);
        if (iter != m_known_cas.end()) {
            //m_log.Emsg("CAset", "Skipping known CA with hash", fname.c_str(), hash_ptr);
            ca = chain.Next();
            continue;
        }
        //m_log.Emsg("CAset", "New CA with hash", fname.c_str(), hash_ptr);
        m_known_cas.insert(hash_ptr);

        if (XrdCryptosslX509ToFile(ca, outputfp.get(), fname.c_str())) {
            m_log.Emsg("CAset", "Failed to write out CA", fname.c_str());
            chain.Cleanup();
            return false;
        }
        ca = chain.Next();
    }
    fflush(outputfp.get());
    chain.Cleanup();

    return true;
}


class CRLSet {
public:
    CRLSet(int output_fd, XrdSysError &err)
    : m_log(err),
      m_output_fd(output_fd)
    {}

    /**
     * Given an open file descriptor pointing to
     * a file potentially containing a CRL, process it
     * for PEM-formatted entries.  If a new, unique CRL
     * is found, then it is written into the current
     * tempfile.
     *
     * The fname argument is used solely for debugging.
     *
     * Returns true on success.
     */
    bool processFile(file_smart_ptr &fd, const std::string &fname);

private:
    XrdSysError &m_log;

        // Grid CA directories tend to keep everything in triplicate;
        // we keep a unique hash of all known CRLs so we write out each
        // one only once.
    std::unordered_set<std::string> m_known_crls;
    const int m_output_fd;
};


bool
CRLSet::processFile(file_smart_ptr &fp, const std::string &fname)
{
    file_smart_ptr outputfp(fdopen(dup(m_output_fd), "w"), &fclose);
    if (!outputfp.get()) {
        m_log.Emsg("CAset", "Failed to reopen file for output", fname.c_str());
        return false;
    }

    // Assume we can safely ignore a failure to parse; we load every file in
    // the directory and that will naturally include a number of non-CRL files.
    for (std::unique_ptr<XrdCryptosslX509Crl> xrd_crl(new XrdCryptosslX509Crl(fp.get(), fname.c_str()));
         xrd_crl->IsValid();
         xrd_crl = std::unique_ptr<XrdCryptosslX509Crl>(new XrdCryptosslX509Crl(fp.get(), fname.c_str())))
    {
        auto hash_ptr = xrd_crl->IssuerHash(1);
        if (!hash_ptr) {
            continue;
        }
        auto iter = m_known_crls.find(hash_ptr);
        if (iter != m_known_crls.end()) {
            //m_log.Emsg("CRLset", "Skipping known CRL with hash", fname.c_str(), hash_ptr);
            continue;
        }
        //m_log.Emsg("CRLset", "New CRL with hash", fname.c_str(), hash_ptr);
        m_known_crls.insert(hash_ptr);

        if (!xrd_crl->ToFile(outputfp.get())) {
            m_log.Emsg("CRLset", "Failed to write out CRL", fname.c_str());
            fflush(outputfp.get());
            return false;
        }
    }
    fflush(outputfp.get());

    return true;
}

}


std::unique_ptr<XrdTlsTempCA::TempCAGuard>
XrdTlsTempCA::TempCAGuard::create(XrdSysError &err, const std::string &ca_tmp_dir) {

    if (-1 == mkdir(ca_tmp_dir.c_str(), S_IRWXU) && errno != EEXIST) {
        err.Emsg("TempCA", "Unable to create CA temp directory", ca_tmp_dir.c_str(), strerror(errno));
    }

    std::stringstream ss;
    ss << ca_tmp_dir << "/ca_file.XXXXXX.pem";
    std::vector<char> ca_fname;
    ca_fname.resize(ss.str().size() + 1);
    memcpy(ca_fname.data(), ss.str().c_str(), ss.str().size());

    int ca_fd = mkstemps(ca_fname.data(), 4);
    if (ca_fd < 0) {
        err.Emsg("TempCA", "Failed to create temp file:", strerror(errno));
        return std::unique_ptr<TempCAGuard>();
    }

    std::stringstream ss2;
    ss2 << ca_tmp_dir << "/crl_file.XXXXXX.pem";
    std::vector<char> crl_fname;
    crl_fname.resize(ss2.str().size() + 1);
    memcpy(crl_fname.data(), ss2.str().c_str(), ss2.str().size());

    int crl_fd = mkstemps(crl_fname.data(), 4);
    if (crl_fd < 0) {
        err.Emsg("TempCA", "Failed to create temp file:", strerror(errno));
        return std::unique_ptr<TempCAGuard>();
    }
    return std::unique_ptr<TempCAGuard>(new TempCAGuard(ca_fd, crl_fd, ca_tmp_dir, ca_fname.data(), crl_fname.data()));
}


XrdTlsTempCA::TempCAGuard::~TempCAGuard() {
    if (m_ca_fd >= 0) {
        unlink(m_ca_fname.c_str());
        close(m_ca_fd);
    }
    if (m_crl_fd >= 0) {
        unlink(m_crl_fname.c_str());
        close(m_crl_fd);
    }
}


bool
XrdTlsTempCA::TempCAGuard::commit() {
    if (m_ca_fd < 0 || m_ca_tmp_dir.empty()) {return false;}
    close(m_ca_fd);
    m_ca_fd = -1;
    std::string ca_fname = m_ca_tmp_dir + "/ca_file.pem";
    if (-1 == rename(m_ca_fname.c_str(), ca_fname.c_str())) {
        return false;
    }
    m_ca_fname = ca_fname;

    if (m_crl_fd < 0 || m_ca_tmp_dir.empty()) {return false;}
    close(m_crl_fd);
    m_crl_fd = -1;
    std::string crl_fname = m_ca_tmp_dir + "/crl_file.pem";
    if (-1 == rename(m_crl_fname.c_str(), crl_fname.c_str())) {
        return false;
    }
    m_crl_fname = crl_fname;

    return true;
}


XrdTlsTempCA::TempCAGuard::TempCAGuard(int ca_fd, int crl_fd, const std::string &ca_tmp_dir, const std::string &ca_fname, const std::string &crl_fname)
    : m_ca_fd(ca_fd), m_crl_fd(crl_fd), m_ca_tmp_dir(ca_tmp_dir), m_ca_fname(ca_fname), m_crl_fname(crl_fname)
    {}


XrdTlsTempCA::XrdTlsTempCA(XrdSysError *err, std::string ca_dir)
    : m_log(*err),
      m_ca_dir(ca_dir)
{
    // Setup communication pipes; we write one byte to the child to tell it to shutdown;
    // it'll write one byte back to acknowledge before our destructor exits.
    int pipes[2];
    if (-1 == XrdSysFD_Pipe(pipes)) {
        m_log.Emsg("XrdTlsTempCA", "Failed to create communication pipes", strerror(errno));
        return;
    }
    m_maintenance_pipe_r = pipes[0];
    m_maintenance_pipe_w = pipes[1];
    if (-1 == XrdSysFD_Pipe(pipes)) {
        m_log.Emsg("XrdTlsTempCA", "Failed to create communication pipes", strerror(errno));
        return;
    }
    m_maintenance_thread_pipe_r = pipes[0];
    m_maintenance_thread_pipe_w = pipes[1];
    if (!Maintenance()) {return;}

    pthread_t tid;
    auto rc = XrdSysThread::Run(&tid, XrdTlsTempCA::MaintenanceThread,
                                static_cast<void*>(this), 0, "CA/CRL refresh");
    if (rc) {
        m_log.Emsg("XrdTlsTempCA", "Failed to launch CA monitoring thread");
        m_ca_file.reset();
        m_crl_file.reset();
    }
}


XrdTlsTempCA::~XrdTlsTempCA()
{
    char indicator[1];
    if (m_maintenance_pipe_w >= 0) {
        indicator[0] = '1';
        int rval;
        do {rval = write(m_maintenance_pipe_w, indicator, 1);} while (rval != -1 || errno == EINTR);
        if (m_maintenance_thread_pipe_r >= 0) {
            do {rval = read(m_maintenance_thread_pipe_r, indicator, 1);} while (rval != -1 || errno == EINTR);
            close(m_maintenance_thread_pipe_r);
            close(m_maintenance_thread_pipe_w);
        }
        close(m_maintenance_pipe_r);
        close(m_maintenance_pipe_w);
    }
}


bool
XrdTlsTempCA::Maintenance()
{
    m_log.Emsg("TempCA", "Reloading the list of CAs and CRLs in directory");

    auto adminpath = getenv("XRDADMINPATH");
    if (!adminpath) {
        m_log.Emsg("TempCA", "Admin path is not set!");
        return false;
    }
    std::string ca_tmp_dir = std::string(adminpath) + "/.xrdtls";

    std::unique_ptr<TempCAGuard> new_file(TempCAGuard::create(m_log, ca_tmp_dir));
    if (!new_file) {
        m_log.Emsg("TempCA", "Failed to create a new temp CA / CRL file");
        return false;
    }
    CASet ca_builder(new_file->getCAFD(), m_log);
    CRLSet crl_builder(new_file->getCRLFD(), m_log);

    int fddir = XrdSysFD_Open(m_ca_dir.c_str(), O_DIRECTORY);
    if (fddir < 0) {
        m_log.Emsg("TempCA", "Failed to open the CA directory", m_ca_dir.c_str());
        return false;
    }

    DIR *dirp = fdopendir(fddir);
    if (!dirp) {
        m_log.Emsg("Maintenance", "Failed to allocate a directory pointer");
        return false;
    }

    struct dirent *result;
    errno = 0;
    while ((result = readdir(dirp))) {
        //m_log.Emsg("Will parse file for CA certificates", result->d_name);
        if (result->d_name[0] == '.') {continue;}
        if (result->d_type != DT_REG)
           {if (result->d_type != DT_UNKNOWN && result->d_type != DT_LNK)
               continue;
            struct stat Stat;
            if (fstatat(fddir, result->d_name, &Stat, 0))
               {m_log.Emsg("Maintenance", "Failed to stat certificate file",
                           result->d_name, strerror(errno));
                continue;
               }
            if (!S_ISREG(Stat.st_mode)) continue;
           }
        int fd = XrdSysFD_Openat(fddir, result->d_name, O_RDONLY);
        if (fd < 0) {
            m_log.Emsg("Maintenance", "Failed to open certificate file", result->d_name, strerror(errno));
            closedir(dirp);
            return false;
        }
        file_smart_ptr fp(fdopen(fd, "r"), &fclose);

        if (!ca_builder.processFile(fp, result->d_name)) {
            m_log.Emsg("Maintenance", "Failed to process file for CAs", result->d_name);
        }
        rewind(fp.get());
        if (!crl_builder.processFile(fp, result->d_name)) {
            m_log.Emsg("Maintenance", "Failed to process file for CRLs", result->d_name);
        }
        errno = 0;
    }
    if (errno) {
        m_log.Emsg("Maintenance", "Failure during readdir", strerror(errno));
        closedir(dirp);
        return false;
    }
    closedir(dirp);

    if (!new_file->commit()) {
        m_log.Emsg("Mainteance", "Failed to finalize new CA / CRL files");
        return false;
    }
    //m_log.Emsg("Maintenance", "Successfully created CA and CRL files", new_file->getCAFilename().c_str(),
    //    new_file->getCRLFilename().c_str());
    m_ca_file.reset(new std::string(new_file->getCAFilename()));
    m_crl_file.reset(new std::string(new_file->getCRLFilename()));
    return true;
}


void *XrdTlsTempCA::MaintenanceThread(void *myself_raw)
{
   auto myself = static_cast<XrdTlsTempCA *>(myself_raw);

   auto now = monotonic_time_s();
   auto next_update = now + m_update_interval;
   while (true) {
       now = monotonic_time_s();
       auto remaining = next_update - now;
       struct pollfd fds;
       fds.fd = myself->m_maintenance_pipe_r;
       fds.events = POLLIN;
       auto rval = poll(&fds, 1, remaining*1000);
       if (rval == -1) {
           if (rval == EINTR) continue;
           else break;
       } else if (rval == 0) { // timeout!  Let's run maintenance.
           if (myself->Maintenance()) {
               next_update = monotonic_time_s() + m_update_interval;
           } else {
               next_update = monotonic_time_s() + m_update_interval_failure;
           }
       } else { // FD ready; let's shutdown
           if (fds.revents & POLLIN) {
               char indicator[1];
               do {rval = read(myself->m_maintenance_pipe_r, indicator, 1);} while (rval != -1 || errno == EINTR);
           }
       }
   }
   if (errno) {
       myself->m_log.Emsg("Maintenance", "Failed to poll for events from parent object");
   }
   char indicator = '1';
   int rval;
   do {rval = write(myself->m_maintenance_thread_pipe_w, &indicator, 1);} while (rval != -1 || errno == EINTR);

   return nullptr;
}
