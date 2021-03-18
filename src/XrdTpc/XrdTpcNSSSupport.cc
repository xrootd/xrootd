/******************************************************************************/
/*                                                                            */
/*               X r d T p c N S S S u p p o r t . c c                        */
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
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdCrypto/XrdCryptoX509Chain.hh"
#include "XrdCrypto/XrdCryptosslAux.hh"
#include "XrdVersion.hh"

#include "XrdTpcNSSSupport.hh"

namespace {
    
typedef std::unique_ptr<FILE, decltype(&fclose)> file_smart_ptr;
 
XrdVERSIONINFODEF(g_ver, cryptoloader, XrdVNUMBER, XrdVERSION);

class CASet {
public:
    CASet(int output_fd, XrdSysError &err, void *x509_to_file_func, void *file_to_x509_func)
    : m_log(err),
      m_output_fd(output_fd),
      m_x509_to_file_func(reinterpret_cast<decltype(m_x509_to_file_func)>(x509_to_file_func)),
      m_file_to_x509_func(reinterpret_cast<decltype(m_file_to_x509_func)>(file_to_x509_func))
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

    decltype(&XrdCryptosslX509ToFile) m_x509_to_file_func;
    int (*m_file_to_x509_func)(FILE *file, XrdCryptoX509Chain *c);
};


bool
CASet::processFile(file_smart_ptr &fp, const std::string &fname)
{
    XrdCryptoX509Chain chain;
    // Not checking return value here; function returns `0` on error and
    // if no certificate is found.
    (*m_file_to_x509_func)(fp.get(), &chain);

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

        if ((*m_x509_to_file_func)(ca, outputfp)) {
            m_log.Emsg("CAset", "Failed to write out CA", fname.c_str());
            return false;
        }
        ca = chain.Next();
    }
    fflush(outputfp);

    return true;
}

}


using namespace TPC;


std::unique_ptr<XrdTpcNSSSupport::TempCAGuard>
XrdTpcNSSSupport::TempCAGuard::create(XrdSysError &err) {
    char fname[] = "/tmp/xrootd_curl_nss_ca.XXXXXX.pem";
    int fd = mkstemps(fname, 4);
    if (fd < 0) {
        err.Emsg("NSSSupport", "Failed to create temp file:", strerror(errno));
        return std::unique_ptr<TempCAGuard>();
    }
    return std::unique_ptr<TempCAGuard>(new TempCAGuard(fd, fname));
}


XrdTpcNSSSupport::TempCAGuard::~TempCAGuard() {
    if (m_fd >= 0) {
        unlink(m_fname.c_str());
        close(m_fd);
    }
}


XrdTpcNSSSupport::TempCAGuard::TempCAGuard(int fd, const std::string &fname)
    : m_fd(fd), m_fname(fname)
    {}


XrdTpcNSSSupport::XrdTpcNSSSupport(XrdSysError *err, std::string ca_dir)
    : m_log(*err),
      m_ca_dir(ca_dir),
      m_plugin(err, &g_ver, "Crypto Support", "libXrdCryptossl.so")
{
    if (!(m_x509_to_file_func = m_plugin.Resolve("XrdCryptosslX509ToFile")) ||
        !(m_file_to_x509_func = m_plugin.Resolve("XrdCryptosslX509ParseFile")))
    {
        return;
    }

    Maintenance();
}


bool
XrdTpcNSSSupport::NeedsNSSHack()
{
    const auto *version_info = curl_version_info(CURLVERSION_NOW);

    if (!version_info->ssl_version) {return true;}
    std::string ssl_version(version_info->ssl_version);

    auto iter = ssl_version.find("NSS");
    return iter != std::string::npos;
}


bool
XrdTpcNSSSupport::Maintenance()
{
    m_log.Emsg("XrdTpc", "Starting NSS leak workaround maintenance");

    std::unique_ptr<TempCAGuard> new_file(TempCAGuard::create(m_log));
    if (!new_file) {
        m_log.Emsg("XrdTpc", "Failed to create a new temp CA file");
        return false;
    }
    CASet builder(new_file->getFD(), m_log, m_x509_to_file_func, m_file_to_x509_func);

    int fddir = open(m_ca_dir.c_str(), O_DIRECTORY);
    if (fddir < 0) {
        m_log.Emsg("XrdTpc", "Failed to open the CA directory", m_ca_dir.c_str());
        return false;
    }

    auto dirp = fdopendir(fddir);
    if (!dirp) {
        m_log.Emsg("XrdTpc", "Failed to allocate a directory pointer");
        return false;
    }

    struct dirent ent, *result;
    bool got_eos = false;
    while (!readdir_r(dirp, &ent, &result)) {
        if (result == nullptr) {
            got_eos = true;
            break;
        }
        //m_log.Emsg("Will parse file for CA certificates", result->d_name);
        if (result->d_type != DT_REG && result->d_type != DT_LNK) {continue;}
        if (result->d_name[0] == '.') {continue;}
        int fd = openat(fddir, result->d_name, O_RDONLY);
        if (fd < 0) {
            m_log.Emsg("XrdTpc", "Failed to open certificate file", result->d_name, strerror(errno));
            return false;
        }
        file_smart_ptr fp(fdopen(fd, "r"), &fclose);

        if (!builder.processFile(fp, result->d_name)) {
            m_log.Emsg("XrdTpc", "Failed to process file", result->d_name);
        }
    }
    closedir(dirp);
    if (!got_eos) {
        m_log.Emsg("XrdTpc", "Failure during readdir");
        return false;
    }

    m_next_update = time(NULL) + 900;
    m_ca_file.reset(new_file.release());
    return true;
}


bool
XrdTpcNSSSupport::NeedsMaintenance()
{
    return time(NULL) > m_next_update;
}


std::shared_ptr<XrdTpcNSSSupport::TempCAGuard>
XrdTpcNSSSupport::ConfigureCurl(CURL * curl)
{
    if (NeedsMaintenance()) {
        Maintenance();
    }

    if (IsValid()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, m_ca_file->getFilename().c_str());
        return m_ca_file;
    }

    return std::shared_ptr<TempCAGuard>();
}
