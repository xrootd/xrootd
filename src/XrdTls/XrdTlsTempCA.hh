/******************************************************************************/
/*                                                                            */
/*               X r d T l s T e m p C A . h h                                */
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

#include <string>
#include <memory>

#include <openssl/x509.h>

#include "XrdSys/XrdSysPthread.hh"

// Forward dec'ls.
class XrdSysError;

/**
 * This class provides manages a "CA file" that is a concatenation of all the
 * CAs in a given CA directory.  This is useful in TLS contexts where, instead
 * of loading all CAs for each connection, we only want to load a single file.
 *
 * This will hand out the CA file directly, allowing external libraries (such
 * as libcurl) do the loading of CAs directly.
 *
 * Parsing those files is expensive -- a grid CA directory costs tens of MB of
 * heap once parsed -- so a pre-parsed X509_STORE covering the same CAs and CRLs
 * is maintained alongside them; see CAStore().
 */
class XrdTlsTempCA {
public:
    class TempCAGuard;

    /**
     * Set build_store when the caller intends to use CAStore().  Maintaining the
     * store costs tens of MB of resident memory, so callers that only need the
     * bundle filenames should leave it off.
     */
    XrdTlsTempCA(XrdSysError *log, std::string ca_dir, bool build_store = true);
    ~XrdTlsTempCA();

    /**
     * Returns true if object is valid, i.e. the CA and CRL bundles were generated,
     * and parsed into a store if one was asked for.  Failing to build a requested
     * store is fatal rather than recoverable: falling back to having every consumer
     * parse the bundles for itself is what the store exists to avoid.
     */
    bool IsValid() const {XrdSysMutexHelper lock(m_mutex);
                          return m_ca_file.get() && m_crl_file.get()
                              && (!m_build_store || m_ca_store.get());}

    /**
     * Returns the current location of the CA temp file.
     */
    std::string CAFilename() const {XrdSysMutexHelper lock(m_mutex); return m_ca_file ? *m_ca_file : "";}

    /**
     * Returns the current location of the CA temp file.
     */
    std::string CRLFilename() const {XrdSysMutexHelper lock(m_mutex); return m_crl_file ? *m_crl_file : "";}

    /**
     * Returns true if a valid CRL file has been found during the Maintenance thread execution
     * false otherwise
     */
    bool atLeastOneValidCRLFound() const {XrdSysMutexHelper lock(m_mutex); return m_atLeastOneCRLFound;}

    /**
     * Returns the CA and CRL contents pre-parsed into a single X509_STORE, rebuilt
     * once per maintenance cycle.  An X509_STORE is reference counted and internally
     * locked by OpenSSL, so a single instance may be shared across any number of
     * concurrent TLS handshakes -- e.g. via SSL_CTX_set1_cert_store() -- instead of
     * having every connection parse the CA and CRL bundles for itself.
     *
     * The returned reference keeps the store alive for as long as the caller holds
     * it, so a maintenance cycle may publish a replacement without disturbing the
     * TLS sessions still using the previous one.
     *
     * Only ever null before the first successful maintenance run, which IsValid()
     * reports on; a maintenance run that cannot build a store keeps the previous
     * one rather than withdrawing it.  Callers should treat a null store as a hard
     * error, not as a cue to load the bundles themselves.
     */
    std::shared_ptr<X509_STORE> CAStore() const {XrdSysMutexHelper lock(m_mutex); return m_ca_store;}

    /**
     * Manages the temporary file associated with the curl handle
     */
    class TempCAGuard {
    public:
        static std::unique_ptr<TempCAGuard> create(XrdSysError &, const std::string &ca_tmp_dir);

    int getCAFD() const {return m_ca_fd;}
    std::string getCAFilename() const {return m_ca_fname;}

    int getCRLFD() const {return m_crl_fd;}
    std::string getCRLFilename() const {return m_crl_fname;}

    /**
     * Move temporary file to the permanent location.
     */
    bool commit();

    TempCAGuard(const TempCAGuard &) = delete;

    ~TempCAGuard();

    private:
        TempCAGuard(int ca_fd, int crl_fd, const std::string &ca_tmp_dir, const std::string &ca_fname, const std::string &crl_fname);

        int m_ca_fd{-1};
        int m_crl_fd{-1};
        std::string m_ca_tmp_dir;
        std::string m_ca_fname;
        std::string m_crl_fname;
    };


private:
    /** 
     * Run the CA maintenance routines.
     * This will go through the CA directory, concatenate the
     * CA contents into a single PEM file, and delete the prior
     * copy of the concatenated CA certs.
     */
    bool Maintenance();

    /**
     * Parse the freshly-generated CA and CRL bundles into a single X509_STORE.
     * The CRL bundle is only added -- and CRL checking only enabled -- when it
     * holds at least one valid CRL, mirroring the behaviour of the callers that
     * hand these files to libcurl directly.
     *
     * Returns nullptr on failure, in which case the caller publishes nothing and
     * leaves consumers on the previously loaded store.
     */
    std::shared_ptr<X509_STORE> BuildCAStore(const std::string &ca_fname,
                                             const std::string &crl_fname,
                                             bool use_crls);

    /**
     * Thread managing the invocation of the CA maintenance routines
     */
    static void *MaintenanceThread(void *myself_raw);

    /**
     * Read and write ends of a pipe to communicate between the parent
     * object and the maintenance thread.
     */
    int m_maintenance_pipe_r{-1};
    int m_maintenance_pipe_w{-1};
    int m_maintenance_thread_pipe_r{-1};
    int m_maintenance_thread_pipe_w{-1};
    XrdSysError &m_log;
    const std::string m_ca_dir;
    const bool m_build_store;
        // Guards the published state below; taken once per consumer request (not
        // per I/O), so the critical sections are deliberately kept to a pointer copy.
    mutable XrdSysMutex m_mutex;
    std::shared_ptr<std::string> m_ca_file;
    std::shared_ptr<std::string> m_crl_file;
    std::shared_ptr<X509_STORE> m_ca_store;
    bool m_atLeastOneCRLFound = false;

        // After success, how long to wait until the next CA reload.
    static constexpr unsigned m_update_interval = 900;
        // After failure, how long to wait until the next CA reload.
    static constexpr unsigned m_update_interval_failure = 10;
};
