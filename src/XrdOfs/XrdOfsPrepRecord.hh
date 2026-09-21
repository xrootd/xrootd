#ifndef XRD_OFS_PREP_RECORD_HH
#define XRD_OFS_PREP_RECORD_HH
// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
// Private codec for the existing file format and coordinator's wire model.
#include "XrdOfsPrepStorage.hh"
#include "XrdOfsPrepProtocol.hh"
namespace XrdOfsPrep {
void ValidateOwner(const Owner &owner);
// Semantic recovery boundary, also applied to records returned by custom stores.
void ValidateRecord(const Record &record);
Record DecodeRecord(const XrdOfsPrepProtocol::Json &json);
XrdOfsPrepProtocol::Json EncodeRecord(const Record &record);
}
#endif
