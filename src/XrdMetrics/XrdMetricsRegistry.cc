/******************************************************************************/
/*                                                                            */
/*                X r d M e t r i c s R e g i s t r y . c c                   */
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

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "XrdMetrics/XrdMetrics.hh"

/******************************************************************************/
/*                       L o c a l   h e l p e r s                            */
/******************************************************************************/

namespace
{
// Format a double as Prometheus expects: integral values without a decimal
// point, infinities/NaN as the canonical tokens, everything else via %g.
//
std::string fmtDouble(double v)
{
   char buff[64];

   if (std::isnan(v)) return "NaN";
   if (std::isinf(v)) return v < 0 ? "-Inf" : "+Inf";

   if (std::floor(v) == v && std::fabs(v) < 1e15)
      {snprintf(buff, sizeof(buff), "%lld", (long long)v);
       return buff;
      }

   snprintf(buff, sizeof(buff), "%.12g", v);
   return buff;
}

// Escape a string for use in a label value or HELP text per the exposition
// format: backslash, double-quote and newline are escaped.
//
std::string escape(const std::string& s, bool quote)
{
   std::string out;
   out.reserve(s.size());
   for (char c : s)
       {     if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (quote && c == '"') out += "\\\"";
        else out += c;
       }
   return out;
}

// Build the canonical key (order-independent) and the pre-formatted label
// string {k="v",...} for a set of labels.
//
void makeLabels(const XrdMetricsLabels& labels,
                std::string& key, std::string& fmt)
{
   if (labels.empty()) {key.clear(); fmt.clear(); return;}

   XrdMetricsLabels sorted(labels);
   std::sort(sorted.begin(), sorted.end());

   key.clear();
   fmt = "{";
   bool first = true;
   for (auto& kv : sorted)
       {key += kv.first; key += '\x1f'; key += kv.second; key += '\x1e';
        if (!first) fmt += ',';
        fmt += kv.first; fmt += "=\""; fmt += escape(kv.second, true); fmt += '"';
        first = false;
       }
   fmt += '}';
}

// Series whose value lives outside the registry and is read on each scrape.
//
class RefCounter : public XrdMetricInstr
{
public:
   void Serialize(std::string& out, const std::string& name,
                  const std::string& labels) const override
       {out += name; out += labels; out += ' ';
        out += std::to_string(fn()); out += '\n';
       }
   explicit RefCounter(XrdMetricsU64Reader r) : fn(std::move(r)) {}
private:
   XrdMetricsU64Reader fn;
};

class RefGauge : public XrdMetricInstr
{
public:
   void Serialize(std::string& out, const std::string& name,
                  const std::string& labels) const override
       {out += name; out += labels; out += ' ';
        out += fmtDouble(fn()); out += '\n';
       }
   explicit RefGauge(XrdMetricsDblReader r) : fn(std::move(r)) {}
private:
   XrdMetricsDblReader fn;
};

const char* typeName(XrdMetricType t)
{
   switch(t)
         {case XrdMetricType::Counter:   return "counter";
          case XrdMetricType::Gauge:     return "gauge";
          case XrdMetricType::Histogram: return "histogram";
         }
   return "untyped";
}

// Insert le="..." into an existing pre-formatted label string.
//
std::string withLE(const std::string& labels, const std::string& le)
{
   std::string lel = "le=\"" + le + "\"";
   if (labels.empty()) return "{" + lel + "}";
   return labels.substr(0, labels.size()-1) + "," + lel + "}";
}
}

/******************************************************************************/
/*               X r d M e t r i c s C o u n t e r                            */
/******************************************************************************/

void XrdMetricsCounter::Serialize(std::string& out, const std::string& name,
                                  const std::string& labels) const
{
   out += name; out += labels; out += ' ';
   out += std::to_string(value()); out += '\n';
}

/******************************************************************************/
/*                 X r d M e t r i c s G a u g e                              */
/******************************************************************************/

void XrdMetricsGauge::inc(double v)
{
   double cur = val.load(std::memory_order_relaxed);
   while(!val.compare_exchange_weak(cur, cur+v, std::memory_order_relaxed)) {}
}

void XrdMetricsGauge::Serialize(std::string& out, const std::string& name,
                                const std::string& labels) const
{
   out += name; out += labels; out += ' ';
   out += fmtDouble(value()); out += '\n';
}

/******************************************************************************/
/*             X r d M e t r i c s H i s t o g r a m                          */
/******************************************************************************/

XrdMetricsHistogram::XrdMetricsHistogram(std::vector<double> bounds)
                    : bound(std::move(bounds)), bcount(bound.size()+1)
{
   std::sort(bound.begin(), bound.end());
}

void XrdMetricsHistogram::observe(double v)
{
   size_t idx = bound.size();                       // default: +Inf bucket
   for (size_t i = 0; i < bound.size(); i++)
       if (v <= bound[i]) {idx = i; break;}

   bcount[idx]++;
   count++;

   double cur = sum.load(std::memory_order_relaxed);
   while(!sum.compare_exchange_weak(cur, cur+v, std::memory_order_relaxed)) {}
}

void XrdMetricsHistogram::Serialize(std::string& out, const std::string& name,
                                    const std::string& labels) const
{
   uint64_t running = 0;
   for (size_t i = 0; i < bound.size(); i++)
       {running += const_cast<RAtomic_uint64_t&>(bcount[i]).load();
        out += name; out += "_bucket"; out += withLE(labels, fmtDouble(bound[i]));
        out += ' '; out += std::to_string(running); out += '\n';
       }

   running += const_cast<RAtomic_uint64_t&>(bcount[bound.size()]).load();
   out += name; out += "_bucket"; out += withLE(labels, "+Inf");
   out += ' '; out += std::to_string(running); out += '\n';

   out += name; out += "_sum"; out += labels; out += ' ';
   out += fmtDouble(sum.load(std::memory_order_relaxed)); out += '\n';

   out += name; out += "_count"; out += labels; out += ' ';
   out += std::to_string(const_cast<RAtomic_uint64_t&>(count).load());
   out += '\n';
}

/******************************************************************************/
/*              X r d M e t r i c s R e g i s t r y                           */
/******************************************************************************/

XrdMetricsRegistry& XrdMetricsRegistry::Default()
{
   static XrdMetricsRegistry instance;
   return instance;
}

XrdMetricInstr* XrdMetricsRegistry::GetOrAdd(const std::string& name,
                                             const std::string& help,
                                             XrdMetricType type,
                                             const XrdMetricsLabels& labels,
                                             const std::vector<double>* bounds)
{
   std::string key, fmt;
   makeLabels(labels, key, fmt);

   std::lock_guard<std::mutex> guard(regMtx);

   auto& fam = family[name];
   if (fam.series.empty()) {fam.type = type; fam.help = help;}

   auto it = fam.series.find(key);
   if (it != fam.series.end()) return it->second.instr.get();

   std::unique_ptr<XrdMetricInstr> inst;
   switch(type)
         {case XrdMetricType::Counter:
               inst.reset(new XrdMetricsCounter()); break;
          case XrdMetricType::Gauge:
               inst.reset(new XrdMetricsGauge()); break;
          case XrdMetricType::Histogram:
               inst.reset(new XrdMetricsHistogram(bounds ? *bounds
                                                         : std::vector<double>()));
               break;
         }

   XrdMetricInstr* ret = inst.get();
   fam.series[key] = Series{fmt, std::move(inst)};
   return ret;
}

XrdMetricsCounter& XrdMetricsRegistry::Counter(const std::string& name,
                                               const std::string& help,
                                               const XrdMetricsLabels& labels)
{
   return *static_cast<XrdMetricsCounter*>
          (GetOrAdd(name, help, XrdMetricType::Counter, labels, nullptr));
}

XrdMetricsGauge& XrdMetricsRegistry::Gauge(const std::string& name,
                                           const std::string& help,
                                           const XrdMetricsLabels& labels)
{
   return *static_cast<XrdMetricsGauge*>
          (GetOrAdd(name, help, XrdMetricType::Gauge, labels, nullptr));
}

XrdMetricsHistogram& XrdMetricsRegistry::Histogram(const std::string& name,
                                                   const std::string& help,
                                                   const std::vector<double>& bounds,
                                                   const XrdMetricsLabels& labels)
{
   return *static_cast<XrdMetricsHistogram*>
          (GetOrAdd(name, help, XrdMetricType::Histogram, labels, &bounds));
}

void XrdMetricsRegistry::AddCollector(XrdMetricsCollector c)
{
   std::lock_guard<std::mutex> guard(regMtx);
   collectors.push_back(std::move(c));
}

void XrdMetricsRegistry::AddRef(const std::string& name, const std::string& help,
                               XrdMetricType type, const XrdMetricsLabels& labels,
                               std::unique_ptr<XrdMetricInstr> inst)
{
   std::string key, fmt;
   makeLabels(labels, key, fmt);

   std::lock_guard<std::mutex> guard(regMtx);

   auto& fam = family[name];
   if (fam.series.empty()) {fam.type = type; fam.help = help;}

   if (fam.series.find(key) != fam.series.end()) return;  // idempotent
   fam.series[key] = Series{fmt, std::move(inst)};
}

void XrdMetricsRegistry::AddRefCounter(const std::string& name,
                                       const std::string& help,
                                       const XrdMetricsLabels& labels,
                                       XrdMetricsU64Reader reader)
{
   AddRef(name, help, XrdMetricType::Counter, labels,
          std::unique_ptr<XrdMetricInstr>(new RefCounter(std::move(reader))));
}

void XrdMetricsRegistry::AddRefGauge(const std::string& name,
                                     const std::string& help,
                                     const XrdMetricsLabels& labels,
                                     XrdMetricsDblReader reader)
{
   AddRef(name, help, XrdMetricType::Gauge, labels,
          std::unique_ptr<XrdMetricInstr>(new RefGauge(std::move(reader))));
}

int XrdMetricsRegistry::Scrape(std::string& out)
{
   out.clear();

   std::vector<XrdMetricsCollector> cols;

   {std::lock_guard<std::mutex> guard(regMtx);

    for (auto& fe : family)
        {const std::string& name = fe.first;
         Family&            fam  = fe.second;

         out += "# HELP "; out += name; out += ' ';
         out += escape(fam.help, false); out += '\n';
         out += "# TYPE "; out += name; out += ' ';
         out += typeName(fam.type); out += '\n';

         for (auto& se : fam.series)
             se.second.instr->Serialize(out, name, se.second.labelStr);
        }
    cols = collectors;
   }

// Collectors are run without holding regMtx so they may take their own locks.
//
   for (auto& c : cols) c(out);

   return (int)out.size();
}
