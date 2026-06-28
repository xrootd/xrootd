target_sources(XrdUtils
  PRIVATE
    XrdMetricsSerialize.cc   XrdMetricsConfig.cc
    XrdMetricsValue.hh       XrdMetricsLabels.hh
    XrdMetricsInstrument.hh  XrdMetricsSerializer.hh
    XrdMetricsFamily.hh      XrdMetricsRegistry.hh
    XrdMetricsConfig.hh
)
