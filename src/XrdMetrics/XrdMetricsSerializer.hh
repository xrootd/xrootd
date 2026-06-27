#ifndef __XRDMETRICS_SERIALIZER_HH__
#define __XRDMETRICS_SERIALIZER_HH__
/******************************************************************************/
/*                                                                            */
/*              X r d M e t r i c s S e r i a l i z e r . h h                 */
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

#include <cstdint>
#include <string>
#include <vector>

#include "XrdMetrics/XrdMetricsInstrument.hh"   // MetricKind
#include "XrdMetrics/XrdMetricsLabels.hh"        // SeriesLabels

//-----------------------------------------------------------------------------
//! The serialization seam. A family is serialized by accepting an ISerializer
//! and calling beginFamily, one series() per child (with the value's static
//! type preserved so a uint64 counter is never coerced through double), then
//! endFamily. Adding an output format (OTel JSON, XRootD XML) is a new
//! ISerializer subclass and never a change to families or instruments.
//-----------------------------------------------------------------------------

namespace XrdMetrics
{
//! A histogram series' scrape-time snapshot. bounds are the upper bounds;
//! cumulative has bounds.size()+1 entries (the last is the +Inf bucket and
//! equals the total observation count).
struct HistogramData
{
const std::vector<double>&        bounds;
const std::vector<std::uint64_t>& cumulative;
double                            sum;
};

/******************************************************************************/
/*                          I S e r i a l i z e r                            */
/******************************************************************************/

class ISerializer
{
public:
virtual ~ISerializer() = default;

virtual void beginFamily(const std::string& /*fullName*/, MetricKind /*kind*/,
                         const std::string& /*help*/) {}
virtual void endFamily() {}

//! Typed series emit; the family's static Child type selects the overload.
virtual void series(const SeriesLabels& labels, std::uint64_t value) = 0;
virtual void series(const SeriesLabels& labels, std::int64_t  value) = 0;
virtual void series(const SeriesLabels& labels, double        value) = 0;

//! A histogram series (emits _bucket/_sum/_count in the text format).
virtual void histogram(const std::string& fullName, const SeriesLabels& labels,
                       const HistogramData& data) = 0;
};

/******************************************************************************/
/*             P r o m e t h e u s T e x t S e r i a l i z e r               */
/******************************************************************************/

//! Emits the Prometheus text exposition format into a caller-owned string. The
//! same buffer can be reused across scrapes (the registry traversal clears
//! nothing, so the caller clears once before serialize()), giving an
//! allocation-free steady state. Per series the work is a memcpy of the cached
//! prefix plus one number append and a newline.

class PrometheusTextSerializer : public ISerializer
{
public:
explicit PrometheusTextSerializer(std::string& out) : out_(out) {}

void beginFamily(const std::string& name, MetricKind kind,
                 const std::string& help) override;

void series(const SeriesLabels& labels, std::uint64_t value) override;
void series(const SeriesLabels& labels, std::int64_t  value) override;
void series(const SeriesLabels& labels, double        value) override;

void histogram(const std::string& fullName, const SeriesLabels& labels,
               const HistogramData& data) override;

private:
template <class T> void emit(const SeriesLabels& labels, T value);

std::string& out_;
};
}
#endif
