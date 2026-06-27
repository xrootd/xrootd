#ifndef __XRDMETRICS_FAMILY_HH__
#define __XRDMETRICS_FAMILY_HH__
/******************************************************************************/
/*                                                                            */
/*                   X r d M e t r i c s F a m i l y . h h                    */
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

#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "XrdMetrics/XrdMetricsInstrument.hh"
#include "XrdMetrics/XrdMetricsLabels.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"

//-----------------------------------------------------------------------------
//! A metric family: one name, one help string, one type, and a set of series
//! keyed by their variable label values. The family owns a two-tier child
//! cache. The first time a label combination is seen it is created under a
//! short write lock; thereafter the caller caches the returned handle and hits
//! it lock-free forever. Scraping snapshots child pointers under a brief read
//! lock and serializes outside it, so a scrape never blocks metric producers.
//-----------------------------------------------------------------------------

namespace XrdMetrics
{
/******************************************************************************/
/*                             I F a m i l y                                 */
/******************************************************************************/

//! Type-erased family interface so the registry can hold counters and gauges
//! together and drive any serializer over them.

class IFamily
{
public:
virtual ~IFamily() = default;
virtual void serialize(ISerializer& s) const = 0;
};

/******************************************************************************/
/*                        L a b e l e d F a m i l y                          */
/******************************************************************************/

//! @tparam Child  Counter, IntGauge or FloatGauge.

template <class Child>
class LabeledFamily : public IFamily
{
public:

//! @param fullName resolved name (prefix_subsystem_metric), embedded in every
//!                 series prefix.
//! @param ctx      immutable label context shared by all series.
//! @param help     family HELP text.
//! @param maxKids  cardinality cap; 0 means unlimited. Label combinations past
//!                 the cap fold into a single overflow series rather than
//!                 growing the series count without bound.
LabeledFamily(std::string fullName, LabelContext ctx, std::string help,
              std::size_t maxKids = 0)
             : name_(std::move(fullName)), help_(std::move(help)),
               ctx_(std::move(ctx)), maxKids_(maxKids) {}

//! Positional lookup; vals order matches the schema. Returns a stable handle
//! the caller should cache. This is the only locked step on the update path,
//! amortized to once per call site.
Child& withLabelValues(std::vector<std::string> vals)
{
   LabelValues key{std::move(vals)};

   // Normalize to the schema arity so the cached prefix and forEachLabel can
   // walk names and values in lockstep: missing values become empty, extras
   // are dropped. The argument is meant to be positional in schema order.
   key.v.resize(ctx_.schema.size());

   {std::shared_lock<std::shared_mutex> rd(mutex_);
    auto it = children_.find(key);
    if (it != children_.end()) return *it->second;
   }

   std::unique_lock<std::shared_mutex> wr(mutex_);
   auto it = children_.find(key);                       // re-check under write
   if (it != children_.end()) return *it->second;

   if (maxKids_ && children_.size() >= maxKids_) return overflow();

   auto child = std::make_unique<Child>(SeriesLabels(ctx_, name_, key));
   Child& ref = *child;
   children_.emplace(std::move(key), std::move(child));
   return ref;
}

//! Convenience for an unlabeled metric (a family with an empty schema).
Child& noLabels() { return withLabelValues({}); }

//! The full metric name (prefix_subsystem_metric).
const std::string& name() const noexcept { return name_; }

void serialize(ISerializer& s) const override
{
   s.beginFamily(name_, Child::kind(), help_);
   for (const Child* c : snapshot()) s.series(c->labels(), c->value());
   s.endFamily();
}

private:

//! Snapshot child pointers under a short read lock. Node-based unordered_map
//! keeps Child pointers stable across rehash, so serialization outside the
//! lock is safe.
std::vector<const Child*> snapshot() const
{
   std::shared_lock<std::shared_mutex> rd(mutex_);
   std::vector<const Child*> out;
   out.reserve(children_.size() + (overflow_ ? 1 : 0));
   for (auto& kv : children_) out.push_back(kv.second.get());
   if (overflow_) out.push_back(overflow_.get());
   return out;
}

//! The shared series that over-cap label combinations fold into. Built lazily
//! under the held write lock, labelled with a placeholder per schema slot.
Child& overflow()
{
   if (!overflow_)
      {LabelValues key;
       key.v.assign(ctx_.schema.size(), "__over_cardinality_limit__");
       overflow_ = std::make_unique<Child>(SeriesLabels(ctx_, name_, key));
      }
   return *overflow_;
}

std::string  name_;
std::string  help_;
LabelContext ctx_;
std::size_t  maxKids_;

mutable std::shared_mutex mutex_;
std::unordered_map<LabelValues, std::unique_ptr<Child>, LabelValuesHash> children_;
std::unique_ptr<Child> overflow_;
};
}
#endif
