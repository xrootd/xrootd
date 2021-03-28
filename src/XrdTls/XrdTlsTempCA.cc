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


#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>

#include <unordered_set>
#include <memory>

#include <curl/curl.h>

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdCrypto/XrdCryptoX509Chain.hh"
#include "XrdCrypto/XrdCryptosslAux.hh"
#include "XrdCrypto/XrdCryptosslX509Crl.hh"
#include "XrdVersion.hh"

#include "XrdTlsTempCA.hh"

namespace {
    
typedef std::unique_ptr<FILE, decltype(&fclose)> file_smart_ptr;
 
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
    // Note we purposely leak the outputfp here; we are just borrowing the handle.
    FILE *outputfp = fdopen(m_output_fd, "w");
    if (!outputfp) {
        m_log.Emsg("CAset", "Failed to reopen file for output", fname.c_str());
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

        if (XrdCryptosslX509ToFile(ca, outputfp, fname.c_str())) {
            m_log.Emsg("CAset", "Failed to write out CA", fname.c_str());
            return false;
        }
        ca = chain.Next();
    }
    fflush(outputfp);

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
    // Note we purposely leak the outputfp here; we are just borrowing the handle.
    FILE *outputfp = fdopen(m_output_fd, "w");
    if (!outputfp) {
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

        if (!xrd_crl->ToFile(outputfp)) {
            m_log.Emsg("CRLset", "Failed to write out CRL", fname.c_str());
            fflush(outputfp);
            return false;
        }
    }
    fflush(outputfp);

    return true;
}

}


using namespace XrdTls;


std::unique_ptr<XrdTlsTempCA::TempCAGuard>
XrdTlsTempCA::TempCAGuard::create(XrdSysError &err) {
    char ca_fname[] = "/tmp/xrootd_ca_file.XXXXXX.pem";
    int ca_fd = mkstemps(ca_fname, 4);
    if (ca_fd < 0) {
        err.Emsg("TempCA", "Failed to create temp file:", strerror(errno));
        return std::unique_ptr<TempCAGuard>();
    }
    char crl_fname[] = "/tmp/xrootd_crl_file.XXXXXX.pem";
    int crl_fd = mkstemps(crl_fname, 4);
    if (crl_fd < 0) {
        err.Emsg("TempCA", "Failed to create temp file:", strerror(errno));
        return std::unique_ptr<TempCAGuard>();
    }
    return std::unique_ptr<TempCAGuard>(new TempCAGuard(ca_fd, crl_fd, ca_fname, crl_fname));
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


XrdTlsTempCA::TempCAGuard::TempCAGuard(int ca_fd, int crl_fd, const std::string &ca_fname, const std::string &crl_fname)
    : m_ca_fd(ca_fd), m_crl_fd(crl_fd), m_ca_fname(ca_fname), m_crl_fname(crl_fname)
    {}


XrdTlsTempCA::XrdTlsTempCA(XrdSysError *err, std::string ca_dir)
    : m_log(*err),
      m_ca_dir(ca_dir)
{
    Maintenance();
}


bool
XrdTlsTempCA::Maintenance()
{
    m_log.Emsg("TempCA", "Reloading the list of CAs and CRLs in directory");

    std::unique_ptr<TempCAGuard> new_file(TempCAGuard::create(m_log));
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
    while ((result = readdir(dirp))) {
        //m_log.Emsg("Will parse file for CA certificates", result->d_name);
        if (result->d_type != DT_REG && result->d_type != DT_LNK) {continue;}
        if (result->d_name[0] == '.') {continue;}
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
    }
    if (errno) {
        m_log.Emsg("Maintenance", "Failure during readdir", strerror(errno));
        closedir(dirp);
        return false;
    }
    closedir(dirp);

    m_next_update.store(time(NULL) + 900, std::memory_order_relaxed);
    m_ca_file.reset(new_file.release());
    return true;
}


bool
XrdTlsTempCA::NeedsMaintenance()
{
    return time(NULL) > m_next_update.load(std::memory_order_relaxed);
}


std::shared_ptr<XrdTlsTempCA::TempCAGuard>
XrdTlsTempCA::getHandle()
{
    if (NeedsMaintenance()) {
        Maintenance();
    }

    return m_ca_file;
}
