/******************************************************************************/
/*                                                                            */
/*               X r d M e t r i c s S e r i a l i z e . c c                  */
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

#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>

#ifndef HAVE_FLOAT_TO_CHARS
#include <locale.h>
#endif

#include "XrdMetrics/XrdMetricsLabels.hh"
#include "XrdMetrics/XrdMetricsRegistry.hh"
#include "XrdMetrics/XrdMetricsSerializer.hh"
#include "XrdMetrics/XrdMetricsValue.hh"

namespace XrdMetrics
{
/******************************************************************************/
/*                       L o c a l   h e l p e r s                           */
/******************************************************************************/

namespace
{
// Escape a label value per the exposition format: backslash, double-quote and
// newline.
//
void appendEscaped(std::string& out, const std::string& val)
{
   for (char c : val)
       switch (c)
             {case '\\': out += "\\\\"; break;
              case '"':  out += "\\\""; break;
              case '\n': out += "\\n";  break;
              default:   out.push_back(c);
             }
}

// Escape HELP text: only backslash and newline are special there.
//
void appendHelpEscaped(std::string& out, const std::string& help)
{
   for (char c : help)
       {     if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else                out.push_back(c);
       }
}

bool nameFirst(char c)
{
   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool nameRest(char c)
{
   return nameFirst(c) || (c >= '0' && c <= '9');
}

// Escape a string per RFC 8259 (JSON): the seven short escapes plus \u00XX for
// the remaining control characters.
//
void appendJsonEscaped(std::string& out, const std::string& s)
{
   for (char c : s)
       switch (c)
             {case '"':  out += "\\\""; break;
              case '\\': out += "\\\\"; break;
              case '\n': out += "\\n";  break;
              case '\r': out += "\\r";  break;
              case '\t': out += "\\t";  break;
              case '\b': out += "\\b";  break;
              case '\f': out += "\\f";  break;
              default:
                 if (static_cast<unsigned char>(c) < 0x20)
                    {char u[7];
                     std::snprintf(u, sizeof(u), "\\u%04x", (unsigned)(unsigned char)c);
                     out += u;
                    }
                 else out.push_back(c);
             }
}

// Append a double as a JSON value. Finite values render as a bare JSON number;
// the non-finite values, which JSON cannot express, use the proto3-JSON tokens
// ("NaN"/"Infinity"/"-Infinity") that OTLP consumers accept for double fields.
//
void appendJsonNumber(std::string& out, double v)
{
   if (std::isnan(v)) {out += "\"NaN\"";                          return;}
   if (std::isinf(v)) {out += (v < 0 ? "\"-Infinity\"" : "\"Infinity\""); return;}
   appendValue(out, v);
}

// One OTLP KeyValue with a string value: {"key":"k","value":{"stringValue":"v"}}
//
void appendOtelAttr(std::string& out, const std::string& k, const std::string& v)
{
   out += "{\"key\":\"";
   appendJsonEscaped(out, k);
   out += "\",\"value\":{\"stringValue\":\"";
   appendJsonEscaped(out, v);
   out += "\"}}";
}
}

/******************************************************************************/
/*                           a p p e n d V a l u e                           */
/******************************************************************************/

void appendValue(std::string& out, std::uint64_t v)
{
   char b[20];
   auto r = std::to_chars(b, b + sizeof(b), v);
   out.append(b, r.ptr);
}

void appendValue(std::string& out, std::int64_t v)
{
   char b[21];
   auto r = std::to_chars(b, b + sizeof(b), v);
   out.append(b, r.ptr);
}

void appendValue(std::string& out, double v)
{
   if (std::isnan(v)) {out += "NaN"; return;}
   if (std::isinf(v)) {out += (v < 0 ? "-Inf" : "+Inf"); return;}

#ifdef HAVE_FLOAT_TO_CHARS
   char b[32];
   auto r = std::to_chars(b, b + sizeof(b), v);
   out.append(b, r.ptr);
#else
// Integer-valued doubles avoid __printf_fp entirely on both paths and render
// exactly (counters and integer-valued gauges are the common case).
//
   double ip;
   if (std::modf(v, &ip) == 0.0 && std::fabs(v) < 9.0e15)
      {char b[24];
       auto r = std::to_chars(b, b + sizeof(b), static_cast<long long>(v));
       out.append(b, r.ptr);
       return;
      }

// %.17g always round-trips. Guard LC_NUMERIC so a comma-decimal locale cannot
// produce invalid exposition output.
//
   char b[32];
   int n;
   locale_t cloc = newlocale(LC_NUMERIC_MASK, "C", static_cast<locale_t>(0));
   if (cloc)
      {locale_t prev = uselocale(cloc);
       n = std::snprintf(b, sizeof(b), "%.17g", v);
       uselocale(prev);
       freelocale(cloc);
      }
   else n = std::snprintf(b, sizeof(b), "%.17g", v);
   if (n > 0) out.append(b, static_cast<std::size_t>(n));
#endif
}

/******************************************************************************/
/*                            S e r i e s L a b e l s                        */
/******************************************************************************/

SeriesLabels::SeriesLabels(const LabelContext& ctx, const std::string& metricName,
                           LabelValues vals)
            : ctx_(&ctx), values_(std::move(vals))
{
   buildPrefix(metricName);
}

void SeriesLabels::buildPrefix(const std::string& metricName)
{
   prefix_.reserve(metricName.size() + 64);
   prefix_ += metricName;

   bool any = false;
   auto emit = [&](const std::string& k, const std::string& v)
              {prefix_.push_back(any ? ',' : '{');
               any = true;
               prefix_ += k;
               prefix_ += "=\"";
               appendEscaped(prefix_, v);
               prefix_.push_back('"');
              };

   if (ctx_->global) for (auto& kv : *ctx_->global) emit(kv.first, kv.second);
   for (auto& kv : ctx_->constLabels)               emit(kv.first, kv.second);

   const auto& n = ctx_->schema.names();
   for (std::size_t i = 0; i < n.size(); ++i)
       emit(n[i], i < values_.v.size() ? values_.v[i] : std::string());

   if (any) prefix_.push_back('}');
   prefix_.push_back(' ');             // the value is appended right after this
}

/******************************************************************************/
/*                              H e l p e r s                                */
/******************************************************************************/

std::string joinName(const std::string& a, const std::string& b)
{
   if (a.empty()) return b;
   if (b.empty()) return a;
   std::string r;
   r.reserve(a.size() + 1 + b.size());
   r.append(a);
   r.push_back('_');
   r.append(b);
   return r;
}

bool validMetricName(const std::string& name)
{
   if (name.empty()) return false;
   if (!(nameFirst(name[0]) || name[0] == ':')) return false;
   for (std::size_t i = 1; i < name.size(); ++i)
       if (!(nameRest(name[i]) || name[i] == ':')) return false;
   return true;
}

bool validLabelName(const std::string& name)
{
   if (name.empty()) return false;
   if (!nameFirst(name[0])) return false;
   for (std::size_t i = 1; i < name.size(); ++i)
       if (!nameRest(name[i])) return false;
   return true;
}

/******************************************************************************/
/*              P r o m e t h e u s T e x t S e r i a l i z e r              */
/******************************************************************************/

void PrometheusTextSerializer::beginFamily(const std::string& name,
                                           MetricKind kind,
                                           const std::string& help)
{
   if (!help.empty())
      {out_ += "# HELP ";
       out_ += name;
       out_.push_back(' ');
       appendHelpEscaped(out_, help);
       out_.push_back('\n');
      }
   out_ += "# TYPE ";
   out_ += name;
   out_.push_back(' ');
   switch (kind)
         {case MetricKind::Counter:   out_ += "counter";   break;
          case MetricKind::Histogram: out_ += "histogram"; break;
          default:                    out_ += "gauge";     break;
         }
   out_.push_back('\n');
}

template <class T>
void PrometheusTextSerializer::emit(const SeriesLabels& labels, T value)
{
   out_.append(labels.prometheusPrefix());   // cached, pure memcpy
   appendValue(out_, value);
   out_.push_back('\n');
}

void PrometheusTextSerializer::series(const SeriesLabels& l, std::uint64_t v)
{
   emit(l, v);
}

void PrometheusTextSerializer::series(const SeriesLabels& l, std::int64_t v)
{
   emit(l, v);
}

void PrometheusTextSerializer::series(const SeriesLabels& l, double v)
{
   emit(l, v);
}

void PrometheusTextSerializer::histogram(const std::string& fullName,
                                         const SeriesLabels& l,
                                         const HistogramData& d)
{
// The cached prefix is "fullName{labels} " (or "fullName " with no labels);
// strip the name and trailing space to get the "{labels}" portion (or empty),
// then splice in the le label for the per-bucket lines.
//
   const std::string& pfx = l.prometheusPrefix();
   std::string lbls(pfx, fullName.size(), pfx.size() - fullName.size() - 1);

   auto bucket = [&](const std::string& le, std::uint64_t cum)
                {out_ += fullName; out_ += "_bucket";
                 if (lbls.empty()) {out_ += "{le=\""; out_ += le; out_ += "\"}";}
                 else {out_.append(lbls, 0, lbls.size() - 1);
                       out_ += ",le=\""; out_ += le; out_ += "\"}";}
                 out_.push_back(' '); appendValue(out_, cum); out_.push_back('\n');
                };

   std::string le;
   for (std::size_t i = 0; i < d.bounds.size(); ++i)
       {le.clear(); appendValue(le, d.bounds[i]); bucket(le, d.cumulative[i]);}
   bucket("+Inf", d.cumulative.back());

   out_ += fullName; out_ += "_sum"; out_ += lbls;
   out_.push_back(' '); appendValue(out_, d.sum); out_.push_back('\n');
   out_ += fullName; out_ += "_count"; out_ += lbls;
   out_.push_back(' '); appendValue(out_, d.cumulative.back()); out_.push_back('\n');
}

/******************************************************************************/
/*                   O t e l J s o n S e r i a l i z e r                     */
/******************************************************************************/

void OtelJsonSerializer::begin()
{
// Capture one scrape timestamp (nanoseconds since the Unix epoch) for every
// data point in this document.
//
   auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
   ts_.clear();
   appendValue(ts_, static_cast<std::uint64_t>(ns));

   firstMetric_ = true;

   out_ += "{\"resourceMetrics\":[{\"resource\":{";
   if (!resource_.empty())
      {out_ += "\"attributes\":[";
       bool first = true;
       for (auto& kv : resource_)
           {if (!first) out_.push_back(',');
            first = false;
            appendOtelAttr(out_, kv.first, kv.second);
           }
       out_ += "]";
      }
   out_ += "},\"scopeMetrics\":[{\"scope\":{\"name\":\"";
   appendJsonEscaped(out_, scope_);
   out_ += "\"},\"metrics\":[";
}

void OtelJsonSerializer::end()
{
   out_ += "]}]}]}";   // metrics, scopeMetrics(obj), scopeMetrics(arr), RM, RMs
}

void OtelJsonSerializer::beginFamily(const std::string& name, MetricKind kind,
                                     const std::string& help)
{
   if (!firstMetric_) out_.push_back(',');
   firstMetric_ = false;
   kind_        = kind;
   firstPoint_  = true;

   out_ += "{\"name\":\"";
   appendJsonEscaped(out_, name);
   out_ += "\"";
   if (!help.empty())
      {out_ += ",\"description\":\"";
       appendJsonEscaped(out_, help);
       out_ += "\"";
      }

   switch (kind)
         {case MetricKind::Counter:
             out_ += ",\"sum\":{\"aggregationTemporality\":2,"
                     "\"isMonotonic\":true,\"dataPoints\":[";
             break;
          case MetricKind::Histogram:
             out_ += ",\"histogram\":{\"aggregationTemporality\":2,"
                     "\"dataPoints\":[";
             break;
          default:
             out_ += ",\"gauge\":{\"dataPoints\":[";
             break;
         }
}

void OtelJsonSerializer::endFamily()
{
   out_ += "]}}";   // dataPoints array, data field object, metric object
}

void OtelJsonSerializer::beginPoint(const SeriesLabels& labels)
{
   if (!firstPoint_) out_.push_back(',');
   firstPoint_ = false;

   out_ += "{\"attributes\":[";
   bool first = true;
   labels.forEachLabel([&](const std::string& k, const std::string& v)
                      {if (!first) out_.push_back(',');
                       first = false;
                       appendOtelAttr(out_, k, v);
                      });
   out_ += "]";
}

void OtelJsonSerializer::series(const SeriesLabels& l, std::uint64_t v)
{
   beginPoint(l);
   out_ += ",\"asInt\":\"";
   appendValue(out_, v);
   out_ += "\",\"timeUnixNano\":\"";
   out_ += ts_;
   out_ += "\"}";
}

void OtelJsonSerializer::series(const SeriesLabels& l, std::int64_t v)
{
   beginPoint(l);
   out_ += ",\"asInt\":\"";
   appendValue(out_, v);
   out_ += "\",\"timeUnixNano\":\"";
   out_ += ts_;
   out_ += "\"}";
}

void OtelJsonSerializer::series(const SeriesLabels& l, double v)
{
   beginPoint(l);
   out_ += ",\"asDouble\":";
   appendJsonNumber(out_, v);
   out_ += ",\"timeUnixNano\":\"";
   out_ += ts_;
   out_ += "\"}";
}

void OtelJsonSerializer::histogram(const std::string& /*fullName*/,
                                   const SeriesLabels& l, const HistogramData& d)
{
   beginPoint(l);

   out_ += ",\"count\":\"";
   appendValue(out_, d.cumulative.back());
   out_ += "\",\"sum\":";
   appendJsonNumber(out_, d.sum);

// OTLP bucketCounts are per-bucket, not cumulative; de-cumulate as we go. There
// are bounds.size()+1 of them (the trailing implicit +Inf bucket).
//
   out_ += ",\"bucketCounts\":[";
   std::uint64_t prev = 0;
   for (std::size_t i = 0; i < d.cumulative.size(); ++i)
       {if (i) out_.push_back(',');
        out_.push_back('"');
        appendValue(out_, d.cumulative[i] - prev);
        out_.push_back('"');
        prev = d.cumulative[i];
       }
   out_ += "],\"explicitBounds\":[";
   for (std::size_t i = 0; i < d.bounds.size(); ++i)
       {if (i) out_.push_back(',');
        appendJsonNumber(out_, d.bounds[i]);
       }
   out_ += "],\"timeUnixNano\":\"";
   out_ += ts_;
   out_ += "\"}";
}

/******************************************************************************/
/*                              D e f a u l t                                */
/******************************************************************************/

Registry& Default()
{
   static Registry instance("xrootd");
   return instance;
}
}
