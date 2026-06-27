target_sources(XrdUtils
  PRIVATE
    # Phase-1 prototype (consumed by the server and xrdmoncollect; deprecated).
    XrdMetrics.cc  XrdMetrics.hh
    # Phase-2 next-generation registry (Registry/MetricGroup/Family/series).
    XrdMetricsSerialize.cc
    XrdMetricsValue.hh       XrdMetricsLabels.hh
    XrdMetricsInstrument.hh  XrdMetricsSerializer.hh
    XrdMetricsFamily.hh      XrdMetricsRegistry.hh
)
