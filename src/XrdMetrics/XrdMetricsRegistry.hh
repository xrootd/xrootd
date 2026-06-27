#ifndef __XRDMETRICS_REGISTRY_HH__
#define __XRDMETRICS_REGISTRY_HH__
/******************************************************************************/
/*                                                                            */
/*                 X r d M e t r i c s R e g i s t r y . h h                  */
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
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "XrdMetrics/XrdMetricsFamily.hh"
#include "XrdMetrics/XrdMetricsLabels.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"

//-----------------------------------------------------------------------------
//! The three-level hierarchy: Registry owns MetricGroups, a MetricGroup owns
//! Families, a Family owns series. Naming context flows down at registration
//! time (the registry's prefix and the group's subsystem are resolved into each
//! family's full name once, when the family is created, and baked into the
//! cached series prefixes); iteration flows down at scrape time.
//!
//! Lifetime is strictly nested and that is what keeps the back-pointers valid:
//! Registry outlives groups outlive families outlive series. The registry's
//! global const labels must be frozen before the first family is created, since
//! they are baked into the text prefixes.
//-----------------------------------------------------------------------------

namespace XrdMetrics
{
class Registry;   // referenced by MetricGroup; defined below

/******************************************************************************/
/*                           M e t r i c G r o u p                           */
/******************************************************************************/

//! A named subsystem (e.g. "scheduler", "ops") that owns a set of families and
//! is the factory injecting the resolved full name and label context downward.

class MetricGroup
{
public:
MetricGroup(Registry& reg, std::string subsystem)
           : reg_(reg), subsystem_(std::move(subsystem)) {}

//! Create a counter family. varLabels are the variable label names (schema);
//! constLabels are fixed for every series of the family. maxKids caps series
//! cardinality (0 = unlimited). Throws std::invalid_argument on an invalid
//! metric or label name.
LabeledFamily<Counter>&   counter(const std::string& name,
                                  std::vector<std::string> varLabels = {},
                                  std::vector<ConstLabel> constLabels = {},
                                  std::string help = {}, std::size_t maxKids = 0);

LabeledFamily<IntGauge>&  intGauge(const std::string& name,
                                  std::vector<std::string> varLabels = {},
                                  std::vector<ConstLabel> constLabels = {},
                                  std::string help = {}, std::size_t maxKids = 0);

LabeledFamily<FloatGauge>& floatGauge(const std::string& name,
                                  std::vector<std::string> varLabels = {},
                                  std::vector<ConstLabel> constLabels = {},
                                  std::string help = {}, std::size_t maxKids = 0);

const std::string& subsystem() const noexcept { return subsystem_; }

void serialize(ISerializer& s) const
{
   std::vector<const IFamily*> snap;
   {std::shared_lock<std::shared_mutex> rd(mutex_);
    snap.reserve(families_.size());
    for (auto& f : families_) snap.push_back(f.get());
   }
   for (auto* f : snap) f->serialize(s);
}

private:

template <class Child>
LabeledFamily<Child>& add(const std::string& name,
                          std::vector<std::string> varNames,
                          std::vector<ConstLabel> constLabels,
                          std::string help, std::size_t maxKids);

Registry&   reg_;
std::string subsystem_;

mutable std::shared_mutex mutex_;
std::vector<std::unique_ptr<IFamily>> families_;
};

/******************************************************************************/
/*                              R e g i s t r y                              */
/******************************************************************************/

//! Owns the global prefix, the frozen global const labels, and the groups.

class Registry
{
public:

//! @param prefix       leading name component for every metric (e.g. "xrootd").
//! @param globalLabels const labels added to every series. Reserve these for
//!                     things the Prometheus server cannot know (an XRootD
//!                     instance name); instance/job are usually set at scrape
//!                     time. They must not be mutated once a family exists.
explicit Registry(std::string prefix, std::vector<ConstLabel> globalLabels = {})
        : prefix_(std::move(prefix)), globalLabels_(std::move(globalLabels)) {}

//! Obtain (creating on first use) the group for a subsystem.
MetricGroup& group(const std::string& subsystem)
{
   {std::shared_lock<std::shared_mutex> rd(mutex_);
    auto it = groups_.find(subsystem);
    if (it != groups_.end()) return *it->second;
   }
   std::unique_lock<std::shared_mutex> wr(mutex_);
   auto it = groups_.find(subsystem);
   if (it != groups_.end()) return *it->second;
   auto g = std::unique_ptr<MetricGroup>(new MetricGroup(*this, subsystem));
   auto& ref = *g;
   groups_.emplace(subsystem, std::move(g));
   return ref;
}

const std::string&             prefix()       const noexcept { return prefix_; }
const std::vector<ConstLabel>& globalLabels() const noexcept { return globalLabels_; }

//! Drive a serializer over every group in the registry. Groups are snapshotted
//! under a brief read lock and serialized outside it.
void serialize(ISerializer& s) const
{
   std::vector<const MetricGroup*> snap;
   {std::shared_lock<std::shared_mutex> rd(mutex_);
    snap.reserve(groups_.size());
    for (auto& kv : groups_) snap.push_back(kv.second.get());
   }
   for (auto* g : snap) g->serialize(s);
}

private:
std::string             prefix_;
std::vector<ConstLabel> globalLabels_;

mutable std::shared_mutex mutex_;
std::unordered_map<std::string, std::unique_ptr<MetricGroup>> groups_;
};

/******************************************************************************/
/*        M e t r i c G r o u p   f a c t o r i e s   ( need Registry )       */
/******************************************************************************/

template <class Child>
LabeledFamily<Child>& MetricGroup::add(const std::string& name,
                                       std::vector<std::string> varNames,
                                       std::vector<ConstLabel> constLabels,
                                       std::string help, std::size_t maxKids)
{
   std::string full = joinName(joinName(reg_.prefix(), subsystem_), name);

   if (!validMetricName(full))
      throw std::invalid_argument("XrdMetrics: invalid metric name '" + full + "'");
   for (auto& ln : varNames)
       if (!validLabelName(ln))
          throw std::invalid_argument("XrdMetrics: invalid label name '" + ln + "'");
   for (auto& cl : constLabels)
       if (!validLabelName(cl.first))
          throw std::invalid_argument("XrdMetrics: invalid label name '" + cl.first + "'");

   LabelContext ctx;
   ctx.global      = &reg_.globalLabels();
   ctx.constLabels = std::move(constLabels);
   ctx.schema      = LabelSchema(std::move(varNames));

   auto fam = std::unique_ptr<LabeledFamily<Child>>(
                 new LabeledFamily<Child>(std::move(full), std::move(ctx),
                                          std::move(help), maxKids));
   auto& ref = *fam;
   std::unique_lock<std::shared_mutex> wr(mutex_);
   families_.push_back(std::move(fam));
   return ref;
}

inline LabeledFamily<Counter>&
MetricGroup::counter(const std::string& name, std::vector<std::string> varLabels,
                     std::vector<ConstLabel> constLabels, std::string help,
                     std::size_t maxKids)
{
   return add<Counter>(name, std::move(varLabels), std::move(constLabels),
                       std::move(help), maxKids);
}

inline LabeledFamily<IntGauge>&
MetricGroup::intGauge(const std::string& name, std::vector<std::string> varLabels,
                      std::vector<ConstLabel> constLabels, std::string help,
                      std::size_t maxKids)
{
   return add<IntGauge>(name, std::move(varLabels), std::move(constLabels),
                        std::move(help), maxKids);
}

inline LabeledFamily<FloatGauge>&
MetricGroup::floatGauge(const std::string& name, std::vector<std::string> varLabels,
                        std::vector<ConstLabel> constLabels, std::string help,
                        std::size_t maxKids)
{
   return add<FloatGauge>(name, std::move(varLabels), std::move(constLabels),
                          std::move(help), maxKids);
}

/******************************************************************************/
/*                              D e f a u l t                                */
/******************************************************************************/

//! The process-wide registry shared by the server and all loaded plugins.
//! Prefixed "xrootd"; plugins should register into this so all metrics land in
//! the same scrape.
Registry& Default();
}
#endif
