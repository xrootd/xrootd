#ifndef __XRDMETRICS_HH__
#define __XRDMETRICS_HH__
/******************************************************************************/
/*                                                                            */
/*                         X r d M e t r i c s . h h                          */
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
/******************************************************************************/

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "XrdSys/XrdSysRAtomic.hh"

//-----------------------------------------------------------------------------
//! XrdMetrics: a lightweight, type-aware metrics facility whose instruments
//! can be serialized into the Prometheus text exposition format.
//!
//! Instruments (counters, gauges, histograms) are owned by an
//! XrdMetricsRegistry. They are obtained by reference and updated lock-free via
//! relaxed atomics on the hot path; the registry mutex is taken only when an
//! instrument is first created or when the registry is scraped.
//!
//! A metric "family" is identified by its name (e.g. xrootd_ops_read_total) and
//! may contain several series distinguished by label sets. The same name+labels
//! always map to the same instrument, so repeated lookups are cheap and safe.
//-----------------------------------------------------------------------------

//! Ordered list of (label, value) pairs attached to a metric series.
using XrdMetricsLabels = std::vector<std::pair<std::string, std::string>>;

//! A collector appends additional exposition text to its argument at scrape
//! time. It lets subsystems that keep their own counters (e.g. the XrdMonRoll
//! summary registry) contribute to /metrics without copying their state into
//! the registry. Must be thread-safe with respect to concurrent scrapes.
using XrdMetricsCollector = std::function<void(std::string&)>;

//! Readers for externally-backed series: the registry calls them at scrape
//! time to obtain the current value of a counter (kept elsewhere as the single
//! source of truth, e.g. an existing protocol counter read atomically).
using XrdMetricsU64Reader = std::function<unsigned long long()>;
using XrdMetricsDblReader = std::function<double()>;

enum class XrdMetricType {Counter, Gauge, Histogram};

/******************************************************************************/
/*                       X r d M e t r i c I n s t r                          */
/******************************************************************************/

//! Base class for a single metric series. Subclasses know how to render
//! themselves as one or more Prometheus exposition lines.

class XrdMetricInstr
{
public:

//! Append this series' exposition line(s) to out.
//!
//! @param out    output string to append to.
//! @param name   the metric family name.
//! @param labels pre-formatted label string, e.g. {k="v",...} or empty.

virtual void Serialize(std::string& out, const std::string& name,
                       const std::string& labels) const = 0;

         XrdMetricInstr() {}
virtual ~XrdMetricInstr() {}
};

/******************************************************************************/
/*                     X r d M e t r i c s C o u n t e r                      */
/******************************************************************************/

//! A monotonically increasing counter.

class XrdMetricsCounter : public XrdMetricInstr
{
public:

void     inc(uint64_t v = 1) {val += v;}

uint64_t value() const {return const_cast<RAtomic_uint64_t&>(val).load();}

void     Serialize(std::string& out, const std::string& name,
                   const std::string& labels) const override;

         XrdMetricsCounter() {}

private:
RAtomic_uint64_t val{0};
};

/******************************************************************************/
/*                       X r d M e t r i c s G a u g e                        */
/******************************************************************************/

//! A value that may go up or down (e.g. current connections, thread count).
//! Backed by std::atomic<double>; inc/dec use a compare-exchange loop since
//! std::atomic<double>::fetch_add is unavailable before C++20.

class XrdMetricsGauge : public XrdMetricInstr
{
public:

void     set(double v) {val.store(v, std::memory_order_relaxed);}

void     inc(double v = 1);

void     dec(double v = 1) {inc(-v);}

double   value() const {return val.load(std::memory_order_relaxed);}

void     Serialize(std::string& out, const std::string& name,
                   const std::string& labels) const override;

         XrdMetricsGauge() {}

private:
std::atomic<double> val{0.0};
};

/******************************************************************************/
/*                   X r d M e t r i c s H i s t o g r a m                    */
/******************************************************************************/

//! A cumulative histogram with fixed bucket upper bounds (le). Emits the
//! _bucket, _sum and _count series expected by Prometheus.

class XrdMetricsHistogram : public XrdMetricInstr
{
public:

void     observe(double v);

void     Serialize(std::string& out, const std::string& name,
                   const std::string& labels) const override;

//! @param bounds bucket upper bounds in ascending order (the implicit +Inf
//!               bucket is added automatically).

explicit XrdMetricsHistogram(std::vector<double> bounds);

private:
std::vector<double>           bound;   // upper bounds, ascending
std::vector<RAtomic_uint64_t> bcount;  // per-bucket cumulative counts (+Inf last)
RAtomic_uint64_t              count{0};
std::atomic<double>           sum{0.0};
};

/******************************************************************************/
/*                    X r d M e t r i c s R e g i s t r y                     */
/******************************************************************************/

//! Owns a set of metric families and serializes them on demand. A process-wide
//! instance is available via Default(); plugins should use that so that all
//! metrics land in the same scrape.

class XrdMetricsRegistry
{
public:

//! The process-wide registry shared by the server and all loaded plugins.
static XrdMetricsRegistry& Default();

//! Obtain (creating on first use) the counter for name+labels. The help text
//! is recorded the first time the family is seen.
XrdMetricsCounter&   Counter(const std::string& name, const std::string& help,
                             const XrdMetricsLabels& labels = {});

//! Obtain (creating on first use) the gauge for name+labels.
XrdMetricsGauge&     Gauge(const std::string& name, const std::string& help,
                           const XrdMetricsLabels& labels = {});

//! Obtain (creating on first use) the histogram for name+labels. The bucket
//! bounds are used only when the series is first created.
XrdMetricsHistogram& Histogram(const std::string& name, const std::string& help,
                               const std::vector<double>& bounds,
                               const XrdMetricsLabels& labels = {});

//! Add a counter series whose value is read from an external source at scrape
//! time, rather than stored in the registry. Use this to surface counters that
//! are owned and updated elsewhere (the existing source of truth) as typed
//! Prometheus series. Idempotent for a given name+labels.
void AddRefCounter(const std::string& name, const std::string& help,
                   const XrdMetricsLabels& labels, XrdMetricsU64Reader reader);

//! Like AddRefCounter but for a gauge (value may go up or down).
void AddRefGauge(const std::string& name, const std::string& help,
                 const XrdMetricsLabels& labels, XrdMetricsDblReader reader);

//! Register a collector invoked on every scrape after the registry's own
//! metrics have been rendered. Collectors are run in registration order.
void AddCollector(XrdMetricsCollector c);

//! Render every registered family into the Prometheus text exposition format,
//! then append the output of each registered collector.
//!
//! @param out string that receives the exposition text (cleared first).
//! @return    number of bytes written to out.
int  Scrape(std::string& out);

     XrdMetricsRegistry() {}
    ~XrdMetricsRegistry() {}

private:

struct Series
      {std::string                     labelStr; // pre-formatted, e.g. {k="v"}
       std::unique_ptr<XrdMetricInstr> instr;
      };

struct Family
      {XrdMetricType            type;
       std::string              help;
       std::map<std::string, Series> series;   // keyed by canonical label key
      };

XrdMetricInstr* GetOrAdd(const std::string& name, const std::string& help,
                         XrdMetricType type, const XrdMetricsLabels& labels,
                         const std::vector<double>* bounds);

void            AddRef(const std::string& name, const std::string& help,
                       XrdMetricType type, const XrdMetricsLabels& labels,
                       std::unique_ptr<XrdMetricInstr> inst);

std::mutex                       regMtx;
std::map<std::string, Family>    family;       // keyed by metric name
std::vector<XrdMetricsCollector> collectors;
};
#endif
